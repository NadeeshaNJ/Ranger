// Codec: Codec2 voice compression (8 kHz speech <-> a few bytes per frame).
// !! Codec2 needs ~24 KB of stack: create and use it inside a task with a 32 KB stack.
#pragma once
#include <Arduino.h>

enum CodecMode : uint8_t { C3200, C2400, C1600, C1300, C1200, C700C };

class Codec {
public:
  bool begin(uint8_t mode = C3200);           // also used to change mode
  int  samples();                             // per frame: 160 (20 ms) or 320 (40 ms)
  int  bytes();                               // per frame: 8, 6, 8, 7, 6 or 4
  int  framesPerPacket();                     // how many frames Net should send together
  uint8_t mode() { return _mode; }
  void encode(const int16_t* pcm, uint8_t* bits);
  void decode(const uint8_t* bits, int16_t* pcm);
  void conceal(int16_t* pcm);                 // a frame was lost: fill the gap
private:
  struct CODEC2* _c2 = nullptr;
  uint8_t _mode = C3200;
  uint8_t _last[8];
  bool    _haveLast = false;
};
