/*
  Sound library test -- DAC-only tone sweep.

  Build by pointing an env's custom_src_dir at this folder, or copy into a
  src dir. Nothing but the Sound library and I2S is involved: no mic, no
  codec2, no radio.

  What it is for: if tones at very different frequencies all sound the same,
  the DAC is not actually reproducing the samples it is handed, and every
  diagnosis further up the chain is meaningless. This sweeps 200 Hz -> 4 kHz
  in steps and announces each one on serial, so you can hear whether pitch
  tracks the announcement.

  Expected: a clear, obviously rising pitch. Anything else -- identical noise
  at every step, or a buzz that does not change -- means the fault is in the
  I2S clocking or the DAC wiring, not in software upstream of it.
*/

#include <Arduino.h>
#include <Sound.h>

#define SPK_BCK_PIN   27
#define SPK_WS_PIN    26
#define SPK_DIN_PIN   15

#define SAMPLE_RATE   8000
#define FRAME_SAMPLES 320

Sound speaker;
int16_t toneBuf[FRAME_SAMPLES];

// Well inside the 8 kHz Nyquist limit of 4 kHz, so none of these should alias.
const float sweep[] = {200, 400, 800, 1000, 2000, 3000};
const int sweepCount = sizeof(sweep) / sizeof(sweep[0]);

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Sound test: DAC tone sweep");

  SoundConfig cfg = {};
  cfg.bclkPin = SPK_BCK_PIN;
  cfg.lrcPin = SPK_WS_PIN;
  cfg.dinPin = SPK_DIN_PIN;
  cfg.sampleRate = SAMPLE_RATE;
  cfg.port = I2S_NUM_1;
  cfg.dmaBufCount = 6;
  cfg.dmaBufLen = FRAME_SAMPLES;

  if (!speaker.begin(cfg)) {
    Serial.println("Sound.begin failed, halting.");
    while (true) delay(1000);
  }
  speaker.start();
  Serial.println("Playing. Each tone holds for 2 seconds.");
}

void loop() {
  for (int s = 0; s < sweepCount; s++) {
    Serial.printf("  %.0f Hz\n", (double)sweep[s]);

    // 2 seconds at FRAME_SAMPLES per frame.
    int frames = (SAMPLE_RATE * 2) / FRAME_SAMPLES;
    for (int f = 0; f < frames; f++) {
      speaker.fillTone(toneBuf, FRAME_SAMPLES, sweep[s], 8000);
      // Gain 1: the tone is already at a sensible amplitude, and any clipping
      // here would be the test's fault rather than the hardware's.
      speaker.playMono(toneBuf, FRAME_SAMPLES, 1);
    }
  }
}
