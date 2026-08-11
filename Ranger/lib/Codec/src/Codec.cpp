#include "Codec.h"

bool Codec::begin(int mode) {
  state = codec2_create(mode);
  if (state == nullptr) return false;

  spf = codec2_samples_per_frame(state);
  bpf = (codec2_bits_per_frame(state) + 7) / 8;
  return true;
}

void Codec::end() {
  if (state != nullptr) {
    codec2_destroy(state);
    state = nullptr;
  }
}

void Codec::encode(uint8_t *bitsOut, const int16_t *samplesIn) {
  if (state == nullptr) return;
  // codec2_encode takes a non-const short*; it does not modify the input.
  codec2_encode(state, bitsOut, (short *)samplesIn);
}

void Codec::decode(int16_t *samplesOut, const uint8_t *bitsIn) {
  if (state == nullptr) return;
  codec2_decode(state, samplesOut, (const unsigned char *)bitsIn);
}
