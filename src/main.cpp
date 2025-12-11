#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Preferences.h> // LIBRARY UNTUK MENYIMPAN DATA

// ==========================================
//              KONFIGURASI PIN
// ==========================================
#define PIN_OLED_SDA    22
#define PIN_OLED_SCL    21
#define PIN_ROT_CLK     34  
#define PIN_ROT_DT      35
#define PIN_ROT_SW      25  
#define PIN_T12_SW      33  
#define PIN_PWM         18  
#define PIN_DS18        23  

// ==========================================
//          SETTINGS & VARIABLES
// ==========================================
struct SystemSettings {
  int pwm;          
  int freq;         
  int loopDelay;    
};

// Default awal (akan ditimpa oleh data tersimpan nanti)
volatile SystemSettings settings = {30, 2000, 50}; 

// Objek Penyimpanan
Preferences preferences;
volatile bool settingsChanged = false;
volatile unsigned long lastChangeTime = 0;

volatile int currentMenu = 0; // 0=PWM, 1=FREQ, 2=DELAY
volatile bool isMaintenance = false;
volatile float handleTempDS = 30.0;
String statusHandle = "SLEEP";

// Variabel Fisika & Boost
volatile float virtualTemp = 30.0;
volatile int appliedPWM = 0;
unsigned long boostEndTime = 0;
int boostPWMVal = 0;
unsigned long sleepStartTime = 0;
const int TIME_COLD_LIMIT = 300000; // 5 Menit (Sesuai kode kamu)

// Variabel Input
volatile unsigned long lastEncoderTime = 0; 

// === KALIBRASI TOUCH (Sesuai Request) ===
const int touchThreshold = 5; 
unsigned long lastTouchDetectedTime = 0;
const int TOUCH_HOLD_DELAY = 1000; 

// ==========================================
//          OBJEK
// ==========================================
Adafruit_SSD1306 display(128, 64, &Wire, -1);
OneWire oneWire(PIN_DS18);
DallasTemperature sensors(&oneWire);

const int resolution = 8;
const int ledChannel = 0;

// ==========================================
//          TASK 1: INPUT 
// ==========================================
void TaskInput(void *pvParameters) {
  int lastClk = digitalRead(PIN_ROT_CLK);
  
  int lastBtnState = HIGH;
  unsigned long btnPressTime = 0;
  unsigned long lastClickTime = 0;
  bool longPressHandled = false;

  for (;;) { 
    // --- 1. ENCODER PUTAR ---
    int newClk = digitalRead(PIN_ROT_CLK);
    if (newClk != lastClk && newClk == LOW) {
      if (digitalRead(PIN_ROT_DT) == HIGH) {
        unsigned long dt = millis() - lastEncoderTime;
        lastEncoderTime = millis();
        int step = 1;
        if (dt < 30) step = 10;  
        
        if (currentMenu == 0) settings.pwm += (step > 5 ? 5 : 1); 
        else if (currentMenu == 1) settings.freq += (step > 1 ? 100 : 10);
        else settings.loopDelay += (step > 1 ? 10 : 1);

      } else {
        unsigned long dt = millis() - lastEncoderTime;
        lastEncoderTime = millis();
        int step = 1;
        if (dt < 30) step = 10;

        if (currentMenu == 0) settings.pwm -= (step > 5 ? 5 : 1);
        else if (currentMenu == 1) settings.freq -= (step > 1 ? 100 : 10);
        else settings.loopDelay -= (step > 1 ? 10 : 1);
      }

      // Clamp Values
      if (settings.pwm > 255) settings.pwm = 255;
      if (settings.pwm < 0) settings.pwm = 0;
      if (settings.freq > 50000) settings.freq = 50000;
      if (settings.freq < 100) settings.freq = 100;
      if (settings.loopDelay > 1000) settings.loopDelay = 1000;
      if (settings.loopDelay < 10) settings.loopDelay = 10;

      // TANDAI BAHWA ADA PERUBAHAN (UNTUK AUTO-SAVE)
      settingsChanged = true;
      lastChangeTime = millis();
    }
    lastClk = newClk;


    // --- 2. TOMBOL ---
    int btnRead = digitalRead(PIN_ROT_SW);
    if (btnRead == LOW && lastBtnState == HIGH) {
      btnPressTime = millis();
      longPressHandled = false;
    }

    if (btnRead == LOW && !longPressHandled) {
      if (millis() - btnPressTime > 2000) { 
        isMaintenance = !isMaintenance;
        longPressHandled = true; 
      }
    }

    if (btnRead == HIGH && lastBtnState == LOW) {
      if (!longPressHandled) {
        if (millis() - lastClickTime < 400) {
          currentMenu++;
          if (currentMenu > 2) currentMenu = 0;
        } 
        lastClickTime = millis();
      }
    }
    lastBtnState = btnRead;

    // --- 3. TOUCH SENSOR ---
    int touchValue = touchRead(PIN_T12_SW);
    if (touchValue < touchThreshold) {
       lastTouchDetectedTime = millis(); 
    }
    
    if (!isMaintenance) {
      if (millis() - lastTouchDetectedTime < TOUCH_HOLD_DELAY) {
         statusHandle = "AKTIF";
      } else {
         statusHandle = "SLEEP";
      }
    } else {
      statusHandle = "MAINTENANCE";
    }

    // --- 4. LOGIKA AUTO-SAVE (SMART SAVE) ---
    // Simpan hanya jika ada perubahan DAN sudah diam selama 2 detik
    if (settingsChanged && (millis() - lastChangeTime > 2000)) {
       preferences.putInt("pwm", settings.pwm);
       preferences.putInt("freq", settings.freq);
       preferences.putInt("delay", settings.loopDelay);
       
       settingsChanged = false; // Reset flag
       // Serial.println("Settings Saved!"); // Debug only
    }

    vTaskDelay(5 / portTICK_PERIOD_MS); 
  }
}

