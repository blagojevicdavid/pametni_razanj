#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <AiEsp32RotaryEncoder.h>


// ----------------- PINOUT -----------------
static constexpr uint8_t PIN_PWM = 12;

// OLED
static constexpr uint8_t PIN_I2C_SDA = 33;
static constexpr uint8_t PIN_I2C_SCL = 18;
static constexpr uint8_t OLED_ADDR   = 0x3C;

// Rotary encoder
static constexpr uint8_t PIN_ENC_CLK = 40; 
static constexpr uint8_t PIN_ENC_DT  = 39; 
static constexpr uint8_t PIN_ENC_SW  = 37; 

// ----------------- OLED -----------------
static constexpr int SCREEN_WIDTH  = 128;
static constexpr int SCREEN_HEIGHT = 64;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ----------------- LED -------------------
#define STATUS_LED 16

// ----------------- PWM -------------------
static constexpr uint8_t  PWM_CH   = 0;
static constexpr uint32_t PWM_FREQ = 20000; // 20kHz (quiet)
static constexpr uint8_t  PWM_BITS = 10;    // 0..1023
static constexpr uint16_t PWM_MAX  = (1u << PWM_BITS) - 1;

// ----------------- Rotary library -----------------
AiEsp32RotaryEncoder rotaryEncoder(PIN_ENC_DT, PIN_ENC_CLK, PIN_ENC_SW, -1, 4);

void IRAM_ATTR readEncoderISR() {
  rotaryEncoder.readEncoder_ISR();
}


volatile int speedPct = 0;     // 0..100
bool running = true;

// Long press handling
static constexpr uint32_t LONG_PRESS_MS = 1100;
bool btnWasDown = false;
uint32_t btnDownAt = 0;
bool longPressHandled = false;

static inline uint32_t pctToDuty(int pct) {
  pct = constrain(pct, 0, 100);
  return (uint32_t)((pct * (uint32_t)PWM_MAX) / 100u);
}

static void applyPwm() {
  uint32_t duty = running ? pctToDuty(speedPct) : 0;
  ledcWrite(PWM_CH, duty);
}

static void drawUi() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  display.print("BRZINA:");
  display.print(speedPct); 

  //show state on second line
  display.setCursor(0, 18);
  display.print(running ? "RUN" : "STOP");
  digitalWrite(STATUS_LED, running);

  display.display();
}

void setup() {

  // I2C + OLED
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(400000);

  pinMode(STATUS_LED, OUTPUT);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    // If OLED init fails
    while (true) { delay(100); }
  }
  drawUi();

  //PWM
  ledcSetup(PWM_CH, PWM_FREQ, PWM_BITS);
  ledcAttachPin(PIN_PWM, PWM_CH);

  //Rotary encoder setup
  rotaryEncoder.begin();
  rotaryEncoder.setup(readEncoderISR);

  // Range 0..100
  rotaryEncoder.setBoundaries(0, 100, false);

  // Acceleration
  rotaryEncoder.setAcceleration(100); // 

  //Start at 0
  rotaryEncoder.setEncoderValue(speedPct);

  applyPwm();
}

void loop() {
  //Handle rotation
  if (rotaryEncoder.encoderChanged()) {
    speedPct = rotaryEncoder.readEncoder();
    applyPwm();
    drawUi();
  }

  // Handle long press on SW 
  bool btnDown = (digitalRead(PIN_ENC_SW) == LOW);

  uint32_t now = millis();

  if (btnDown && !btnWasDown) {
    // just pressed
    btnWasDown = true;
    btnDownAt = now;
    longPressHandled = false;
  } else if (btnDown && btnWasDown) {
    // held
    if (!longPressHandled && (now - btnDownAt >= LONG_PRESS_MS)) {
      running = !running;      // toggle start/stop
      longPressHandled = true;
      applyPwm();
      drawUi();
    }
  } else if (!btnDown && btnWasDown) {
    // released
    btnWasDown = false;

    // short press reserved for later
    // ( if (!longPressHandled) { short press ... })
  }

  delay(2);
}
