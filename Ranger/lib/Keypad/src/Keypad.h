/*
  Keypad -- five navigation buttons on a PCF8575 I2C expander.

  Ported from test/I2C_Button_Expand/test/interrupt_buttons.cpp. The debounce
  approach and the INT-plus-periodic-resample pattern are kept as tested; what
  changed is the button count (5 on P0-P4 rather than 4 on P1-P4), the removal
  of the LED writes, and delivery of presses through a queue instead of
  Serial.print.

  Why a queue rather than a polled "is pressed" call: the expander is read in
  its own task, and a short press can begin and end between two calls from the
  main loop. Queueing edges means a press is never missed just because the
  consumer was busy transmitting audio.

  IMPORTANT -- INT pin: the tested sketch used GPIO4, which on this board is
  the LoRa DIO0 line. It has been moved to GPIO13. Wire the PCF8575 INT pin to
  GPIO13, not GPIO4, or the radio and the keypad will fight over the same pin.
*/

#ifndef RANGER_KEYPAD_H
#define RANGER_KEYPAD_H

#include <Arduino.h>
#include <PCF8575.h>

enum KeyEvent {
  KEY_NONE = 0,
  KEY_UP,
  KEY_DOWN,
  KEY_LEFT,
  KEY_RIGHT,
  KEY_ENTER
};

struct KeypadConfig {
  uint8_t i2cAddress = 0x20;   // A0/A1/A2 to GND
  int intPin = 13;             // NOT 4 -- that is LoRa DIO0 on this board
  unsigned long debounceMs = 50;
  // Expander pins for each key, in KeyEvent order (up, down, left, right,
  // enter). Defaults to P0..P4 as mounted.
  int upPin = 0;
  int downPin = 1;
  int leftPin = 2;
  int rightPin = 3;
  int enterPin = 4;
};

class Keypad {
public:
  // Wire.begin() must already have been called -- the OLED shares the bus.
  // Starts a task pinned to core 0 that owns the expander; nothing else may
  // touch the PCF8575, which is what keeps the I2C access lock-free.
  bool begin(const KeypadConfig &cfg = KeypadConfig());

  // Pop one press. Returns KEY_NONE if nothing is waiting. Non-blocking.
  KeyEvent read();

  // Discard queued presses. Call when entering a mode that should not act on
  // buttons pushed while it was busy.
  void flush();

  // Buttons pressed while this is false are still debounced but discarded
  // rather than queued. Used to freeze the UI during audio without letting a
  // backlog build up and then replay all at once.
  void setEnabled(bool on) { enabled = on; }
  bool isEnabled() const { return enabled; }

private:
  static void taskEntry(void *param);
  void serviceButtons();

  KeypadConfig config;
  PCF8575 *pcf = nullptr;
  QueueHandle_t events = NULL;
  TaskHandle_t taskHandle = nullptr;
  volatile bool enabled = true;

  // Per-key debounce state, indexed by KeyEvent-1.
  unsigned long lastDebounceTime[5] = {0, 0, 0, 0, 0};
  bool lastState[5] = {HIGH, HIGH, HIGH, HIGH, HIGH};
};

#endif  // RANGER_KEYPAD_H
