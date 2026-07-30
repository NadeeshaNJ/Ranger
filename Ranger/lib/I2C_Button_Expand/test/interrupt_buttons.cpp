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

const int INT_PIN = 4; // optional, connect to PCF8575 INT

const unsigned long debounceDelay = 50; // ms, tune per your buttons

// Per-button debounce tracking (P1 to P4)
unsigned long lastDebounceTime[4] = {0, 0, 0, 0};
bool lastButtonState[4] = {HIGH, HIGH, HIGH, HIGH};

// Buttons are serviced by their own task so long-running (even blocking) work
// in loop() cannot delay press detection.
TaskHandle_t buttonTaskHandle = nullptr;

void IRAM_ATTR onPcfInterrupt() {
  if (buttonTaskHandle != nullptr) {
    vTaskNotifyGiveFromISR(buttonTaskHandle, NULL);
  }
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

void servicePcfButtons() {
  // Read all 16 pins at once. This read is also what clears the PCF8575 INT.
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

void buttonTask(void *param) {
  for (;;) {
    // Wake immediately on the PCF8575 INT edge, but re-sample every 20 ms
    // regardless: INT is cleared by reading the port and self-clears when the
    // inputs return to their previous value, so a short press that ends before
    // we read would otherwise leave no trace. The periodic read is the safety
    // net, the notification is what makes the common case fast.
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(20));
    servicePcfButtons();
  }
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

  // Pinned to core 0 so heavy work on the Arduino core (1) never starves it.
  // Only this task touches the PCF8575, so no I2C locking is needed.
  xTaskCreatePinnedToCore(buttonTask, "buttons", 4096, NULL, 2,
                          &buttonTaskHandle, 0);

  pinMode(INT_PIN, INPUT_PULLUP); // active low interrupt from PCF8575
  // CHANGE rather than FALLING: catches the release edge too, so LEDs clear
  // promptly instead of waiting for the next periodic sample.
  attachInterrupt(digitalPinToInterrupt(INT_PIN), onPcfInterrupt, CHANGE);
}

void loop() {
  // Your complex code runs here. Blocking is now fine as far as buttons go.
  delay(10000);
}
