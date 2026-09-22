// JitterBuffer: packets arrive at uneven times; the speaker needs one frame every
// 20/40 ms. Frames are stored by their number and taken out in order.
#pragma once
#include <stdint.h>
#include <string.h>

class JitterBuffer {
public:
  static const int SLOTS = 32;                       // 640 ms of 20 ms frames

  void start(uint16_t first) { memset(_have, 0, sizeof(_have)); _next = _newest = first; _any = false; }

  void put(uint16_t index, const uint8_t* bits, int len) {
    int16_t ahead = index - _next;                   // signed: works when the number wraps
    if (ahead < 0 || ahead >= SLOTS) return;         // too late, or too far ahead
    int s = index % SLOTS;
    memcpy(_bits[s], bits, len);
    _tag[s] = index;
    _have[s] = true;
    if (!_any || (int16_t)(index - _newest) > 0) _newest = index;
    _any = true;
  }

  // Next frame in order: true + bits if it arrived, false if it is missing. Always moves on.
  bool take(uint8_t* bits, int len) {
    int s = _next % SLOTS;
    bool ok = _have[s] && _tag[s] == _next;
    if (ok) memcpy(bits, _bits[s], len);
    _have[s] = false;
    _next++;
    return ok;
  }

  int waiting() {                                    // frames ready to play
    int n = (int16_t)(_newest - _next) + 1;
    return _any && n > 0 ? n : 0;
  }

private:
  uint8_t  _bits[SLOTS][8];
  uint16_t _tag[SLOTS];
  bool     _have[SLOTS] = {};
  uint16_t _next = 0, _newest = 0;
  bool     _any = false;
};
