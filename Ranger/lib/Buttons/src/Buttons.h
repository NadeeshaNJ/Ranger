/*
  Buttons -- debounced momentary input, used for push-to-talk.

  Note on pin choice: GPIO34-39 on the ESP32 are input-only and have NO
  internal pull-up. A button on one of those floats and reads as permanently
  pressed, so pick a pin that can actually pull up (GPIO12 works).
*/

#ifndef RANGER_BUTTONS_H
#define RANGER_BUTTONS_H

#include <Arduino.h>

class Button {
public:
  // activeLow wires the other leg to ground and uses the internal pull-up.
  void begin(int pin, unsigned long debounceMs = 50, bool activeLow = true);

  // Sample the pin. The debounce timer restarts on every observed change, so
  // the reported state only flips once the pin has held steady for the full
  // window; between bounces the last settled state is returned.
  //
  // Call this often -- it is a sampler, not an interrupt.
  bool isPressed();

  // True only on the sample where the button first went down / came up.
  // Both consume the edge, so call each at most once per loop iteration.
  bool wasPressed();
  bool wasReleased();

private:
  int pin = -1;
  unsigned long debounceMs = 50;
  bool activeLow = true;

  bool lastRaw = true;
  bool settled = true;
  bool lastReported = false;
  unsigned long lastChange = 0;
};

#endif  // RANGER_BUTTONS_H