// ==========================================
//          TASK 2: HEATER & LOGIC
// ==========================================
void TaskHeater(void *pvParameters) {
  
  bool lastActive = false;
  int currentFreq = 2000; 

  for (;;) {
    
    if (settings.freq != currentFreq) {
      ledcSetup(ledChannel, settings.freq, resolution);
      ledcAttachPin(PIN_PWM, ledChannel);
      currentFreq = settings.freq;
    }

    if (isMaintenance) {
      appliedPWM = 0;
      ledcWrite(ledChannel, 0);
      virtualTemp = handleTempDS; 
      vTaskDelay(100 / portTICK_PERIOD_MS);
      continue; 
    }

    float targetVirtual = settings.pwm * 4.0; 
    if (targetVirtual < handleTempDS) targetVirtual = handleTempDS;

    bool isActive = (statusHandle == "AKTIF");

    if (isActive) {
      if (!lastActive) {
        unsigned long sleepDuration = millis() - sleepStartTime;
        if (sleepDuration > TIME_COLD_LIMIT) {
          boostEndTime = millis() + 2000;
          boostPWMVal = settings.pwm * 3; 
        } else {
          float gap = targetVirtual - virtualTemp;
          if (gap < 0) gap = 0;
          long duration = map((long)gap, 0, 300, 0, 4000);
          boostEndTime = millis() + duration;
          boostPWMVal = settings.pwm * 2.5; 
        }
        if (boostPWMVal > 255) boostPWMVal = 255;
      }

      if (virtualTemp < targetVirtual) {
        virtualTemp += (targetVirtual - virtualTemp) * 0.15; 
      }

      if (millis() < boostEndTime) appliedPWM = boostPWMVal;
      else appliedPWM = settings.pwm;
      
      ledcWrite(ledChannel, appliedPWM);

    } else {
      if (lastActive) sleepStartTime = millis();
      if (virtualTemp > handleTempDS) {
        virtualTemp -= (virtualTemp - handleTempDS) * 0.025; 
      }
      appliedPWM = 0;
      ledcWrite(ledChannel, 0);
    }
    
    lastActive = isActive;
    vTaskDelay(settings.loopDelay / portTICK_PERIOD_MS);
  }
}

