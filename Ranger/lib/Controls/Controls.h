// Controls: keypad + VOICE + knob push on the PCF8575 (I2C 0x20, SDA=IO21 SCL=IO22),
// knob turning on IO39/IO36 (hardware counter), amp on/off on expander P15.
#pragma once
#include <Arduino.h>

enum Key : uint8_t { KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,
                     KEY_VOICE, KEY_KNOB, KEY_COUNT, KEY_NONE = 0xFF };

struct InputEvent {
  uint8_t key;     // a Key, or KEY_NONE for knob turning
  bool    down;    // true = pressed, false = released
  int8_t  turn;    // knob clicks: + clockwise, - counter-clockwise (key = KEY_NONE)
};

class Controls {
public:
  bool begin();                      // also starts I2C (Wire) for the OLED
  bool poll(InputEvent& e);          // call often; true when there is an event
  bool isDown(uint8_t key) { return _stable & (1 << key); }
  void setAmp(bool on);              // speaker amp SD_MODE
private:
  void scan();
  void writePort();
  uint16_t _out = 0xFFFF, _stable = 0, _last = 0;
  int16_t  _encBase = 0;
  uint32_t _lastScan = 0;
  InputEvent _q[8];
  uint8_t _head = 0, _count = 0;
};
