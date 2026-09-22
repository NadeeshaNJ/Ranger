// Radio: E28-2G4M27SX (SX1280 + amplifier), LoRa at 2.4 GHz, via RadioLib.
// SPI: SCK=IO18 MISO=IO19 MOSI=IO23 NSS=IO5 | BUSY=IO34 DIO1=IO35 RST=IO15 | RX_EN=IO2 TX_EN=IO12
#pragma once
#include <Arduino.h>

struct RadioPacket {
  uint8_t data[255];
  uint8_t len;
  float   rssi, snr;
};

class Radio {
public:
  bool begin(uint8_t channel = 9);            // channel 0..15 = 2405..2480 MHz (9 = 2450)
  bool send(const uint8_t* data, uint8_t len); // starts sending; false if still busy
  bool receive(RadioPacket& p);               // true when a packet has arrived
  bool busy();                                // sending, or a packet is arriving right now
  void setChannel(uint8_t ch);
  static float channelMHz(uint8_t ch) { return 2405 + 5 * ch; }
};
