/*
  Codec -- Codec2 vocoder wrapper.

  One instance owns one CODEC2 handle. That is deliberate and load-bearing: a
  CODEC2 handle carries mutable per-frame state (pitch and energy history, LSP
  predictor memory) that both encode and decode write through. Sharing a single
  handle between an encoding task and a decoding task corrupts it and crashes.

  So create two: one for the capture side, one for the playback side. They are
  never needed for the same audio anyway, since the radio is half duplex.

  Note on stack: Codec2 does NOT keep its FFT working buffers inside the
  handle. analyse_one_frame() alone puts ~12 kB on the caller's stack and calls
  nlp() which adds ~8 kB; the decode chain reaches ~26 kB. Any task calling
  these needs a stack sized accordingly -- see the constants below.
*/

#ifndef RANGER_CODEC_H
#define RANGER_CODEC_H

#include <Arduino.h>
#include <codec2.h>

// Minimum task stack sizes for callers, measured from the built firmware's
// `entry` instructions rather than estimated:
//   encode: analyse_one_frame 12352 B -> nlp 8256 B          = ~20.6 kB
//   decode: codec2_decode_1300 7232 B -> aks_to_M2 8272 B
//           -> lpc_post_filter 10288 B                       = ~25.8 kB
// These add headroom on top of those peaks.
#define CODEC_ENCODE_STACK  32768
#define CODEC_DECODE_STACK  40960

class Codec {
public:
  // mode is a CODEC2_MODE_* constant. Returns false if allocation failed.
  bool begin(int mode);
  void end();

  // Geometry of the configured mode, valid after begin().
  int samplesPerFrame() const { return spf; }
  int bytesPerFrame() const { return bpf; }

  // Verify the mode matches what the caller has sized its buffers for. Cheap
  // insurance against a mode change silently overrunning fixed arrays.
  bool geometryMatches(int expectSamples, int expectBytes) const {
    return spf == expectSamples && bpf == expectBytes;
  }

  // One frame in each direction. Call these from a task with the stack
  // headroom noted above.
  void encode(uint8_t *bitsOut, const int16_t *samplesIn);
  void decode(int16_t *samplesOut, const uint8_t *bitsIn);

private:
  struct CODEC2 *state = nullptr;
  int spf = 0;
  int bpf = 0;
};

#endif  // RANGER_CODEC_H
