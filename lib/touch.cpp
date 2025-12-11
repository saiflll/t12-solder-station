#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ==========================================
//              KONFIGURASI PIN
// ==========================================
#define PIN_OLED_SDA    22
#define PIN_OLED_SCL    21
#define PIN_ROT_CLK     34
#define PIN_ROT_DT      35
#define PIN_ROT_SW      25
#define PIN_LM358       26  
#define PIN_T12_SW      33  // Pin Touch Sensor
#define PIN_PWM         18  
#define PIN_DS18        23  

// ==========================================
//          SETTING TOUCH SENSOR
// ==========================================
const int touchPin = 33;
const int threshold = 27;     // Settingan Pilihan Kamu
const int debounceTarget = 3; // Harus terdeteksi 3x loop baru valid

// Variabel Touch
int touchValue = 0;
int debounceCounter = 0;
String statusHandle = "SLEEP";

// ==========================================
//          OBJECT & VARIABEL LAIN
// ==========================================
Adafruit_SSD1306 display(128, 64, &Wire, -1);
OneWire oneWire(PIN_DS18);
DallasTemperature sensors(&oneWire);

// Variabel Encoder & Heater
int pwmSet = 0;        
int rawADC = 0;        
float realTemp = 0.0;  
int lastClk = HIGH;

// PWM Properties
const int freq = 2000;
const int ledChannel = 0;
const int resolution = 8;

void setup() {
  Serial.begin(115200);

  // 1. Init Pins
  pinMode(PIN_ROT_CLK, INPUT);
  pinMode(PIN_ROT_DT, INPUT);
  pinMode(PIN_ROT_SW, INPUT_PULLUP);
  pinMode(PIN_LM358, INPUT);
  
  // 2. SETTING SENSITIVITAS TOUCH (MAGIC FUNCTION)
  // Ini kunci agar sensor peka dan bisa tembus isolasi
  touchSetCycles(0x3000, 0x3000); 

  // 3. Init PWM (Heater)
  ledcSetup(ledChannel, freq, resolution);
  ledcAttachPin(PIN_PWM, ledChannel);
  ledcWrite(ledChannel, 0); // Start kondisi mati

  // 4. Init DS18B20
  sensors.begin();

  // 5. Init OLED
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println("OLED Error");
    for(;;); 
  }
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("T12 SYSTEM START...");
  display.display();
  delay(1000);
}

void loop() {
  // ----------------------------------------
  // A. BACA ENCODER (Pengatur Panas Manual)
  // ----------------------------------------
  int newClk = digitalRead(PIN_ROT_CLK);
  if (newClk != lastClk && newClk == LOW) {
    if (digitalRead(PIN_ROT_DT) == HIGH) {
      pwmSet += 10; 
    } else {
      pwmSet -= 10;
    }
    // Limit PWM (Safety)
    if (pwmSet > 255) pwmSet = 255; 
    if (pwmSet < 0) pwmSet = 0;
  }
  lastClk = newClk;

  // ----------------------------------------
  // B. BACA TOUCH SENSOR (Dengan Debounce)
  // ----------------------------------------
  touchValue = touchRead(touchPin);

  // Logika Debounce (Peredam Noise)
  if (touchValue < threshold) {
    // Jika nilai dibawah 27 (terdeteksi sentuh), naikkan counter
    if (debounceCounter <= debounceTarget) {
      debounceCounter++;
    }
  } else {
    // Jika nilai tinggi (diam), reset counter
    debounceCounter = 0;
  }

  // Tentukan Status Akhir
  if (debounceCounter >= debounceTarget) {
    statusHandle = "AKTIF";
  } else {
    statusHandle = "SLEEP";
  }

  // ----------------------------------------
  // C. BACA SUHU T12 (ADC)
  // ----------------------------------------
  ledcWrite(ledChannel, 0); // Matikan heater sesaat agar ADC bersih
  delay(5); 

  long totalADC = 0;
  for(int i=0; i<5; i++){
    totalADC += analogRead(PIN_LM358);
  }
  rawADC = totalADC / 5;

  // ----------------------------------------
  // D. KONTROL HEATER (Logic Safety)
  // ----------------------------------------
  if (statusHandle == "AKTIF") {
     // Jika gagang dipegang, nyalakan sesuai settingan encoder
     ledcWrite(ledChannel, pwmSet);
  } else {
     // Jika gagang diletakkan (Sleep), MATIKAN HEATER (Dingin)
     ledcWrite(ledChannel, 0); 
  }

  // ----------------------------------------
  // E. BACA SUHU RUANGAN & DISPLAY
  // ----------------------------------------
  sensors.requestTemperatures(); 
  realTemp = sensors.getTempCByIndex(0);

  display.clearDisplay();
  
  // Baris 1: PWM Setting & Raw Touch (Buat pantau)
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print("PWM:"); 
  display.print(pwmSet);
  display.setCursor(64, 0);
  display.print("Touch:"); 
  display.println(touchValue); // Biar tau nilai aslinya

  // Baris 2: Suhu Ruangan
  display.setCursor(0, 12);
  display.print("Room: "); 
  display.print(realTemp, 1);
  display.print(" C");

  // Baris 3: ADC T12
  display.setCursor(0, 24);
  display.print("ADC Tip: "); 
  display.println(rawADC);

  // Baris 4: STATUS (Besar)
  display.setTextSize(2);
  display.setCursor(0, 45);
  display.print(statusHandle);

  display.display();

  // Debug Serial Monitor
  Serial.print("RawTouch:");
  Serial.print(touchValue);
  Serial.print(" | PWM:");
  Serial.print(pwmSet);
  Serial.print(" | Status:");
  Serial.println(statusHandle);

  delay(50); 
}