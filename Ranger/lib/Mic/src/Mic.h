/*
  Mic -- INMP441 (or any I2S MEMS mic) capture at a fixed sample rate.

  Owns one I2S peripheral in RX mode. Hands back mono 16-bit PCM ready for a
  vocoder, doing the 32-bit-slot to 16-bit conversion and the stereo-to-mono
  pick internally.

  The INMP441 is a 24-bit device in a 32-bit slot, left-justified, and with
  L/R tied low it speaks only in the left slot. Getting either the shift or
  the slot wrong yields plausible-looking numbers that are actually noise, so
  both are configurable and both are documented at their definitions.
*/

#ifndef RANGER_MIC_H
#define RANGER_MIC_H

#include <Arduino.h>
#include <driver/i2s.h>

// Pins must be given; the rest default to what a Codec2 voice link wants, so
// a caller that agrees with those writes only the three pins:
//     MicConfig cfg(32, 25, 33);
struct MicConfig {
  int sckPin;
  int wsPin;
  int sdPin;
  uint32_t sampleRate;
  i2s_port_t port;
  int dmaBufCount;
  int dmaBufLen;      // samples per DMA buffer
  int frameSamples;   // mono samples the caller wants per read

  MicConfig(int sck = -1, int ws = -1, int sd = -1,
            uint32_t rate = 8000, i2s_port_t p = I2S_NUM_0,
            int bufCount = 4, int bufLen = 320, int frameLen = 320)
    : sckPin(sck), wsPin(ws), sdPin(sd), sampleRate(rate), port(p),
      dmaBufCount(bufCount), dmaBufLen(bufLen), frameSamples(frameLen) {}
};

class Mic {
public:
  // Installs and starts the I2S RX driver. Unlike the speaker there is no
  // start/stop pair: the mic clocks continuously and reads are what pace the
  // caller. Returns false if the driver refused the configuration.
  bool begin(const MicConfig &cfg);

  // Read exactly frameSamples of mono 16-bit PCM into dst.
  // Returns false if the mic delivered short within timeoutMs, which is a real
  // condition worth counting rather than papering over -- a mic that has
  // stopped clocking should surface as short reads, not as silence.
  bool readFrame(int16_t *dst, uint32_t timeoutMs = 200);

  // Discard whatever is sitting in the DMA ring. Call before a transmission so
  // it does not open with audio captured before the user pressed the button.
  void flush();

private:
  MicConfig config = {};
  bool installed = false;

  // Raw I2S landing buffer: frameSamples stereo pairs of int32.
  int32_t *rawBuf = nullptr;
  int rawCapacity = 0;
};

#endif  // RANGER_MIC_H
