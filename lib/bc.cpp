#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RotaryEncoder.h>

// OLED ==========================
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1);

// Rotary Encoder =================
#define PIN_CLK 34
#define PIN_DT  35
#define PIN_SW  25
RotaryEncoder encoder(PIN_CLK, PIN_DT, RotaryEncoder::LatchMode::TWO03);

// PWM ============================
#define PWM_PIN 18
int pwmChannel = 0;
int freq = 20000;
int resolution = 10;     // 0–1023
int duty = 0;            // 0–100%
bool pwmEnabled = true;

unsigned long lastBtn = 0;
int menu = 0; // 0=Monitor , 1=Config , 2=System
bool navActive = false;      // true when in menu-selection mode
bool inMenuControl = false;  // true when inside a menu's control mode

const unsigned long LONG_PRESS_MS = 800;

// AOD idle system
bool aodMode = false;
unsigned long lastActivityTime = 0;
const unsigned long AOD_IDLE_MS = 5000;  // 5 seconds idle before AOD

void resetIdleTimer() {
  lastActivityTime = millis();
  if (aodMode) {
    // exit AOD mode immediately on activity
    aodMode = false;
    menu = 0;
  }
}

// ISR
void IRAM_ATTR readEncoderISR() {
  encoder.tick();
}

void drawProgress(int x, int y, int w, int h, int value, int maxVal) {
  if (w <= 2 || h <= 2) {
    // too small to draw border+fill; draw a single filled rect proportionally
    int fill = map(value, 0, maxVal, 0, w);
    if (fill > 0) display.fillRect(x, y, fill, h, WHITE);
    return;
  }

  int fill = map(value, 0, maxVal, 0, w);
  display.drawRect(x, y, w, h, WHITE);
  int innerMax = w - 2;
  int innerFill = constrain(fill - 2, 0, innerMax);
  if (innerFill > 0 && h > 2) display.fillRect(x + 1, y + 1, innerFill, h - 2, WHITE);
}

void setup() {
  Serial.begin(115200);

  Wire.begin(22, 21);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  pinMode(PIN_SW, INPUT_PULLUP);
  encoder.setPosition(0);
  attachInterrupt(digitalPinToInterrupt(PIN_CLK), readEncoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_DT),  readEncoderISR, CHANGE);

  ledcSetup(pwmChannel, freq, resolution);
  ledcAttachPin(PWM_PIN, pwmChannel);
}

