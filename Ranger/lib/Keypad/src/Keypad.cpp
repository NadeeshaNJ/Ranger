#include "Keypad.h"

// The ISR only wakes the task; all I2C work happens outside interrupt context
// because a PCF8575 read is a bus transaction and must not run in an ISR.
static TaskHandle_t isrTaskHandle = nullptr;

static void IRAM_ATTR onPcfInterrupt() {
  if (isrTaskHandle != nullptr) {
    vTaskNotifyGiveFromISR(isrTaskHandle, NULL);
  }
}

bool Keypad::begin(const KeypadConfig &cfg) {
  config = cfg;

  pcf = new PCF8575(config.i2cAddress);
  if (pcf == nullptr) return false;

  // P0-P7 as inputs (buttons). The tested sketch also drove P10-P17 as LED
  // outputs; those are left alone here since this board has no LEDs on them.
  for (int i = 0; i < 8; i++) pcf->pinMode(i, INPUT);
  pcf->begin();

  events = xQueueCreate(8, sizeof(KeyEvent));
  if (events == NULL) return false;

  // Priority 1: below the audio capture task (2), above the Arduino loop. A
  // menu keypress losing a race with a Codec2 frame is the correct outcome.
  if (xTaskCreatePinnedToCore(taskEntry, "keypad", 3072, this, 1,
                              &taskHandle, 0) != pdPASS) {
    return false;
  }
  isrTaskHandle = taskHandle;

  pinMode(config.intPin, INPUT_PULLUP);   // INT is active low, open drain
  // CHANGE rather than FALLING so the release edge is seen too; the debounce
  // logic needs both transitions to track state correctly.
  attachInterrupt(digitalPinToInterrupt(config.intPin), onPcfInterrupt, CHANGE);

  return true;
}

void Keypad::taskEntry(void *param) {
  Keypad *self = (Keypad *)param;
  for (;;) {
    // Wake immediately on the INT edge, but re-sample every 20 ms regardless.
    // INT is cleared by reading the port and self-clears when the inputs
    // return to their previous value, so a short press that ends before the
    // read would otherwise leave no trace. The periodic read is the safety
    // net; the notification is what makes the common case fast.
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(20));
    self->serviceButtons();
  }
}

void Keypad::serviceButtons() {
  // One read gets all 16 pins and is also what clears the PCF8575 INT.
  PCF8575::DigitalInput di = pcf->digitalReadAll();

  // Index by expander pin so the config can remap keys without touching this.
  bool pinState[8] = {di.p0, di.p1, di.p2, di.p3, di.p4, di.p5, di.p6, di.p7};
  const int keyPins[5] = {config.upPin, config.downPin, config.leftPin,
                          config.rightPin, config.enterPin};

  unsigned long now = millis();

  for (int i = 0; i < 5; i++) {
    int pin = keyPins[i];
    if (pin < 0 || pin > 7) continue;
    bool state = pinState[pin];

    if (now - lastDebounceTime[i] <= config.debounceMs) continue;
    if (state == lastState[i]) continue;

    lastDebounceTime[i] = now;
    lastState[i] = state;

    // Buttons are active low: pressed pulls the expander pin to ground.
    // Only the press edge is reported; releases update state silently.
    if (state == LOW && enabled) {
      KeyEvent ev = (KeyEvent)(KEY_UP + i);
      // Drop rather than block if the consumer is behind. A stale menu press
      // replayed seconds later is worse than a lost one.
      xQueueSend(events, &ev, 0);
    }
  }
}

KeyEvent Keypad::read() {
  if (events == NULL) return KEY_NONE;
  KeyEvent ev;
  if (xQueueReceive(events, &ev, 0) != pdTRUE) return KEY_NONE;
  return ev;
}

void Keypad::flush() {
  if (events == NULL) return;
  KeyEvent discard;
  while (xQueueReceive(events, &discard, 0) == pdTRUE) { }
}
