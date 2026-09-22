#include "Radio.h"
#include <SPI.h>
#include <RadioLib.h>

static SX1280 radio = new Module(5, 35, 15, 34);    // NSS, DIO1, RST, BUSY
static volatile bool irq = false;                    // DIO1: "sent" or "received"
static bool sending = false;

static void IRAM_ATTR onDio1() { irq = true; }

bool Radio::begin(uint8_t channel) {
  SPI.begin(18, 19, 23, 5);
  // SF9, BW 812.5 kHz, CR 4/5, sync 0x12, chip power 0 dBm (= 27 dBm after the E28 amplifier;
  // more only wastes current), preamble 12
  if (radio.begin(channelMHz(channel), 812.5, 9, 5, 0x12, 0, 12) != RADIOLIB_ERR_NONE) return false;
  radio.setRfSwitchPins(2, 12);                      // after begin (as in RadioLib's examples)
  radio.setHighSensitivityMode(true);
  radio.setDio1Action(onDio1);
  radio.startReceive();
  return true;
}

bool Radio::send(const uint8_t* data, uint8_t len) {
  if (busy()) return false;
  irq = false;
  sending = radio.startTransmit(data, len) == RADIOLIB_ERR_NONE;
  if (!sending) radio.startReceive();
  return sending;
}

bool Radio::receive(RadioPacket& p) {
  if (!irq) return false;
  irq = false;
  if (sending) {                                     // our packet has gone out: listen again
    sending = false;
    radio.finishTransmit();
    radio.startReceive();
    return false;
  }
  p.len = radio.getPacketLength();
  bool ok = radio.readData(p.data, p.len) == RADIOLIB_ERR_NONE;   // CRC checked here
  p.rssi = radio.getRSSI();
  p.snr = radio.getSNR();
  radio.startReceive();
  return ok;
}

bool Radio::busy() {
  if (sending) return true;
  static uint32_t since = 0;
  uint16_t flags = radio.getIrqStatus();             // header received = someone is sending to us
  bool arriving = (flags & RADIOLIB_SX128X_IRQ_HEADER_VALID) && !(flags & RADIOLIB_SX128X_IRQ_RX_DONE);
  if (!arriving) { since = 0; return false; }
  if (!since) since = millis();
  if (millis() - since > 300) { radio.startReceive(); since = 0; return false; }   // never stuck
  return true;
}

void Radio::setChannel(uint8_t ch) {
  radio.standby();
  radio.setFrequency(channelMHz(min(ch, (uint8_t)15)));
  radio.startReceive();
}
