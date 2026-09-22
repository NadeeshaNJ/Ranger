// Battery test: prints the battery voltage and charge every second.
#include <Arduino.h>
#include "Battery.h"

Battery battery;

void setup() {
  Serial.begin(115200);
  battery.begin();
}

void loop() {
  Serial.printf("%.2f V  %d%%\n", battery.volts(), battery.percent());
  delay(1000);
}
