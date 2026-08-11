/*
  Mic library test -- raw level meter, no codec, no radio.

  Prints min/max/RMS of each captured frame plus a text bar. Speak, tap the
  mic, then stay quiet, and watch whether the numbers actually track what you
  are doing.

  What the failure modes look like:
    - values pinned near 0 regardless of sound: mic not clocking, or the data
      is arriving in the slot this code skips (try the other channel).
    - values huge and jumping randomly even in silence: the bit alignment is
      wrong, so what is being read is not a sample. This is the case that
      makes codec2 emit pure noise while everything downstream is healthy.
    - values that respond but stay tiny: correct alignment, wrong shift --
      real audio, just far too quiet for the vocoder to work with.
*/

#include <Arduino.h>
#include <Mic.h>

#define MIC_SCK_PIN   32
#define MIC_WS_PIN    25
#define MIC_SD_PIN    33

#define SAMPLE_RATE   8000
#define FRAME_SAMPLES 320

Mic mic;
int16_t frame[FRAME_SAMPLES];

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Mic test: level meter");

  MicConfig cfg = {};
  cfg.sckPin = MIC_SCK_PIN;
  cfg.wsPin = MIC_WS_PIN;
  cfg.sdPin = MIC_SD_PIN;
  cfg.sampleRate = SAMPLE_RATE;
  cfg.port = I2S_NUM_0;
  cfg.dmaBufCount = 4;
  cfg.dmaBufLen = FRAME_SAMPLES;
  cfg.frameSamples = FRAME_SAMPLES;

  if (!mic.begin(cfg)) {
    Serial.println("Mic.begin failed, halting.");
    while (true) delay(1000);
  }
  Serial.println("Speak, then go quiet. Watch the numbers track.");
}

void loop() {
  if (!mic.readFrame(frame)) {
    Serial.println("short read");
    return;
  }

  int16_t lo = 32767, hi = -32768;
  double sumSq = 0;
  for (int i = 0; i < FRAME_SAMPLES; i++) {
    if (frame[i] < lo) lo = frame[i];
    if (frame[i] > hi) hi = frame[i];
    sumSq += (double)frame[i] * frame[i];
  }
  int rms = (int)sqrt(sumSq / FRAME_SAMPLES);

  // Log-ish bar so quiet speech is still visible next to loud peaks.
  int bars = 0;
  if (rms > 0) {
    bars = (int)(log10((double)rms) * 8);
    if (bars < 0) bars = 0;
    if (bars > 40) bars = 40;
  }

  static int frameCount = 0;
  // ~4 Hz of output; per-frame printing at 25 Hz would flood the port.
  if (++frameCount % 6 == 0) {
    Serial.printf("min %6d  max %6d  rms %5d  ", lo, hi, rms);
    for (int i = 0; i < bars; i++) Serial.print('#');
    Serial.println();
  }
}
