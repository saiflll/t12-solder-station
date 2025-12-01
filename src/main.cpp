#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RotaryEncoder.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Preferences.h>

// ================= PINS (LOCKED) =================
#define PIN_OLED_SDA    22
#define PIN_OLED_SCL    21
#define PIN_ROT_CLK     34
#define PIN_ROT_DT      35
#define PIN_ROT_SW      25
#define PIN_LM358       26  
#define PIN_T12_SW      27  
#define PIN_PWM         18  
#define PIN_DS18        23  

// ================= CONFIG =================
#define MAX_TEMP        450
#define MIN_TEMP        150
#define SLEEP_TEMP      150
#define TIME_TO_SLEEP   60000   
#define TIME_TO_AOD     15000   
#define LONG_PRESS_MS   600 

#define REVERSE_ROTARY  false   

// ================= BITMAPS =================
const unsigned char PROGMEM icon_fire[] = { 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x03, 0x80, 0x03, 0xe0, 0x07, 0xf0, 0x07, 0xf8, 0x0f, 0xdc, 0x0f, 0xfe, 0x1f, 0xbf, 0x3f, 0xdf, 0x3f, 0xef, 0x7f, 0xf6, 0x7f, 0xf2, 0x3f, 0xe0, 0x1f, 0xc0 };
const unsigned char PROGMEM icon_snow[] = { 0x01, 0x80, 0x01, 0x80, 0x09, 0x90, 0x19, 0x98, 0x31, 0x8c, 0x71, 0x8e, 0x60, 0x06, 0xc3, 0xc3, 0xc3, 0xc3, 0x60, 0x06, 0x71, 0x8e, 0x31, 0x8c, 0x19, 0x98, 0x09, 0x90, 0x01, 0x80, 0x01, 0x80 };
const unsigned char PROGMEM icon_hand[] = { 0x00, 0x00, 0x06, 0x00, 0x0f, 0x00, 0x0f, 0x80, 0x0f, 0xc0, 0x0f, 0xe0, 0x0f, 0xf0, 0x0f, 0xf0, 0x1f, 0xf0, 0x3f, 0xf0, 0x7f, 0xf0, 0x7f, 0xe0, 0x7f, 0xc0, 0x3f, 0x80, 0x1f, 0x00, 0x0e, 0x00 };
const unsigned char PROGMEM icon_gear[] = { 0x00, 0x00, 0x03, 0xc0, 0x03, 0xc0, 0x04, 0x20, 0x18, 0x18, 0x21, 0x84, 0x43, 0xc2, 0x43, 0xc2, 0x43, 0xc2, 0x43, 0xc2, 0x21, 0x84, 0x18, 0x18, 0x04, 0x20, 0x03, 0xc0, 0x03, 0xc0, 0x00, 0x00 };
const unsigned char PROGMEM ghost_bmp[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x70, 0x00, 0x00, 0x01, 0xc0, 0x0f, 0xff, 0x00, 0x70, 0xc0, 0x00, 0x03, 0xc0, 0x1f, 0xff, 0x80, 0x71, 0xc0, 0x00, 0xc3, 0x80, 0x7f, 0xff, 0xe0, 0x71, 0xc0, 0x03, 0xe0, 0x00, 0xf8, 0x01, 0xe0, 0x07, 0xf0, 0x03, 0xf0, 0x0f, 0x80, 0x00, 0x7e, 0x07, 0xf0, 0x03, 0xf0, 0x0f, 0x80, 0x00, 0x1e, 0x07, 0xf0, 0x03, 0xf0, 0x3c, 0x00, 0x00, 0x0f, 0x01, 0xc0, 0x00, 0xe0, 0x78, 0x00, 0x00, 0x07, 0x81, 0xc0, 0x00, 0xc0, 0x78, 0x00, 0x00, 0x07, 0x80, 0x00, 0x00, 0xc0, 0xc0, 0x00, 0x00, 0x01, 0xe0, 0x00, 0x00, 0xc0, 0xc0, 0x00, 0x00, 0x00, 0xe0, 0x00, 0x00, 0x41, 0x80, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x07, 0x0f, 0x00, 0x03, 0x80, 0x18, 0x00, 0x00, 0x1f, 0x1f, 0x00, 0x03, 0xc0, 0x1e, 0x00, 0x00, 0x1f, 0x33, 0x80, 0x0c, 0xe0, 0x1e, 0x00, 0x00, 0x1c, 0x3f, 0x80, 0x0f, 0xe0, 0x1e, 0x00, 0x00, 0x1c, 0x3f, 0x80, 0x0f, 0xe0, 0x06, 0x00, 0x00, 0x1c, 0x3f, 0x80, 0x0f, 0xe0, 0x06, 0x00, 0x00, 0x1c, 0x3f, 0x80, 0x07, 0xe0, 0x06, 0x00, 0x00, 0x1c, 0x0e, 0x7f, 0xc3, 0x80, 0x07, 0x80, 0x00, 0x1c, 0x0e, 0x7f, 0xc3, 0x80, 0x07, 0x80, 0x00, 0x7c, 0x00, 0x7f, 0xc0, 0x00, 0x07, 0x80, 0x00, 0x7c, 0x00, 0x7f, 0xc0, 0x00, 0x07, 0x80, 0x00, 0x7c, 0x00, 0x71, 0xc0, 0x00, 0x07, 0x80, 0x00, 0x63, 0x00, 0x71, 0xc0, 0x03, 0x07, 0x80, 0x00, 0x63, 0x00, 0x71, 0xc0, 0x06, 0x07, 0x80, 0x03, 0xc3, 0x00, 0x1f, 0x00, 0x0c, 0x07, 0x80, 0x03, 0x83, 0x00, 0x0e, 0x00, 0x0c, 0x07, 0x80, 0x03, 0x83, 0x00, 0x00, 0x00, 0x0c, 0x07, 0x80, 0x03, 0x83, 0x00, 0x00, 0x00, 0x0c, 0x07, 0x80, 0x03, 0xdf, 0x00, 0x00, 0x00, 0x07, 0x07, 0xe0, 0x03, 0xdf, 0x00, 0x00, 0x00, 0x03, 0x07, 0xe0, 0x03, 0xff, 0x00, 0x00, 0x00, 0x03, 0xff, 0xe0, 0x03, 0xfc, 0x00, 0x00, 0x00, 0x01, 0xff, 0xf0, 0x03, 0xfc, 0x00, 0x00, 0x00, 0x00, 0xfe, 0xf0, 0x00, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x00, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x00, 0x7c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x00, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x00, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x07, 0xf0, 0x00, 0x1f, 0x00, 0x00, 0x00, 0x00, 0x07, 0xe0, 0x00, 0x0f, 0x86, 0x00, 0x00, 0x18, 0x3f, 0xe0, 0x00, 0x00, 0xc6, 0x00, 0x00, 0x1c, 0x3e, 0x00, 0x00, 0x00, 0xcf, 0x01, 0xf0, 0x1c, 0xfe, 0x00, 0x00, 0x00, 0x7f, 0x81, 0xf0, 0x7f, 0xe0, 0x00, 0x00, 0x00, 0xff, 0x81, 0xf0, 0xff, 0xf8, 0x00, 0x00, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xe0, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x07, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

// ================= OBJECTS =================
Adafruit_SSD1306 display(128, 64, &Wire, -1);
RotaryEncoder encoder(PIN_ROT_CLK, PIN_ROT_DT, RotaryEncoder::LatchMode::TWO03);
OneWire oneWire(PIN_DS18);
DallasTemperature sensors(&oneWire);
Preferences prefs;

// ================= VARIABLES =================
int targetTemp = 300;
int currentTemp = 0;
int ambientTemp = 0; 
int pwmOutput = 0;
bool maintenanceMode = false;
bool inSleep = false;
bool inAOD = false;
int lastRotaryPos = 0;
int ghostX = 0; 
int ghostDir = 1;

// Menu System
bool menuOpen = false;
int menuIndex = 0; // 0=Back, 1=Calibrate, 2=Sleep

// Calibration Vars
int tempOffset = 0; 

// PID Variables
double Kp = 10.0, Ki = 0.5, Kd = 5.0; 
double pidError, lastPidError, pidIntegral, pidDerivative;

// Safety & Timers
unsigned long lastActivity = 0;
unsigned long lastGuiUpdate = 0;
unsigned long lastPidCompute = 0;
unsigned long lastSaveTime = 0;
bool settingsChanged = false;
bool errorSensor = false; 

const int pwmCh = 0;

void IRAM_ATTR readEncoderISR() { encoder.tick(); }

// --- FUNGSI AUTO CALIBRATE ---
void autoCalibrate() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(25, 25); display.print("CALIBRATING...");
  display.display();
  delay(500);

  long total = 0;
  for(int i=0; i<10; i++) total += analogRead(PIN_LM358);
  int raw = total / 10;
  
  if (raw > 2500) return; 

  int measured = map(raw, 0, 4095, 0, 520);
  float t = sensors.getTempCByIndex(0);
  int room = (int)t;
  
  if (room > -10 && room < 60 && measured > 0) {
    tempOffset = room - measured;
  }
}

