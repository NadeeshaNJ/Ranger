#include "Net.h"

#define HDR       10
#define TTL_START 2
#define ACK_WAIT  3000    // ms to wait for an ACK before trying again

static void put16(uint8_t* p, uint16_t v) { p[0] = v; p[1] = v >> 8; }
static uint16_t get16(const uint8_t* p) { return p[0] | (p[1] << 8); }
static void put32(uint8_t* p, int32_t v) { memcpy(p, &v, 4); }
static int32_t get32(const uint8_t* p) { int32_t v; memcpy(&v, p, 4); return v; }

void Net::begin(Radio& radio, const char* name, uint8_t network) {
  _radio = &radio;
  _net = network;
  strncpy(_name, name, 12);
  uint64_t mac = ESP.getEfuseMac();                 // my ID comes from the chip's MAC address
  _id = mac ^ (mac >> 16) ^ (mac >> 32);
  if (_id == 0 || _id == NET_ALL) _id = 1;
  _seq = random(1, 60000);
  sendHello(true);
  _nextBootHello = millis() + random(8000, 16000);  // and twice more in the first minute
}

// ---------------- building and sending ----------------

bool Net::build(uint8_t type, uint16_t to, uint8_t ttl, const uint8_t* payload, size_t len,
                uint8_t* out, uint8_t& outLen) {
  out[0] = type;
  out[1] = _cipher ? 1 : 0;                         // flag: encrypted
  out[2] = _net;
  put16(out + 3, _id);
  put16(out + 5, to);
  put16(out + 7, ++_seq);                           // every packet (and every retry) gets a new number
  out[9] = ttl;
  size_t n = len;
  if (_cipher) { if (!_cipher->seal(out, payload, len, out + HDR, n)) return false; }
  else         { if (len > 245) return false; memcpy(out + HDR, payload, len); }
  outLen = HDR + n;
  return true;
}

void Net::send(uint8_t type, uint16_t to, const uint8_t* payload, size_t len, uint32_t delayMs, int copies) {
  uint8_t buf[255], n;
  uint8_t ttl = (type == T_VOICE || type == T_VOICE_END) ? 0 : TTL_START;   // voice is never relayed
  if (!build(type, to, ttl, payload, len, buf, n)) return;
  for (int i = 0; i < copies; i++) queue(buf, n, delayMs + i * 500, type == T_VOICE);
}

void Net::queue(const uint8_t* buf, uint8_t len, uint32_t delayMs, bool voice) {
  for (auto& o : _out) {
    if (o.used) continue;
    memcpy(o.buf, buf, len);
    o.len = len;
    o.at = millis() + delayMs;
    o.voice = voice;
    o.used = true;
    return;
  }
}

void Net::sendNextFromQueue() {
  if (_radio->busy()) return;                       // listen before talk
  Out* pick = nullptr;
  for (auto& o : _out) {
    if (!o.used) continue;
    if (o.voice && millis() - o.at > 200) { o.used = false; continue; }   // late voice is useless
    if ((int32_t)(millis() - o.at) < 0) continue;
    if (!pick || (o.voice && !pick->voice)) pick = &o;                      // voice goes first
  }
  if (pick && _radio->send(pick->buf, pick->len)) pick->used = false;
}

// ---------------- main loop ----------------

void Net::update() {
  RadioPacket p;
  while (_radio->receive(p)) handle(p);

  uint32_t now = millis();
  if (_txt.active && now >= _txt.due) {             // no ACK yet
    if (_txt.tries < 4) sendTextTry();
    else { _txt.active = false; NetEvent e = {}; e.type = EV_FAILED; e.from = _txt.to; e.msgId = _txt.id; emit(e); }
  }
  if (now < _trackedUntil && now >= _nextPos) { sendPos(); _nextPos = now + 20000; }
  if (_tracking && now >= _trackRefresh) track(_tracking, true);
  if (_helloDue && now - _lastHello >= 2000) {      // send the (merged) HELLO
    uint8_t pl[13];
    pl[0] = _helloAsk;
    memcpy(pl + 1, _name, strlen(_name));
    send(T_HELLO, NET_ALL, pl, 1 + strlen(_name), random(100, 1500));   // spread out: fewer clashes
    _helloDue = _helloAsk = false;
    _lastHello = now;
  }
  if (_bootHellos && now >= _nextBootHello) {       // after power-on: announce again (lost ones)
    sendHello(true);
    _bootHellos--;
    _nextBootHello = now + random(8000, 16000);
  }
  if (now >= _nextNameCheck) {                      // a name got lost? ask again every 10 s
    _nextNameCheck = now + 10000;
    for (int i = 0; i < _peerCount; i++) if (!_peers[i].named) { sendHello(true); break; }
  }
  sendNextFromQueue();
}

