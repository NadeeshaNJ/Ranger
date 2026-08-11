#include "Buttons.h"

void Button::begin(int p, unsigned long debounce, bool active_low) {
  pin = p;
  debounceMs = debounce;
  activeLow = active_low;

  pinMode(pin, activeLow ? INPUT_PULLUP : INPUT);

  // Seed the debounce state from the idle level so the first isPressed() does
  // not report a spurious edge.
  lastRaw = activeLow ? HIGH : LOW;
  settled = lastRaw;
  lastReported = false;
  lastChange = millis();
}

bool Button::isPressed() {
  if (pin < 0) return false;

  bool raw = digitalRead(pin);
  unsigned long now = millis();

  if (raw != lastRaw) {
    lastRaw = raw;
    lastChange = now;            // still bouncing, restart the window
  } else if (now - lastChange > debounceMs) {
    settled = raw;               // held steady long enough, accept it
  }

  return activeLow ? (settled == LOW) : (settled == HIGH);
}

bool Button::wasPressed() {
  bool now = isPressed();
  bool edge = now && !lastReported;
  lastReported = now;
  return edge;
}

bool Button::wasReleased() {
  bool now = isPressed();
  bool edge = !now && lastReported;
  lastReported = now;
  return edge;
}