// --- DEEP SLEEP FUNCTION ---
void enterDeepSleep() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(20, 25); display.print("GOODBYE!");
  display.display();
  delay(1000);
  display.ssd1306_command(SSD1306_DISPLAYOFF); // Matikan Layar
  
  ledcWrite(pwmCh, 0); // Matikan Heater
  
  // Konfigurasi Wakeup: Tombol Rotary (GPIO 25) LOW
  // Pastikan Anda menekan tombol untuk bangun
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_ROT_SW, 0); 
  
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_ROT_SW, INPUT_PULLUP);
  pinMode(PIN_T12_SW, INPUT_PULLUP);
  pinMode(PIN_LM358, INPUT);

  prefs.begin("t12_config", false);
  targetTemp = prefs.getInt("target", 300); 

  // --- NYALAKAN LAYAR TERLEBIH DAHULU (PENTING!) ---
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println("OLED ERROR");
    for(;;); 
  }
  
  // Tampilkan Booting Screen
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.setCursor(20, 25); display.print("SYSTEM OK");
  display.display();
  delay(500);

  // --- BARU INISIALISASI SENSOR DAN LAINNYA ---
  sensors.begin();
  sensors.setWaitForConversion(false); 
  sensors.requestTemperatures();       
  
  delay(200);
  autoCalibrate(); // Sekarang aman panggil ini karena layar sudah nyala

  ledcSetup(pwmCh, 2000, 8); 
  ledcAttachPin(PIN_PWM, pwmCh);

  encoder.setPosition(0);
  attachInterrupt(digitalPinToInterrupt(PIN_ROT_CLK), readEncoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ROT_DT),  readEncoderISR, CHANGE);
  lastActivity = millis();
}