void loop() {
  encoder.tick();
  static int lastPos = 0;
  int newPos = encoder.getPosition();

  // Check for rotary encoder activity
  if (newPos != lastPos) {
    resetIdleTimer();
    
    // invert encoder direction: treat clockwise as negative
    int diff = lastPos - newPos;

    if (navActive) {
      // change selected menu while in navigation mode
      menu = constrain(menu + (diff > 0 ? 1 : -1), 0, 2);  // 0-2 only, no AOD in menu
    }
    else if (inMenuControl) {
      // when inside a menu's control mode, let encoder adjust the control
      if (menu == 0) {
        duty = constrain(duty + diff * 2, 0, 100);
      } else {
        // fallback: allow cycling menus
        menu = constrain(menu + (diff > 0 ? 1 : -1), 0, 2);  // 0-2 only
      }
    }
    else {
      // normal operation
      if (menu == 0) { // Monitor mode → adjust PWM
        duty = constrain(duty + diff * 2, 0, 100);
      }
      else { // Config / System → geser menu list
        menu = constrain(menu + (diff > 0 ? 1 : -1), 0, 2);  // 0-2 only
      }
    }
    lastPos = newPos;
  }

  // Button handling: detect short vs long press
  static bool btnPrev = false;
  static unsigned long btnPressedAt = 0;
  bool btn = !digitalRead(PIN_SW); // true when pressed (INPUT_PULLUP)

  if (btn && !btnPrev) {
    // pressed now
    btnPressedAt = millis();
  }

  if (!btn && btnPrev) {
    // released now -> determine short or long press
    resetIdleTimer();
    
    unsigned long dur = millis() - btnPressedAt;
    if (dur < LONG_PRESS_MS) {
      // SHORT PRESS
      if (!navActive && !inMenuControl) {
        // Monitor normal: toggle PWM
        if (menu == 0) pwmEnabled = !pwmEnabled;
      }
      else if (navActive) {
        // OK: enter selected menu's control mode
        inMenuControl = true;
        navActive = false;
      }
      else if (inMenuControl) {
        // when inside control, short press acts as OK/toggle
        if (menu == 0) pwmEnabled = !pwmEnabled;
        // other menus can implement their OK actions later
      }
    } else {
      // LONG PRESS
      if (!navActive && !inMenuControl) {
        // enter navigation mode
        navActive = true;
      }
      else if (navActive) {
        // exit nav and go to control mode
        inMenuControl = true;
        navActive = false;
      }
      else if (inMenuControl) {
        // exit control mode back to monitor
        inMenuControl = false;
        menu = 0;
      }
    }
  }
  btnPrev = btn;

  // Check if should enter AOD mode (idle for 5s, solder off, no menus active)
  unsigned long idleTime = millis() - lastActivityTime;
  if (!aodMode && !pwmEnabled && !navActive && !inMenuControl && idleTime >= AOD_IDLE_MS) {
    aodMode = true;
  }

  // Apply PWM
  ledcWrite(pwmChannel, pwmEnabled ? map(duty, 0, 100, 0, 1023) : 0);

  // Dummy temp
  int tempReal = 320;
  int tempTarget = 350;

  display.clearDisplay();
  display.setTextSize(1);

  // === AOD MODE (Idle display with simple eye expressions) ===
  if (aodMode) {
    // Simple eye animation: look right -> look left -> closed -> repeat
    unsigned long aodCycle = (millis() % 3000); // 3 second cycle
    uint8_t aodFrame = 0;
    if (aodCycle < 750) aodFrame = 0;        // look right (0-750ms)
    else if (aodCycle < 1500) aodFrame = 1;  // look left (750-1500ms)
    else aodFrame = 2;                       // closed (1500-3000ms)

    int cx = OLED_WIDTH / 2;
    int cy = OLED_HEIGHT / 2;

    // Simple head outline
    display.drawCircle(cx, cy - 2, 18, WHITE);

    int eyeY = cy - 6;
    int eyeR = 3;

    if (aodFrame == 2) {
      // CLOSED: just horizontal lines
      display.drawLine(cx - 8, eyeY, cx - 2, eyeY, WHITE);
      display.drawLine(cx + 2, eyeY, cx + 8, eyeY, WHITE);
    } else if (aodFrame == 0) {
      // LOOK RIGHT: pupils shifted right
      display.fillCircle(cx - 8, eyeY, eyeR, WHITE);        // left eye
      display.fillCircle(cx + 8, eyeY, eyeR, WHITE);        // right eye
      display.fillCircle(cx - 6, eyeY, 1, BLACK);           // left pupil (right-shifted)
      display.fillCircle(cx + 10, eyeY, 1, BLACK);          // right pupil (right-shifted)
    } else {
      // LOOK LEFT: pupils shifted left
      display.fillCircle(cx - 8, eyeY, eyeR, WHITE);        // left eye
      display.fillCircle(cx + 8, eyeY, eyeR, WHITE);        // right eye
      display.fillCircle(cx - 10, eyeY, 1, BLACK);          // left pupil (left-shifted)
      display.fillCircle(cx - 6, eyeY, 1, BLACK);           // right pupil (left-shifted)
    }

    // Mouth (small smile)
    display.drawLine(cx - 2, cy + 8, cx + 2, cy + 8, WHITE);
  }
  // === END AOD MODE ===
  else if (menu != 0 && menu != 1 && menu != 2) {
    // Safety: if menu somehow is not 0-2, reset
    menu = 0;
  }

  // Title (centered) and indicator above/beside title when PWM is on
  if (!aodMode) {
    const char* title = "solder station";
    int16_t tbx, tby;
    uint16_t tbw, tbh;
    display.setTextSize(1);
    display.getTextBounds(title, 0, 0, &tbx, &tby, &tbw, &tbh);
    int titleX = (OLED_WIDTH - tbw) / 2;
    int titleY = 0;
    // indicator positioned to the left of the centered title
    int indicatorCX = titleX - 8;
    if (indicatorCX < 4) indicatorCX = 4;
    int indicatorCY = titleY + tbh / 2;
    display.setCursor(titleX, titleY);
    display.print(title);
    if (pwmEnabled) display.fillCircle(indicatorCX, indicatorCY, 3, WHITE);
    // 1-pixel spacer line below title
    display.drawFastHLine(0, 12, OLED_WIDTH, WHITE);
  }

  // === Monitor Screen ===
  if (menu == 0 && !aodMode) {
    // Split screen: left = PWM, right = Temp
    const int gap = 4;
    const int barW = (OLED_WIDTH - gap) / 2; // two equal bars with small gap
    const int leftX = 0;
    const int rightX = leftX + barW + gap;
    const int barY = 26;
    const int barH = 4; // thinner bar

    const int leftLabelX = leftX + 4; // small left margin

    // Left: PWM
    display.setCursor(leftLabelX, 14);
    display.print("PWM: ");
    display.print(duty);
    display.print("%");
    drawProgress(leftX, barY, barW, barH, duty, 100);

    // Right: Temp (compact label to fit)
    display.setCursor(rightX, 14);
    display.print("T:");
    display.print(tempReal);
    display.print("/");
    display.print(tempTarget);
    display.print("C");
    drawProgress(rightX, barY, barW, barH, tempReal, 450);
  }

  // === Config Screen (placeholder) ===
  if (menu == 1 && !aodMode) {
    display.setCursor(10, 30);
    display.print("CONFIG MENU");
  }

  // === System Screen (placeholder) ===
  if (menu == 2 && !aodMode) {
    display.setCursor(10, 30);
    display.print("SYSTEM INFO");
  }

  // Bottom Menu Bar (hide in AOD mode)
  if (!aodMode) {
    display.setCursor(0, 56);
    display.print(menu == 0 ? ">Monitor< " : "Monitor ");
    display.print(menu == 1 ? "| >Config< " : "| Config ");
    display.print(menu == 2 ? "| >System<" : "| System");
  }

  display.display();
  delay(5);
}
