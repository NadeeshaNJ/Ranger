/*
  Radio -- SX1276/SX1278 LoRa link for voice packets.

  Thin wrapper over the Arduino LoRa library. Exists mainly so the modem
  settings live in one place with the reasoning attached, and so main.cpp is
  not littered with LoRa.* calls.

  Airtime is the constraint that shapes everything upstream. At SF7/BW125 a
  28-byte packet costs roughly 36 ms; the preamble is fixed but payload bytes
  add real time, so batching frames helps without being free. Running near 23%
  duty cycle at 4 frames per packet leaves room to trade data rate for range
  (roughly 2.5 dB per SF step).
*/

#ifndef RANGER_RADIO_H
#define RANGER_RADIO_H

#include <Arduino.h>
#include <LoRa.h>

// Pins must be given. The modem defaults are the 433 MHz voice settings
// described above: SF7/BW125 carries 4 Codec2 frames per packet at roughly
// 23% duty cycle. Raise spreadingFactor to trade data rate for range.
//     RadioConfig cfg(5, 14, 4);
struct RadioConfig {
  int ssPin;
  int rstPin;
  int dio0Pin;
  long frequencyHz;
  uint8_t syncWord;
  int txPower;
  int spreadingFactor;
  long bandwidthHz;
  bool enableCrc;

  RadioConfig(int ss = -1, int rst = -1, int dio0 = -1,
              long freq = 433E6, uint8_t sync = 0xF3, int power = 20,
              int sf = 7, long bw = 125E3, bool crc = true)
    : ssPin(ss), rstPin(rst), dio0Pin(dio0), frequencyHz(freq),
      syncWord(sync), txPower(power), spreadingFactor(sf),
      bandwidthHz(bw), enableCrc(crc) {}
};

class Radio {
public:
  // Blocks until the modem answers. Returns false only if it never does within
  // attempts, so a wiring fault surfaces as a boot failure rather than silence.
  bool begin(const RadioConfig &cfg, int attempts = 20);

  // Send one packet. Blocking; returns false if the modem rejected it.
  bool sendPacket(const uint8_t *data, int len);

  // Non-blocking poll. Returns bytes received into buf (0 if no packet).
  // Link quality for the packet just read is captured before returning, since
  // the next parsePacket() overwrites it.
  int receivePacket(uint8_t *buf, int maxLen);

  int lastRssi() const { return rssi; }
  float lastSnr() const { return snr; }

private:
  int rssi = 0;
  float snr = 0.0f;
};

#endif  // RANGER_RADIO_H
