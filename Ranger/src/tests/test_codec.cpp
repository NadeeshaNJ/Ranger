// Codec test: talk into the mic and hear yourself through Codec2 on the speaker.
//   1..6 = mode 3200 / 2400 / 1600 / 1300 / 1200 / 700C
// Volume starts low because mic and speaker are on the same board (it can howl).
#include <Arduino.h>
#include "Mic.h"
#include "Speaker.h"
#include "Controls.h"
#include "Codec.h"

SET_LOOP_TASK_STACK_SIZE(32 * 1024);            // Codec2 needs a big stack

Mic mic;
Speaker spk;
Controls ctl;
Codec codec;

void setup() {
  Serial.begin(115200);
  ctl.begin();
  ctl.setAmp(true);
  mic.begin();
  spk.begin();
  spk.setVolume(30);
  codec.begin(C3200);
  Serial.println("Talk into the mic. Keys 1..6 change the Codec2 mode.");
}

void loop() {
  static int16_t pcm[320];
  uint8_t bits[8];
  mic.read(pcm, codec.samples());
  uint32_t t = micros();
  codec.encode(pcm, bits);
  codec.decode(bits, pcm);
  uint32_t us = micros() - t;
  spk.play(pcm, codec.samples());

  if (Serial.available()) {
    char c = Serial.read();
    if (c >= '1' && c <= '6') {
      codec.begin(c - '1');
      Serial.printf("mode %c: %d samples -> %d bytes, encode+decode %lu us\n", c, codec.samples(), codec.bytes(), (unsigned long)us);
    }
  }
}