void Net::handle(const RadioPacket& p) {
  if (p.len < HDR || p.data[2] != _net) return;
  uint8_t  type = p.data[0];
  uint16_t from = get16(p.data + 3), to = get16(p.data + 5), seq = get16(p.data + 7);
  uint8_t  ttl = p.data[9] & 0x7F;
  bool     relayed = p.data[9] & 0x80;
  if (from == _id) return;

  // 1. With encryption, check the packet is genuine BEFORE trusting anything in it
  uint8_t plain[255];
  const uint8_t* pl = p.data + HDR;
  size_t n = p.len - HDR;
  if (_cipher) {
    size_t m = 0;
    if (!(p.data[1] & 1) || !_cipher->open(p.data, pl, n, plain, m)) return;  // fake, changed or replayed
    pl = plain;
    n = m;
  }

  // 2. Handle each packet once (relays make copies)
  if (seenBefore(from, seq)) return;

  // 3. Remember who sent it
  Peer* pr = findPeer(from);
  if (!pr && _peerCount < 8) {
    pr = &_peers[_peerCount++];
    *pr = {};
    pr->id = from;
    snprintf(pr->name, sizeof(pr->name), "%04X", from);   // until its name arrives
    sendHello(type != T_HELLO);                     // tell it my name (and ask for its name)
  }
  if (pr) {
    pr->heardMs = millis();
    pr->direct = !relayed;
    if (!relayed) { pr->rssi = p.rssi; pr->snr = p.snr; }
  }

  // 4. Use it, 5. pass it on for others
  if (to == _id || to == NET_ALL) deliver(type, from, to, pl, n, p);
  if (to != _id && ttl > 0) {
    uint8_t copy[255];
    memcpy(copy, p.data, p.len);
    copy[9] = (ttl - 1) | 0x80;                     // byte 9 is not part of the encryption check
    queue(copy, p.len, random(30, 150), false);
  }
}

void Net::deliver(uint8_t type, uint16_t from, uint16_t to, const uint8_t* pl, size_t n, const RadioPacket& p) {
  NetEvent e = {};
  e.from = from; e.rssi = p.rssi; e.snr = p.snr;
  Peer* pr = findPeer(from);

  switch (type) {
    case T_HELLO:                                   // [askBack][name]
      if (n < 1) break;
      if (pr) { size_t k = min(n - 1, (size_t)12); memcpy(pr->name, pl + 1, k); pr->name[k] = 0; pr->named = true; }
      if (pl[0]) sendHello(false);                  // it asked for my name
      e.type = EV_PEER; emit(e);
      break;

    case T_TEXT: {                                  // [msgId 2][text]
      if (n < 2) break;
      uint16_t id = get16(pl);
      if (to == _id) { uint8_t a[2]; put16(a, id); send(T_ACK, from, a, 2, random(10, 50)); }
      if (textSeenBefore(from, id)) break;          // a retry of a text already shown
      e.type = EV_TEXT; e.msgId = id; e.data = pl + 2; e.len = n - 2; e.toAll = to == NET_ALL; emit(e);
      break;
    }
    case T_ACK:
      if (to == _id && n >= 2 && _txt.active && get16(pl) == _txt.id) {
        _txt.active = false;
        e.type = EV_SENT; e.msgId = _txt.id; emit(e);
      }
      break;

    case T_VOICE:                                   // [session][index 2][codec][frameBytes][frames][data]
      if (n < 6 || (size_t)(6 + pl[4] * pl[5]) > n) break;
      e.type = EV_VOICE; e.session = pl[0]; e.frameIndex = get16(pl + 1); e.codec = pl[3];
      e.frameBytes = pl[4]; e.frames = pl[5]; e.data = pl + 6; e.len = pl[4] * pl[5];
      emit(e);
      break;

    case T_VOICE_END:
      e.type = EV_VOICE_END; e.session = n ? pl[0] : 0; emit(e);
      break;

    case T_POS:                                     // [fix][lat 4][lon 4]
    case T_SOS:                                     // same + text
      if (pr && n >= 9 && pl[0]) { pr->hasPos = true; pr->lat = get32(pl + 1) / 1e7; pr->lon = get32(pl + 5) / 1e7; }
      if (type == T_SOS) { e.type = EV_SOS; e.data = pl + 9; e.len = n > 9 ? n - 9 : 0; }
      else e.type = EV_PEER;
      emit(e);
      break;

    case T_POS_REQ: {                               // [seconds 2]; 0 = stop
      uint16_t sec = n >= 2 ? get16(pl) : 0;
      _trackedUntil = sec ? millis() + sec * 1000UL : 0;
      _nextPos = millis();                          // answer right away
      break;
    }
    case T_PING:
      send(T_PONG, from, pl, n, random(10, 50));    // echo the ping id back
      break;

    case T_PONG:
      if (n >= 2 && get16(pl) == _pingId) { e.type = EV_PONG; e.rttMs = millis() - _pingAt; emit(e); }
      break;
  }
}