// ==========================================
//          TASK 3: DISPLAY (CLEAN UI + SAVE INDICATOR)
// ==========================================
void TaskDisplay(void *pvParameters) {
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) for(;;);
  display.clearDisplay();
  display.setTextColor(WHITE);

  for (;;) {
    sensors.requestTemperatures();
    float t = sensors.getTempCByIndex(0);
    if(t > -50 && t < 120) handleTempDS = t;

    display.clearDisplay();

    if (isMaintenance) {
      display.fillRect(0, 0, 128, 16, WHITE); 
      display.setTextColor(BLACK);
      display.setTextSize(1);
      display.setCursor(30, 4);
      display.print("MAINTENANCE");
      
      display.setTextColor(WHITE);
      display.setTextSize(2);
      display.setCursor(40, 30);
      display.print("SAFE"); 
      
      display.setTextSize(1);
      display.setCursor(20, 52);
      display.print("HEATER DISABLED");

    } else {
      // 1. HEADER BAR
      display.setTextSize(1);
      display.setCursor(0, 0);
      
      if (statusHandle == "AKTIF") {
         if (millis() < boostEndTime) display.print("BOOSTING!");
         else display.print("HEATING");
      } else {
         display.print("SLEEPING");
      }
      
      display.setCursor(95, 0);
      display.print((int)handleTempDS); display.print("C");

      display.drawLine(0, 10, 128, 10, WHITE); 

      // 2. MENU KIRI
      int yStart = 15;
      int yGap = 13;
      int labelX = 12; 
      
      display.setTextSize(1);
      int cursorY = yStart + (currentMenu * yGap);
      display.setCursor(0, cursorY); 
      display.print(">"); 

      display.setCursor(labelX, yStart);      
      display.print("PWM");
      display.setCursor(labelX, yStart+yGap); 
      display.print("Hz");
      display.setCursor(labelX, yStart+yGap*2); 
      display.print("Dly");

      // 3. NILAI KANAN
      display.setCursor(50, 20);
      
      if (currentMenu == 0) {
         display.setTextSize(3); 
         display.print(settings.pwm);
         display.setTextSize(1);
         display.setCursor(50, 45); display.print("Power");
      } 
      else if (currentMenu == 1) {
         if(settings.freq > 9999) display.setTextSize(2); else display.setTextSize(3);
         display.print(settings.freq);
         display.setTextSize(1);
         display.setCursor(50, 45); display.print("Freq");
      } 
      else {
         display.setTextSize(3);
         display.print(settings.loopDelay);
         display.setTextSize(1);
         display.setCursor(50, 45); display.print("Delay");
      }
      
      // INDIKATOR BELUM DISIMPAN (Tanda Bintang kecil)
      if (settingsChanged) {
         display.setTextSize(1);
         display.setCursor(120, 15);
         display.print("*");
      }

      // 4. FOOTER INFO
      display.setTextSize(1);
      display.setCursor(80, 55); 
      
      if (statusHandle == "SLEEP") {
          unsigned long elapsed = millis() - sleepStartTime;
          if (elapsed < TIME_COLD_LIMIT) {
             int remaining = (TIME_COLD_LIMIT - elapsed) / 1000;
             if(remaining < 10) display.print("0"); 
             display.print(remaining); display.print("s");
          } else {
             display.print("COLD");
          }
      } else {
          display.print("P:"); display.print(appliedPWM);
      }
    }

    display.display();
    vTaskDelay(100 / portTICK_PERIOD_MS); 
  }
}

// ==========================================
//          SETUP
// ==========================================
void setup() {
  Serial.begin(115200);

  // 1. BUKA MEMORI PENYIMPANAN
  preferences.begin("t12_config", false); 
  
  // 2. LOAD SETTINGAN (Jika belum ada, pakai default 30, 2000, 50)
  settings.pwm = preferences.getInt("pwm", 30);
  settings.freq = preferences.getInt("freq", 2000);
  settings.loopDelay = preferences.getInt("delay", 50);

  pinMode(PIN_ROT_CLK, INPUT); 
  pinMode(PIN_ROT_DT, INPUT);
  pinMode(PIN_ROT_SW, INPUT_PULLUP);
  
  touchSetCycles(0x3000, 0x3000); 

  ledcSetup(ledChannel, settings.freq, resolution);
  ledcAttachPin(PIN_PWM, ledChannel);
  ledcWrite(ledChannel, 0);

  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  sensors.begin();

  xTaskCreate(TaskInput, "In", 4096, NULL, 2, NULL);
  xTaskCreate(TaskHeater, "Heat", 4096, NULL, 2, NULL); 
  xTaskCreate(TaskDisplay, "Disp", 4096, NULL, 1, NULL);
}

void loop() {
  vTaskDelete(NULL); 
}