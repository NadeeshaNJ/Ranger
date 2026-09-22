// Net: packets between Ranger devices over the Radio.
//   text  = reliable (ACK + up to 4 tries)      voice = fast, no ACK
//   every device relays packets for others (TTL 2), each packet only once
//   positions are sent every 20 s only while someone tracks you
//   encryption plugs in with setCipher() (Crypto module)
//
// Packet = 10-byte header + payload
//   [0] type  [1] flags  [2] network  [3-4] from  [5-6] to  [7-8] seq  [9] TTL (+0x80 if relayed)
#pragma once
#include <Arduino.h>
#include "Radio.h"

#define NET_ALL 0xFFFF

enum NetType : uint8_t { T_HELLO = 1, T_TEXT, T_ACK, T_VOICE, T_VOICE_END, T_POS, T_POS_REQ, T_PING, T_PONG, T_SOS };
enum NetEventType : uint8_t { EV_PEER, EV_TEXT, EV_SENT, EV_FAILED, EV_VOICE, EV_VOICE_END, EV_PONG, EV_SOS };

struct NetEvent {
  uint8_t  type;            // NetEventType
  uint16_t from;
  const uint8_t* data;      // text / voice frames (only valid inside the callback)
  uint8_t  len;
  float    rssi, snr;
  uint16_t msgId;           // EV_TEXT, EV_SENT, EV_FAILED
  bool     toAll;           // EV_TEXT: it was sent to everyone
  uint8_t  session, codec, frameBytes, frames;   // EV_VOICE
  uint16_t frameIndex;
  uint32_t rttMs;           // EV_PONG
};

struct Peer {
  uint16_t id;
  char     name[13];
  bool     named;           // false until its name has arrived
  float    rssi, snr;       // last packet heard directly
  bool     direct;          // false = only heard through a relay
  uint32_t heardMs;
  bool     hasPos;
  double   lat, lon;
};

// Encryption hook. aad = header bytes 0..8 (checked, not hidden).
class NetCipher {
public:
  virtual uint8_t overhead() = 0;
  virtual bool seal(const uint8_t* aad, const uint8_t* in, size_t len, uint8_t* out, size_t& outLen) = 0;
  virtual bool open(const uint8_t* aad, const uint8_t* in, size_t len, uint8_t* out, size_t& outLen) = 0;
};

class Net {
public:
  void begin(Radio& radio, const char* name, uint8_t network = 0x52);
  void update();                                   // call in every loop()
  void onEvent(void (*fn)(const NetEvent&)) { _cb = fn; }
  void setCipher(NetCipher* c) { _cipher = c; }
  uint16_t id() { return _id; }

  int  sendText(uint16_t to, const char* text);    // message id, or -1 while the last text is pending
  void sendVoice(uint16_t to, uint8_t session, uint16_t index, uint8_t codec,
                 const uint8_t* frames, uint8_t frameBytes, uint8_t count);
  void sendVoiceEnd(uint16_t to, uint8_t session);
  void ping(uint16_t to);
  void sendSos(const char* text);
  void track(uint16_t peer, bool on);              // ask a peer for its position every 20 s
  void sharePosition() { sendPos(); }              // send my position to everyone once
  void setPosition(bool fix, double lat, double lon);

  int   peerCount() { return _peerCount; }
  Peer* peer(int i) { return &_peers[i]; }
  Peer* findPeer(uint16_t id);
  static float distanceM(double lat1, double lon1, double lat2, double lon2);
  static float bearingDeg(double lat1, double lon1, double lat2, double lon2);

private:
  struct Out { bool used, voice; uint8_t buf[255], len; uint32_t at; };

  bool build(uint8_t type, uint16_t to, uint8_t ttl, const uint8_t* payload, size_t len, uint8_t* out, uint8_t& outLen);
  void send(uint8_t type, uint16_t to, const uint8_t* payload, size_t len, uint32_t delayMs = 0, int copies = 1);
  void queue(const uint8_t* buf, uint8_t len, uint32_t delayMs, bool voice);
  void sendNextFromQueue();
  void handle(const RadioPacket& p);
  void deliver(uint8_t type, uint16_t from, uint16_t to, const uint8_t* pl, size_t n, const RadioPacket& p);
  bool seenBefore(uint16_t from, uint16_t seq);
  bool textSeenBefore(uint16_t from, uint16_t msgId);
  void sendTextTry();
  void sendHello(bool askBack);
  void sendPos();
  void emit(NetEvent& e) { if (_cb) _cb(e); }

  Radio* _radio = nullptr;
  NetCipher* _cipher = nullptr;
  void (*_cb)(const NetEvent&) = nullptr;
  uint16_t _id = 0, _seq = 0, _msgId = 0;
  uint8_t  _net = 0;
  char     _name[13] = "";

  Peer _peers[8];
  int  _peerCount = 0;
  Out  _out[8] = {};
  uint32_t _seen[32] = {}, _textSeen[16] = {};    // (from << 16 | seq) of recent packets / texts
  uint8_t  _seenPos = 0, _textSeenPos = 0;

  struct { bool active; uint16_t to, id; char text[201]; uint8_t tries; uint32_t due; } _txt = {};
  bool     _fix = false;
  double   _lat = 0, _lon = 0;
  uint32_t _trackedUntil = 0, _nextPos = 0;        // someone is tracking me
  uint16_t _tracking = 0;                          // I am tracking this peer
  uint32_t _trackRefresh = 0, _nextNameCheck = 0;
  bool     _helloDue = false, _helloAsk = false;       // HELLOs are merged: at most one per 2 s
  uint32_t _lastHello = 0, _nextBootHello = 0;
  uint8_t  _bootHellos = 2;                            // extra announcements after power-on
  uint16_t _pingId = 0;
  uint32_t _pingAt = 0;
};
