#include <Arduino.h>
#include <Wire.h>
#include "PCF8575.h"

// ---------------------------------------------------------------------------
// Pinout
// ---------------------------------------------------------------------------
// ESP32          PCF8575 (I2C addr 0x20, A0/A1/A2 -> GND)
// -----          --------------------------------------
// GPIO21   ----- SDA
// GPIO22   ----- SCL
// GPIO27   ----- INT   (active low, opendrain -> needs pullup, triggers on
//                        any P0-P17 change)
// 3V3      ----- VCC
// GND      ----- GND
//
// PCF8575 expander pins
// ----------------------
// P0     : unused   (INPUT)
// P1-P4  : buttons  (INPUT)    -> button0=P1, button1=P2, button2=P3, button3=P4
// P5-P7  : unused   (INPUT)
// P10-P17: LEDs     (OUTPUT)
// ---------------------------------------------------------------------------

// Default address 0x20 when A0, A1, A2 are all grounded
PCF8575 pcf8575(0x20);

const int INT_PIN = 27; // optional, connect to PCF8575 INT

const unsigned long debounceDelay = 50; // ms, tune per your buttons

// Per-button debounce tracking (P1 to P4)
unsigned long lastDebounceTime[4] = {0, 0, 0, 0};
bool lastButtonState[4] = {HIGH, HIGH, HIGH, HIGH};

volatile bool buttonChanged = false;

void IRAM_ATTR onPcfInterrupt() {
  buttonChanged = true;
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22); // SDA, SCL

  // Set pin modes: P0 to P7 as inputs (buttons), P10 to P17 as outputs (LEDs)
  for (int i = 0; i < 8; i++) {
    pcf8575.pinMode(i, INPUT);
  }
  for (int i = 8; i < 16; i++) {
    pcf8575.pinMode(i, OUTPUT);
  }

  pcf8575.begin();

  pinMode(INT_PIN, INPUT_PULLUP); // active low interrupt from PCF8575
  attachInterrupt(digitalPinToInterrupt(INT_PIN), onPcfInterrupt, FALLING);
}

void handleButton(int index, bool currentState, int ledPin, const char* label) {
  unsigned long now = millis();

  if (now - lastDebounceTime[index] > debounceDelay) {
    if (currentState != lastButtonState[index]) {
      lastDebounceTime[index] = now;
      lastButtonState[index] = currentState;

      if (currentState == LOW) {
        Serial.print("Button on ");
        Serial.print(label);
        Serial.println(" pressed");
        pcf8575.digitalWrite(ledPin, HIGH);
      } else {
        pcf8575.digitalWrite(ledPin, LOW);
      }
    }
  }
}

void loop() {
  // Your complex code runs here, uninterrupted, most of the time.

  if (buttonChanged) {
    buttonChanged = false;

    // Read all 16 pins at once
    PCF8575::DigitalInput di = pcf8575.digitalReadAll();

    bool button0 = di.p1;
    bool button1 = di.p2;
    bool button2 = di.p3;
    bool button3 = di.p4;

    handleButton(0, button0, P1, "P0");
    handleButton(1, button1, P2, "P1");
    handleButton(2, button2, P3, "P2");
    handleButton(3, button3, P4, "P3");
  }
}