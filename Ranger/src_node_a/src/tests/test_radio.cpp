// Radio test (flash on 2 boards): each board sends "hello N" every 2 s and
// prints what it hears with the signal strength.   c = next channel (do it on both)
#include <Arduino.h>
#include "Radio.h"

Radio radio;
uint8_t channel = 9;

void setup() {
  Serial.begin(115200);
  Serial.println(radio.begin(channel) ? "Radio OK, 2450 MHz" : "Radio FAILED (check SPI wiring / 3.3 V)");
}

void loop() {
  static uint32_t last = 0, n = 0;
  if (millis() - last > 2000 + random(0, 300)) {           // a little random so boards don't collide
    last = millis();
    char msg[32];
    snprintf(msg, sizeof(msg), "hello %lu", (unsigned long)n++);
    Serial.printf("sent: %s (%s)\n", msg, radio.send((uint8_t*)msg, strlen(msg)) ? "ok" : "busy");
  }
  RadioPacket p;
  if (radio.receive(p)) {
    p.data[p.len] = 0;
    Serial.printf("heard: %s   RSSI %.0f dBm  SNR %.1f dB\n", (char*)p.data, p.rssi, p.snr);
  }
  if (Serial.available() && Serial.read() == 'c') {
    channel = (channel + 1) % 16;
    radio.setChannel(channel);
    Serial.printf("channel %d = %.0f MHz\n", channel, Radio::channelMHz(channel));
  }
}
