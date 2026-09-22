// Mic test: prints a sound level bar 10 times a second. Talk or tap near the mic.
#include <Arduino.h>
#include "Mic.h"

Mic mic;

void setup() {
  Serial.begin(115200);
  Serial.println(mic.begin() ? "Mic OK" : "Mic FAILED: no sound data (check wiring and 3.3 V)");
}

void loop() {
  static int16_t s[800];                       // 100 ms of sound
  mic.read(s, 800);
  int peak = 0;
  for (int i = 0; i < 800; i++) peak = max(peak, abs(s[i]));
  char bar[41];
  for (int i = 0; i < 40; i++) bar[i] = i < peak * 40 / 32768 ? '#' : ' ';
  bar[40] = 0;
  Serial.printf("%6d |%s|\n", peak, bar);
}
