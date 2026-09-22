#include "Controls.h"
#include <Wire.h>
#include <driver/pcnt.h>

#define PCF_ADDR 0x20
#define AMP_BIT  13          // P15 = Sound_ShutDown
#define ENC_A    39          // swap A and B if the knob turns the wrong way
#define ENC_B    36

// Expander bit for each key (P00..P07 = bit 0..7, P10..P17 = bit 8..15), from the schematic
static const uint8_t KEY_BIT[KEY_COUNT] = {
  3, 0, 6, 8, 1, 5, 9, 2, 4, 10,   // KEY_0 .. KEY_9
  15,                              // KEY_VOICE (P17)
  12                               // KEY_KNOB  (P14)
};

bool Controls::begin() {
  Wire.begin(21, 22, 400000);
  Wire.beginTransmission(PCF_ADDR);
  if (Wire.endTransmission() != 0) return false;
  writePort();                                     // all inputs HIGH, amp on

  // Knob: count every edge of A and B (x4), 2 counts per click
  pcnt_config_t c = {};
  c.unit = PCNT_UNIT_0;
  c.counter_h_lim = 30000;
  c.counter_l_lim = -30000;
  c.channel = PCNT_CHANNEL_0;  c.pulse_gpio_num = ENC_A;  c.ctrl_gpio_num = ENC_B;
  c.pos_mode = PCNT_COUNT_DEC; c.neg_mode = PCNT_COUNT_INC;
  c.lctrl_mode = PCNT_MODE_REVERSE; c.hctrl_mode = PCNT_MODE_KEEP;
  pcnt_unit_config(&c);
  c.channel = PCNT_CHANNEL_1;  c.pulse_gpio_num = ENC_B;  c.ctrl_gpio_num = ENC_A;
  c.pos_mode = PCNT_COUNT_INC; c.neg_mode = PCNT_COUNT_DEC;
  pcnt_unit_config(&c);
  pcnt_set_filter_value(PCNT_UNIT_0, 1023);        // ignore glitches under 12.8 us
  pcnt_filter_enable(PCNT_UNIT_0);
  pcnt_counter_clear(PCNT_UNIT_0);
  return true;
}

void Controls::writePort() {                       // inputs must be written HIGH on a PCF8575
  Wire.beginTransmission(PCF_ADDR);
  Wire.write(_out & 0xFF);
  Wire.write(_out >> 8);
  Wire.endTransmission();
}

void Controls::setAmp(bool on) {
  if (on) _out |= (1 << AMP_BIT); else _out &= ~(1 << AMP_BIT);
  writePort();
}

bool Controls::poll(InputEvent& e) {
  if (_count == 0 && millis() - _lastScan >= 10) { _lastScan = millis(); scan(); }
  if (_count == 0) return false;
  e = _q[_head];
  _head = (_head + 1) % 8;
  _count--;
  return true;
}

void Controls::scan() {
  auto push = [&](InputEvent ev) { if (_count < 8) _q[(_head + _count++) % 8] = ev; };

  // Keys: read both port bytes, LOW = pressed
  uint16_t port = 0xFFFF;
  if (Wire.requestFrom(PCF_ADDR, 2) == 2) {
    uint8_t lo = Wire.read();                        // P00..P07 first
    uint8_t hi = Wire.read();                        // then P10..P17
    port = lo | (hi << 8);
  }
  uint16_t down = 0;
  for (int k = 0; k < KEY_COUNT; k++) if (!(port & (1 << KEY_BIT[k]))) down |= (1 << k);

  // Debounce: accept a change only when two reads 10 ms apart agree
  if (down == _last) {
    uint16_t changed = down ^ _stable;
    for (int k = 0; k < KEY_COUNT; k++)
      if (changed & (1 << k)) push({(uint8_t)k, (bool)(down & (1 << k)), 0});
    _stable = down;
  }
  _last = down;

  // Knob
  int16_t count;
  pcnt_get_counter_value(PCNT_UNIT_0, &count);
  int clicks = (count - _encBase) / 2;
  if (clicks) {
    _encBase += clicks * 2;
    push({KEY_NONE, false, (int8_t)constrain(clicks, -100, 100)});
  }
  if (abs(_encBase) > 20000 && count == _encBase) { pcnt_counter_clear(PCNT_UNIT_0); _encBase = 0; }
}
