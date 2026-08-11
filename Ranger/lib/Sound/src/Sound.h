/*
  Sound -- speaker output on I2S, plus a test tone generator.

  Owns one I2S peripheral in TX mode and everything that writes to it. The
  caller hands over mono 16-bit PCM; this duplicates it to stereo, applies
  gain, and pushes it to the DAC.

  The speaker is started and stopped explicitly rather than left running. An
  idle I2S TX peripheral keeps clocking whatever is in its DMA ring, so a
  receiver that stops getting packets would otherwise repeat the last frame
  forever. See Sound::stop().
*/

#ifndef RANGER_SOUND_H
#define RANGER_SOUND_H

#include <Arduino.h>
#include <driver/i2s.h>

// Pins have no sensible default and must be given. The rest default to what a
// Codec2 voice link wants, so a caller that agrees with those only writes the
// three pins. dmaBufLen must be >= one audio frame: a ring smaller than a
// single write blocks partway through every frame and chops the audio.
//     SoundConfig cfg(27, 26, 15);
struct SoundConfig {
  int bclkPin;
  int lrcPin;
  int dinPin;
  uint32_t sampleRate;
  i2s_port_t port;
  int dmaBufCount;
  int dmaBufLen;

  SoundConfig(int bclk = -1, int lrc = -1, int din = -1,
              uint32_t rate = 8000, i2s_port_t p = I2S_NUM_1,
              int bufCount = 6, int bufLen = 320)
    : bclkPin(bclk), lrcPin(lrc), dinPin(din), sampleRate(rate), port(p),
      dmaBufCount(bufCount), dmaBufLen(bufLen) {}
};

class Sound {
public:
  // Installs the I2S driver but leaves the peripheral stopped, so nothing is
  // clocked out until start() is called. Returns false if the driver refused
  // the configuration.
  bool begin(const SoundConfig &cfg);

  // Start/stop the DAC. Both are idempotent -- calling start() on a running
  // peripheral (or stop() on a stopped one) is a no-op rather than an error,
  // which matters because the PTT handler and the receive squelch both drive
  // this and neither knows what the other did.
  void start();
  void stop();
  bool isRunning() const { return running; }

  // Write one mono frame. Samples are duplicated to both channels and scaled
  // by gain, with a clamp so the multiply cannot wrap. Blocks until the DMA
  // ring accepts the data or timeoutMs elapses.
  // Returns the number of bytes actually written.
  size_t playMono(const int16_t *samples, int count, int gain, uint32_t timeoutMs = 200);

  // Zero the DMA ring. Call before stop() so the DAC is not left holding the
  // tail of the last frame, and after start() so playback does not begin with
  // stale audio.
  void zeroBuffer();

  // Fill dst with a continuous sine wave at toneHz.
  //
  // Phase is retained between calls, which is the whole point: restarting the
  // sine at zero each frame puts a discontinuity at every frame boundary, and
  // that click is itself an artefact that would confuse the diagnosis this
  // generator exists to support.
  void fillTone(int16_t *dst, int count, float toneHz, int16_t amplitude);

private:
  SoundConfig config = {};
  bool installed = false;
  bool running = false;
  float tonePhase = 0.0f;

  // Scratch for stereo interleaving. Sized at construction from dmaBufLen.
  int16_t *stereoBuf = nullptr;
  int stereoCapacity = 0;
};

#endif  // RANGER_SOUND_H