bool Net::seenBefore(uint16_t from, uint16_t seq) {
  uint32_t key = ((uint32_t)from << 16) | seq;
  for (uint32_t k : _seen) if (k == key) return true;
  _seen[_seenPos++ % 32] = key;
  return false;
}

bool Net::textSeenBefore(uint16_t from, uint16_t msgId) {
  uint32_t key = ((uint32_t)from << 16) | msgId;
  for (uint32_t k : _textSeen) if (k == key) return true;
  _textSeen[_textSeenPos++ % 16] = key;
  return false;
}

// ---------------- sending things ----------------

int Net::sendText(uint16_t to, const char* text) {
  if (_txt.active) return -1;
  strncpy(_txt.text, text, 200);
  _txt.to = to;
  _txt.id = ++_msgId;
  _txt.tries = 0;
  if (to == NET_ALL) {                              // to everyone: no ACKs, just send it twice
    uint8_t pl[202];
    put16(pl, _txt.id);
    memcpy(pl + 2, _txt.text, strlen(_txt.text));
    send(T_TEXT, NET_ALL, pl, 2 + strlen(_txt.text), 0, 2);
    NetEvent e = {}; e.type = EV_SENT; e.msgId = _txt.id; emit(e);
  } else {
    _txt.active = true;
    sendTextTry();
  }
  return _txt.id;
}

void Net::sendTextTry() {
  uint8_t pl[202];
  put16(pl, _txt.id);
  memcpy(pl + 2, _txt.text, strlen(_txt.text));
  send(T_TEXT, _txt.to, pl, 2 + strlen(_txt.text));
  _txt.tries++;
  _txt.due = millis() + ACK_WAIT;
}

void Net::sendVoice(uint16_t to, uint8_t session, uint16_t index, uint8_t codec,
                    const uint8_t* frames, uint8_t frameBytes, uint8_t count) {
  uint8_t pl[6 + 48];
  if (frameBytes * count > 48) return;
  pl[0] = session; put16(pl + 1, index); pl[3] = codec; pl[4] = frameBytes; pl[5] = count;
  memcpy(pl + 6, frames, frameBytes * count);
  send(T_VOICE, to, pl, 6 + frameBytes * count);
}

void Net::sendVoiceEnd(uint16_t to, uint8_t session) { send(T_VOICE_END, to, &session, 1, 0, 2); }

void Net::sendHello(bool askBack) {                // sent from update(), max one per 2 s
  _helloDue = true;
  _helloAsk |= askBack;
}

void Net::ping(uint16_t to) {
  uint8_t pl[2];
  put16(pl, _pingId = ++_msgId);
  _pingAt = millis();
  send(T_PING, to, pl, 2);
}

void Net::setPosition(bool fix, double lat, double lon) { _fix = fix; _lat = lat; _lon = lon; }

void Net::sendPos() {
  uint8_t pl[9];
  pl[0] = _fix;
  put32(pl + 1, lround(_lat * 1e7));
  put32(pl + 5, lround(_lon * 1e7));
  send(T_POS, NET_ALL, pl, 9);
}

void Net::sendSos(const char* text) {
  uint8_t pl[9 + 60];
  size_t t = min(strlen(text), (size_t)60);
  pl[0] = _fix;
  put32(pl + 1, lround(_lat * 1e7));
  put32(pl + 5, lround(_lon * 1e7));
  memcpy(pl + 9, text, t);
  send(T_SOS, NET_ALL, pl, 9 + t, 0, 3);            // 3 copies, same packet: shown once
}

void Net::track(uint16_t peer, bool on) {
  uint8_t pl[2];
  put16(pl, on ? 300 : 0);                          // "send me your position for 5 minutes"
  send(T_POS_REQ, peer, pl, 2);
  _tracking = on ? peer : 0;
  _trackRefresh = millis() + 240000;                // ask again before it runs out
}

// ---------------- peers and maths ----------------

Peer* Net::findPeer(uint16_t id) {
  for (int i = 0; i < _peerCount; i++) if (_peers[i].id == id) return &_peers[i];
  return nullptr;
}

float Net::distanceM(double lat1, double lon1, double lat2, double lon2) {
  double r = PI / 180, a = sin((lat2 - lat1) * r / 2), b = sin((lon2 - lon1) * r / 2);
  double h = a * a + cos(lat1 * r) * cos(lat2 * r) * b * b;
  return 2 * 6371000.0 * atan2(sqrt(h), sqrt(1 - h));
}

float Net::bearingDeg(double lat1, double lon1, double lat2, double lon2) {
  double r = PI / 180;
  double y = sin((lon2 - lon1) * r) * cos(lat2 * r);
  double x = cos(lat1 * r) * sin(lat2 * r) - sin(lat1 * r) * cos(lat2 * r) * cos((lon2 - lon1) * r);
  double d = atan2(y, x) / r;
  return d < 0 ? d + 360 : d;
}
