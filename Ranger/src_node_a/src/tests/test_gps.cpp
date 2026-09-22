// GPS test: prints position, satellites and time every second. Go outside for a fix.
#include <Arduino.h>
#include "Gps.h"

Gps gps;

void setup() {
  Serial.begin(115200);
  Serial.println(gps.begin() ? "GPS found" : "GPS NOT found (check 5 V, TX->IO13, RX<-IO4)");
}

void loop() {
  gps.update();
  static uint32_t last = 0;
  if (millis() - last < 1000) return;
  last = millis();
  char t[6] = "--:--";
  gps.timeText(t);
  if (gps.hasFix())
    Serial.printf("%s  %.6f, %.6f  sats %d  %.1f km/h  course %.0f\n", t, gps.lat(), gps.lon(), gps.sats(), gps.speedKmh(), gps.course());
  else
    Serial.printf("%s  no fix yet, %d satellites\n", t, gps.sats());
}
