// Voice: push-to-talk.  talk:   Mic -> Codec2 -> Net
//                       listen: Net -> jitter buffer -> Codec2 -> Speaker
// Audio runs in its own task (32 KB stack, core 0). Net is only used from loop().
#pragma once
#include <Arduino.h>
#include "Mic.h"
#include "Speaker.h"
#include "Codec.h"
#include "Net.h"
#include "JitterBuffer.h"

class Voice {
public:
  bool begin(Mic& mic, Speaker& spk, Net& net, uint8_t codecMode = C3200);
  void update();                        // in loop(), right after net.update()
  void onNetEvent(const NetEvent& e);   // forward all Net events here
  void talk(bool on, uint16_t to = NET_ALL);
  bool talking()   { return _talking; }
  bool listening() { return _listening; }
  uint16_t talker() { return _from; }   // who we are listening to

  struct Packet { bool end; uint16_t peer; uint8_t session, codec, frameBytes, frames; uint16_t index; uint8_t data[48]; };

private:
  static void task(void* self);
  void loop();
  void doTalk();
  void doListen();

  Mic* _mic; Speaker* _spk; Net* _net;
  Codec _tx, _rx;
  uint8_t _txMode;
  JitterBuffer _jb;
  QueueHandle_t _toNet, _fromNet;
  volatile bool _talking = false, _listening = false, _wantTalk = false;
  uint16_t _to = NET_ALL, _from = 0, _index = 0;
  uint8_t  _session = 0, _rxSession = 0;
  bool     _buffering = false, _ended = false;
  uint32_t _lastRx = 0;
  Packet   _pkt;                        // frames being collected for the next packet
};
