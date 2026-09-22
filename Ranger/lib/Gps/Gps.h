// Gps: SpeedyBee BZ-251 (u-blox M10), NMEA over UART2.  GPS TX -> IO13, GPS RX <- IO4. Needs 5 V.
#pragma once
#include <Arduino.h>

class Gps {
public:
  bool begin();                 // finds the baud rate by itself; false if no GPS data arrives
  void update();                // call often (reads what the GPS has sent)
  bool hasFix();                // true = position is valid and fresh
  double lat();
  double lon();
  int   sats();
  float speedKmh();
  float course();               // direction of travel in degrees (only meaningful while moving)
  bool  timeText(char out[6]);  // local time "HH:MM" (UTC + 5:30, Sri Lanka)
  int   bars();                 // 0..3 bars for the status bar (0 = no fix)
};
