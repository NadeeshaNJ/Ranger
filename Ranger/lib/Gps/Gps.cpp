#include "Gps.h"
#include <TinyGPS++.h>

#define GPS_RX_PIN 13                 // ESP32 receives on IO13 (from GPS TX)
#define GPS_TX_PIN 4                  // ESP32 sends on IO4 (to GPS RX)
#define UTC_OFFSET_MIN 330            // +5:30

static TinyGPSPlus nmea;              // turns the GPS text ("$GNRMC,...") into numbers

bool Gps::begin() {
  // SpeedyBee says 115200 baud, the BZGNSS manual says 38400: try both (and 9600)
  const uint32_t rates[] = {115200, 38400, 9600};
  for (uint32_t baud : rates) {
    Serial2.begin(baud, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    uint32_t good = nmea.passedChecksum(), start = millis();
    while (millis() - start < 1500) update();
    if (nmea.passedChecksum() > good) return true;   // valid sentences = right speed
    Serial2.end();
  }
  return false;
}

void Gps::update() {
  while (Serial2.available()) nmea.encode(Serial2.read());
}

bool   Gps::hasFix()   { return nmea.location.isValid() && nmea.location.age() < 3000; }
double Gps::lat()      { return nmea.location.lat(); }
double Gps::lon()      { return nmea.location.lng(); }
int    Gps::sats()     { return nmea.satellites.value(); }
float  Gps::speedKmh() { return nmea.speed.kmph(); }
float  Gps::course()   { return nmea.course.deg(); }

bool Gps::timeText(char out[6]) {
  if (!nmea.time.isValid()) return false;
  int m = (nmea.time.hour() * 60 + nmea.time.minute() + UTC_OFFSET_MIN) % 1440;
  snprintf(out, 6, "%02d:%02d", m / 60, m % 60);
  return true;
}

int Gps::bars() {
  if (!hasFix()) return 0;
  return sats() < 6 ? 1 : sats() < 10 ? 2 : 3;
}
