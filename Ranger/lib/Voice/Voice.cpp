#include "Voice.h"

#define PREBUFFER_MS 200      // collect this much before playing (smooths uneven arrival)
#define END_AFTER_MS 600      // silence this long = talk is over (if VOICE_END was lost)

static SemaphoreHandle_t ready;

bool Voice::begin(Mic& mic, Speaker& spk, Net& net, uint8_t codecMode) {
  _mic = &mic; _spk = &spk; _net = &net; _txMode = codecMode;
  _toNet = xQueueCreate(4, sizeof(Packet));
  _fromNet = xQueueCreate(8, sizeof(Packet));
  ready = xSemaphoreCreateBinary();
  xTaskCreatePinnedToCore(task, "voice", 32768, this, 5, NULL, 0);
  return xSemaphoreTake(ready, 3000) == pdTRUE;       // task has created the codecs
}

// ---------------- loop() side: only place that touches Net ----------------

void Voice::update() {
  Packet p;
  while (xQueueReceive(_toNet, &p, 0)) {
    if (p.end) _net->sendVoiceEnd(p.peer, p.session);
    else _net->sendVoice(p.peer, p.session, p.index, p.codec, p.data, p.frameBytes, p.frames);
  }
}

void Voice::onNetEvent(const NetEvent& e) {
  if (e.type != EV_VOICE && e.type != EV_VOICE_END) return;
  if (e.len > 48) return;
  Packet p = {};
  p.end = e.type == EV_VOICE_END;
  p.peer = e.from; p.session = e.session; p.codec = e.codec;
  p.frameBytes = e.frameBytes; p.frames = e.frames; p.index = e.frameIndex;
  memcpy(p.data, e.data, e.len);
  xQueueSend(_fromNet, &p, 0);
}

void Voice::talk(bool on, uint16_t to) { _to = to; _wantTalk = on; }

// ---------------- voice task ----------------

void Voice::task(void* self) { ((Voice*)self)->loop(); }

void Voice::loop() {
  _tx.begin(_txMode);                                  // Codec2 needs this task's big stack
  _rx.begin(_txMode);
  _mic->sleep();
  xSemaphoreGive(ready);

  for (;;) {
    if (_wantTalk && !_talking) {                      // PTT pressed (wins over listening)
      _listening = false;
      _session++; _index = 0; _pkt.frames = 0;
      _mic->wake();
      _talking = true;
    }
    if (!_wantTalk && _talking) {                      // PTT released: send the rest + END
      if (_pkt.frames) xQueueSend(_toNet, &_pkt, 50);
      Packet end = {}; end.end = true; end.peer = _to; end.session = _session;
      xQueueSend(_toNet, &end, 50);
      _mic->sleep();
      _talking = false;
    }

    if (_talking) doTalk();
    else doListen();
  }
}

void Voice::doTalk() {
  static int16_t pcm[320];
  int n = _tx.samples(), fb = _tx.bytes();
  _mic->read(pcm, n);                                  // waits 20 or 40 ms
  if (_pkt.frames == 0) {
    _pkt = {};
    _pkt.peer = _to; _pkt.session = _session; _pkt.codec = _tx.mode();
    _pkt.frameBytes = fb; _pkt.index = _index;
  }
  _tx.encode(pcm, _pkt.data + _pkt.frames * fb);
  _pkt.frames++;
  _index++;
  if (_pkt.frames == _tx.framesPerPacket()) {
    xQueueSend(_toNet, &_pkt, 0);
    _pkt.frames = 0;
  }
  Packet ignore;
  while (xQueueReceive(_fromNet, &ignore, 0)) {}       // can't listen while talking
}

void Voice::doListen() {
  Packet p;
  while (xQueueReceive(_fromNet, &p, _listening ? 0 : 20)) {
    if (!_listening && !p.end) {                       // someone starts talking
      _rx.begin(p.codec);                              // use the sender's codec mode
      if (_rx.bytes() != p.frameBytes) continue;
      _from = p.peer; _rxSession = p.session;
      _jb.start(p.index);
      _buffering = true; _ended = false;
      _listening = true;
    }
    if (p.peer != _from || p.session != _rxSession) continue;   // first talker wins
    if (p.end) { _ended = true; continue; }
    for (int k = 0; k < p.frames; k++) _jb.put(p.index + k, p.data + k * p.frameBytes, p.frameBytes);
    _lastRx = millis();
  }
  if (!_listening) return;

  int waiting = _jb.waiting();
  bool silent = millis() - _lastRx > END_AFTER_MS;
  if (waiting == 0 && (_ended || silent)) { _listening = false; return; }        // talk over
  if (_buffering) {
    if (waiting * _rx.samples() / 8 < PREBUFFER_MS && !_ended && !silent) { delay(10); return; }
    _buffering = false;
  }

  static int16_t pcm[320];
  uint8_t bits[8];
  if (waiting == 0) { _rx.conceal(pcm); _buffering = true; }   // ran dry: fade, buffer again
  else if (_jb.take(bits, _rx.bytes())) _rx.decode(bits, pcm);
  else _rx.conceal(pcm);                                       // this frame was lost
  _spk->play(pcm, _rx.samples());                              // waits for the speaker: keeps time
}
