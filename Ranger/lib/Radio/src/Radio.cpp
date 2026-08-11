#include "Radio.h"

bool Radio::begin(const RadioConfig &cfg, int attempts) {
  LoRa.setPins(cfg.ssPin, cfg.rstPin, cfg.dio0Pin);

  bool up = false;
  for (int i = 0; i < attempts; i++) {
    if (LoRa.begin(cfg.frequencyHz)) { up = true; break; }
    delay(500);
  }
  if (!up) return false;

  LoRa.setSyncWord(cfg.syncWord);
  LoRa.setTxPower(cfg.txPower, PA_OUTPUT_PA_BOOST_PIN);
  LoRa.setSpreadingFactor(cfg.spreadingFactor);
  LoRa.setSignalBandwidth(cfg.bandwidthHz);
  if (cfg.enableCrc) LoRa.enableCrc(); else LoRa.disableCrc();

  return true;
}

bool Radio::sendPacket(const uint8_t *data, int len) {
  if (len <= 0) return false;
  if (LoRa.beginPacket() == 0) return false;
  LoRa.write(data, len);
  return LoRa.endPacket() == 1;
}

int Radio::receivePacket(uint8_t *buf, int maxLen) {
  int packetSize = LoRa.parsePacket();
  if (packetSize == 0) return 0;

  int read = 0;
  while (LoRa.available() && read < maxLen) {
    buf[read++] = (uint8_t)LoRa.read();
  }

  // Sample link quality now; the next parsePacket() overwrites these.
  rssi = LoRa.packetRssi();
  snr = LoRa.packetSnr();

  return read;
}
