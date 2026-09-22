#include "Codec.h"
#include <codec2.h>

static const int MODES[] = { CODEC2_MODE_3200, CODEC2_MODE_2400, CODEC2_MODE_1600,
                             CODEC2_MODE_1300, CODEC2_MODE_1200, CODEC2_MODE_700C };
// Frames per packet: keeps radio airtime near 60% on SF9/BW812 with encryption
static const uint8_t PER_PACKET[] = { 5, 5, 3, 3, 3, 3 };

bool Codec::begin(uint8_t mode) {
  if (_c2) codec2_destroy(_c2);
  _mode = mode > C700C ? (uint8_t)C3200 : mode;
  _c2 = codec2_create(MODES[_mode]);
  _haveLast = false;
  return _c2 != nullptr;
}

int Codec::samples()         { return codec2_samples_per_frame(_c2); }
int Codec::bytes()           { return codec2_bytes_per_frame(_c2); }
int Codec::framesPerPacket() { return PER_PACKET[_mode]; }

void Codec::encode(const int16_t* pcm, uint8_t* bits) {
  codec2_encode(_c2, bits, (short*)pcm);             // Codec2 only reads the samples
}

void Codec::decode(const uint8_t* bits, int16_t* pcm) {
  codec2_decode(_c2, pcm, bits);
  memcpy(_last, bits, bytes());
  _haveLast = true;
}

void Codec::conceal(int16_t* pcm) {
  // Repeat the last frame once at half volume (softer than a click), then silence
  if (_haveLast) {
    codec2_decode(_c2, pcm, _last);
    for (int i = 0; i < samples(); i++) pcm[i] /= 2;
    _haveLast = false;
  } else {
    memset(pcm, 0, samples() * 2);
  }
}
