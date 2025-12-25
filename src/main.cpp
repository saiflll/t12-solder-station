#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "config.h"

Adafruit_SSD1306 display(128, 64, &Wire, -1);
OneWire oneWire(PIN_DS18);
DallasTemperature sensors(&oneWire);
Preferences pref;

volatile SystemSettings settings = {DEF_PWM, DEF_FREQ, DEF_DELAY};
volatile bool setChanged = false;
volatile unsigned long lastChg = 0;
volatile int curMenu = 0; 
volatile bool isMaint = false;
volatile float ambTemp = 30.0;
volatile float tipTemp = 30.0;
volatile float vIn = 0.0;
volatile int appPWM = 0;
volatile float pWatt = 0.0;

String stat = "SLEEP";
unsigned long bstEnd = 0;
int bstVal = 0;
unsigned long slpStart = 0;
volatile unsigned long lastEncTime = 0; 
unsigned long lastTchTime = 0;

void saveSet() {
    if (setChanged && (millis() - lastChg > 2000)) {
        pref.begin("t12_cfg", false);
        pref.putInt("pwm", settings.pwm);
        pref.putInt("freq", settings.freq);
        pref.putInt("delay", settings.loopDelay);
        pref.end();
        setChanged = false;
    }
}

void TaskTel(void *pv) {
    for(;;) {
        if (WiFi.status() == WL_CONNECTED) {
            HTTPClient http;
            http.begin(SERVER_URL);
            http.addHeader("Content-Type", "application/json");
            StaticJsonDocument<256> doc;
            doc["ambient"] = ambTemp;
            doc["tip"] = tipTemp;
            doc["voltage"] = vIn;
            doc["pwm"] = appPWM;
            doc["power"] = pWatt;
            doc["status"] = stat;
            String body;
            serializeJson(doc, body);
            http.POST(body);
            http.end();
        }
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

void TaskInput(void *pv) {
    int lClk = digitalRead(ROT_CLK);
    int lBtn = HIGH;
    unsigned long bTime = 0;
    unsigned long lClick = 0;
    bool lpOk = false;

    for (;;) {
        int cClk = digitalRead(ROT_CLK);
        if (cClk != lClk && cClk == LOW) {
            unsigned long now = millis();
            unsigned long dt = now - lastEncTime;
            lastEncTime = now;
            int dir = (digitalRead(ROT_DT) == HIGH) ? 1 : -1;
            int stp = (dt < 30) ? 10 : 1;
            if (curMenu == 0) settings.pwm = constrain(settings.pwm + (dir * (stp > 5 ? 5 : 1)), 0, 255);
            else if (curMenu == 1) settings.freq = constrain(settings.freq + (dir * (stp > 1 ? 100 : 10)), 100, 50000);
            else settings.loopDelay = constrain(settings.loopDelay + (dir * (stp > 1 ? 10 : 1)), 10, 1000);
            setChanged = true; lastChg = now;
        }
        lClk = cClk;
        int btn = digitalRead(ROT_SW);
        if (btn == LOW && lBtn == HIGH) { bTime = millis(); lpOk = false; }
        if (btn == LOW && !lpOk) { if (millis() - bTime > 2000) { isMaint = !isMaint; lpOk = true; } }
        if (btn == HIGH && lBtn == LOW) { if (!lpOk) { if (millis() - lClick < 400) curMenu = (curMenu + 1) % 3; lClick = millis(); } }
        lBtn = btn;

        if (touchRead(T12_SW) < T_THRESHOLD) lastTchTime = millis();
        if (!isMaint) stat = (millis() - lastTchTime < T_HOLD) ? "ACTIVE" : "SLEEP";
        else stat = "SERVICE";

        saveSet();
        vTaskDelay(5);
    }
}

void TaskHeat(void *pv) {
    bool lAct = false;
    int cFreq = settings.freq;
    for (;;) {
        if (settings.freq != cFreq) { ledcSetup(0, settings.freq, 8); ledcAttachPin(PIN_PWM, 0); cFreq = settings.freq; }
        
        int tipR = analogRead(PIN_LM358);
        tipTemp = map(tipR, 0, 4095, (int)ambTemp, 450); 
        int vR = analogRead(PIN_VOLT);
        vIn = (vR / 4095.0) * 3.3 * ((R1 + R2) / R2);
        float dty = appPWM / 255.0;
        pWatt = (vIn * vIn / 8.0) * dty; 

        if (isMaint) { appPWM = 0; ledcWrite(0, 0); vTaskDelay(100); continue; }
        float trg = settings.pwm * 4.0;
        if (trg < ambTemp) trg = ambTemp;

        if (stat == "ACTIVE") {
            if (!lAct) {
                unsigned long sDur = millis() - slpStart;
                if (sDur > COLD_MS) { bstEnd = millis() + 3000; bstVal = constrain(settings.pwm * 3, 0, 255); } 
                else { float g = trg - tipTemp; long d = map((long)constrain(g, 0, 300), 0, 300, 0, 4000); bstEnd = millis() + d; bstVal = constrain((int)(settings.pwm * 2.5), 0, 255); }
            }
            appPWM = (millis() < bstEnd) ? bstVal : settings.pwm;
            ledcWrite(0, appPWM);
        } else {
            if (lAct) slpStart = millis();
            appPWM = 0; ledcWrite(0, 0);
        }
        lAct = (stat == "ACTIVE");
        vTaskDelay(settings.loopDelay);
    }
}

void TaskDisp(void *pv) {
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    for (;;) {
        sensors.requestTemperatures();
        float t = sensors.getTempCByIndex(0);
        if (t > -50 && t < 120) ambTemp = t;

        display.clearDisplay();
        if (isMaint) {
            display.fillRect(0, 0, 128, 16, SSD1306_WHITE); display.setTextColor(SSD1306_BLACK);
            display.setCursor(30, 4); display.print("MAINTENANCE");
            display.setTextColor(SSD1306_WHITE); display.setTextSize(2); display.setCursor(40, 30); display.print("SAFE");
            display.setTextSize(1); display.setCursor(20, 52); display.print("HEATER DISABLED");
        } else {
            display.setTextSize(1); display.setCursor(0, 0);
            if (stat == "ACTIVE") display.print(millis() < bstEnd ? "BOOST!" : "HEAT"); else display.print("IDLE");
            display.setCursor(80, 0); display.print("RT:"); display.print((int)ambTemp);
            display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
            display.setCursor(60, 15); display.setTextSize(3); display.print((int)tipTemp);
            display.setCursor(0, 15 + (curMenu * 13)); display.print(">");
            display.setCursor(12, 15); display.print("P"); display.setCursor(12, 28); display.print("F"); display.setCursor(12, 41); display.print("D");
            display.setCursor(30, 25); display.setTextSize(2);
            if (curMenu == 0) display.print(settings.pwm); else if (curMenu == 1) { if (settings.freq > 9999) display.setTextSize(1); display.print(settings.freq); } else display.print(settings.loopDelay);
            display.setTextSize(1); display.setCursor(90, 45); display.print((int)pWatt); display.print("W");
            if (WiFi.status() == WL_CONNECTED) { display.setCursor(125, 0); display.print("."); }
        }
        display.display();
        vTaskDelay(100);
    }
}

void setup() {
    Serial.begin(115200);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    pref.begin("t12_cfg", false);
    settings.pwm = pref.getInt("pwm", DEF_PWM);
    settings.freq = pref.getInt("freq", DEF_FREQ);
    settings.loopDelay = pref.getInt("delay", DEF_DELAY);
    pref.end();
    pinMode(ROT_CLK, INPUT); pinMode(ROT_DT, INPUT); pinMode(ROT_SW, INPUT_PULLUP); pinMode(PIN_VOLT, INPUT); pinMode(PIN_LM358, INPUT);
    touchSetCycles(0x3000, 0x3000);
    ledcSetup(0, settings.freq, 8); ledcAttachPin(PIN_PWM, 0); ledcWrite(0, 0);
    Wire.begin(OLED_SDA, OLED_SCL); sensors.begin();
    xTaskCreate(TaskInput, "In", 4096, NULL, 2, NULL);
    xTaskCreate(TaskHeat, "Heat", 4096, NULL, 3, NULL);
    xTaskCreate(TaskDisp, "Disp", 4096, NULL, 1, NULL);
    xTaskCreate(TaskTel, "Tel", 4096, NULL, 1, NULL);
}

void loop() { vTaskDelete(NULL); }