int readTempAccurate() {
  ledcWrite(pwmCh, 0);
  delayMicroseconds(200); 

  long total = 0;
  for(int i=0; i<20; i++) total += analogRead(PIN_LM358);
  int raw = total / 20;
  
  // Deteksi No Tip (Limit 2500)
  if (raw > 2500) { errorSensor = true; return 999; } 
  else { errorSensor = false; }

  int t12Temp = map(raw, 0, 4095, 0, 520); 

  static unsigned long lastDSCheck = 0;
  if (millis() - lastDSCheck > 2000) {
    float t = sensors.getTempCByIndex(0);
    if (t > -127 && t < 100) ambientTemp = (int)t;
    sensors.requestTemperatures(); 
    lastDSCheck = millis();
  }
  
  int finalT = t12Temp + tempOffset;
  if (finalT < 0) finalT = 0;
  return finalT;
}

void drawMirroredBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, uint16_t color) {
  int16_t byteWidth = (w + 7) / 8; 
  uint8_t byte = 0;
  for (int16_t j = 0; j < h; j++) {
    for (int16_t i = 0; i < w; i++) {
      if (i & 7) byte <<= 1;
      else byte = pgm_read_byte(bitmap + j * byteWidth + i / 8);
      if (byte & 0x80) display.drawPixel(x + w - 1 - i, y + j, color);
    }
  }
}

