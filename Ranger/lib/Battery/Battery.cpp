#include "Battery.h"

#define BAT_PIN 14

// LiPo voltage -> charge. 0% is set at 3.6 V, about where the 3.3 V regulator stops
// working. If your device switches off at a different voltage, change the last row.
static const float VOLTS[]   = {4.20, 4.10, 4.00, 3.90, 3.80, 3.70, 3.60};
static const int   PERCENT[] = { 100,   88,   74,   58,   40,   18,    0};

void Battery::begin() {
  analogSetPinAttenuation(BAT_PIN, ADC_11db);   // measures up to ~3.1 V (we see max 2.1 V)
}

float Battery::volts() {
  uint32_t mv = 0;
  for (int i = 0; i < 16; i++) mv += analogReadMilliVolts(BAT_PIN);   // calibrated ADC, averaged
  return mv / 16 * 2 / 1000.0;                                        // x2 undoes the divider
}

int Battery::percent() {
  float v = volts();
  if (v >= VOLTS[0]) return 100;
  for (int i = 1; i < 7; i++) {
    if (v >= VOLTS[i])   // straight line between the two table points around v
      return lround(PERCENT[i] + (v - VOLTS[i]) / (VOLTS[i - 1] - VOLTS[i]) * (PERCENT[i - 1] - PERCENT[i]));
  }
  return 0;
}