void loop() {
  unsigned long now = millis();

  // === 1. INPUT HANDLING (MENU SYSTEM) ===
  static bool btnStateLast = HIGH;
  static unsigned long btnPressTime = 0;
  bool btnState = digitalRead(PIN_ROT_SW);

  // Detect Press Start
  if (btnState == LOW && btnStateLast == HIGH) {
    btnPressTime = now;
  }

  // Detect Press Release
  if (btnState == HIGH && btnStateLast == LOW) {
    unsigned long duration = now - btnPressTime;
    
    if (duration > LONG_PRESS_MS) {
      // --- LONG PRESS: TOGGLE MENU ---
      menuOpen = !menuOpen;
      menuIndex = 0; // Reset ke menu pertama
      if(!menuOpen) lastActivity = now; 
    } 
    else {
      // --- SHORT PRESS ---
      if (menuOpen) {
        // Execute Menu Item
        if (menuIndex == 0) { // Back
          menuOpen = false;
        } 
        else if (menuIndex == 1) { // Re-Calibrate
          autoCalibrate();
          menuOpen = false;
        } 
        else if (menuIndex == 2) { // Sleep
          enterDeepSleep();
        }
      } else {
        // Normal Mode: Toggle Maintenance
        maintenanceMode = !maintenanceMode;
        pidIntegral = 0;
        lastActivity = now; inAOD = false; ghostX = 0;
      }
    }
  }
  btnStateLast = btnState;

  int newPos = encoder.getPosition();
  if (newPos != lastRotaryPos) {
    lastActivity = now; inSleep = false; inAOD = false; ghostX = 0;
    
    if (menuOpen) {
      // --- MENU NAVIGATION ---
      if (newPos > lastRotaryPos) menuIndex++; else menuIndex--;
      if (menuIndex > 2) menuIndex = 0;
      if (menuIndex < 0) menuIndex = 2;
    } 
    else if (!maintenanceMode) {
      // --- TEMP ADJUSTMENT ---
      bool up = (newPos < lastRotaryPos);
      if (REVERSE_ROTARY) up = !up;
      if (up) targetTemp += 5; else targetTemp -= 5;
      targetTemp = constrain(targetTemp, MIN_TEMP, MAX_TEMP);
      settingsChanged = true;
      lastSaveTime = now;
    }
    lastRotaryPos = newPos;
  }

  // Handle Switch
  bool handleActive = (digitalRead(PIN_T12_SW) == LOW);
  if (handleActive) { lastActivity = now; inSleep = false; inAOD = false; ghostX = 0; }

  // Timeout Logic
  if (!maintenanceMode && !handleActive && !menuOpen) {
    if (now - lastActivity > TIME_TO_AOD) inAOD = true;
    else if (now - lastActivity > TIME_TO_SLEEP) inSleep = true;
  }

  // === 2. HEATER CONTROL ===
  if (now - lastPidCompute > 100) {
    lastPidCompute = now;
    currentTemp = readTempAccurate(); 

    int setPoint = targetTemp;
    if (maintenanceMode || errorSensor || menuOpen) setPoint = 0; 
    else if (inSleep || inAOD) setPoint = SLEEP_TEMP;

    pidError = setPoint - currentTemp;
    pidIntegral += pidError;
    pidDerivative = pidError - lastPidError;
    pidIntegral = constrain(pidIntegral, -255, 255);

    double output = (Kp * pidError) + (Ki * pidIntegral) + (Kd * pidDerivative);
    if (currentTemp > 500 || errorSensor) output = 0;
    
    pwmOutput = constrain(output, 0, 255);
    lastPidError = pidError;
    ledcWrite(pwmCh, pwmOutput);
  }

  // 3. AUTO SAVE
  if (settingsChanged && (now - lastSaveTime > 3000)) {
    prefs.putInt("target", targetTemp);
    settingsChanged = false;
  }

  // === 4. GUI DISPLAY ===
  if (now - lastGuiUpdate > 50) { 
    lastGuiUpdate = now;
    display.clearDisplay();

    // --- MENU OVERLAY MODE ---
    if (menuOpen) {
      display.setTextSize(1);
      display.setCursor(30, 0); display.print("--- MENU ---");
      
      display.setCursor(10, 20); 
      if(menuIndex == 0) display.print("> "); display.print("BACK to STATION");
      
      display.setCursor(10, 35); 
      if(menuIndex == 1) display.print("> "); display.print("RE-CALIBRATE");
      
      display.setCursor(10, 50); 
      if(menuIndex == 2) display.print("> "); display.print("DEEP SLEEP");
      
      display.display();
      return; 
    }

    // --- AOD GHOST ---
    if (inAOD && !maintenanceMode ) //&& !errorSensor
     {
      ghostX += (2 * ghostDir);
      if (ghostX >= 64) { ghostX = 64; ghostDir = -1; }
      else if (ghostX <= 0) { ghostX = 0; ghostDir = 1; }

      if (ghostDir == 1) drawMirroredBitmap(ghostX, 0, ghost_bmp, 64, 64, WHITE);
      else display.drawBitmap(ghostX, 0, ghost_bmp, 64, 64, WHITE);

      int shadowWidth = 24 + ((now / 150) % 6);
      int centerX = ghostX + 32;
      display.fillRect(centerX - (shadowWidth/2), 62, shadowWidth, 2, WHITE);
      display.display();
      return; 
    }

    // --- MAIN UI ---
    display.fillRoundRect(0, 0, 48, 15, 6, WHITE);
    display.setTextColor(BLACK, WHITE);
    display.setTextSize(1);
    display.setCursor(4, 4);
    display.print(targetTemp); display.print(" C");
    display.setTextColor(WHITE);

    int iconX = 110; 
    if (errorSensor) {
        display.setCursor(115, 4); display.print("!"); 
    } else {
        if (maintenanceMode) display.drawBitmap(iconX, 0, icon_gear, 16, 16, WHITE);
        else if (inSleep) display.drawBitmap(iconX, 0, icon_snow, 16, 16, WHITE);
        else if (handleActive) display.drawBitmap(iconX, 0, icon_hand, 16, 16, WHITE);
        else if (pwmOutput > 30) {
           if ((now/200)%2 == 0) display.drawBitmap(iconX, 0, icon_fire, 16, 16, WHITE);
        }
    }

    if (errorSensor) {
        display.setTextSize(3); 
        display.setCursor(10, 25);
        display.print("NO TIP"); 
        display.drawLine(0, 61, 128, 64, WHITE);
        display.drawLine(0, 64, 128, 61, WHITE);
    } else {
        display.setTextSize(4);
        int baseX = 20; 
        if (currentTemp < 100) baseX = 32; 
        if (currentTemp < 10) baseX = 44;
        display.setCursor(baseX, 20);
        display.print(currentTemp);

        display.setTextSize(2);
        int unitX = baseX + (currentTemp >= 100 ? 74 : (currentTemp >= 10 ? 49 : 25));
        display.fillCircle(unitX - 2, 22, 2, WHITE);
        display.setCursor(unitX + 2, 34); 
        display.print("C");

        display.setTextSize(1);
        display.setCursor(unitX + 16, 40); 
        display.print(ambientTemp);
        display.drawCircle(unitX + 28, 40, 1, WHITE); 

        display.drawRect(0, 61, 128, 3, WHITE);
        int barW = map(currentTemp, 0, MAX_TEMP, 0, 126);
        if(barW > 0) display.fillRect(1, 62, barW, 1, WHITE);

        int tX = map(targetTemp, 0, MAX_TEMP, 0, 126);
        display.drawPixel(tX, 60, WHITE);
        display.drawPixel(tX-1, 59, WHITE);
        display.drawPixel(tX+1, 59, WHITE);
        display.drawFastHLine(tX-2, 58, 5, WHITE);
    }

    display.display();
  }
}