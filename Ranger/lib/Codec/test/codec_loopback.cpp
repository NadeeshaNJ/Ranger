/*
  Codec library test -- encode/decode a tone with no radio in the path.

  Generates a clean sine, encodes it, immediately decodes it, and plays the
  result. Nothing is transmitted, so this separates "codec2 is mangling the
  audio" from "the LoRa link is corrupting the payload".

  Expected: a recognisable tone, somewhat rougher than the input. Codec2 is a
  speech vocoder, not a general audio codec -- it models the vocal tract, so a
  pure sine comes back imperfectly. It should still be clearly the same pitch.

  If this sounds like a recognisable tone but the full radio path does not,
  the fault is in the link or the packing, not the vocoder.
*/

#include <Arduino.h>
#include <Codec.h>
#include <Sound.h>

#define SPK_BCK_PIN   27
#define SPK_WS_PIN    26
#define SPK_DIN_PIN   15

#define SAMPLE_RATE   8000
#define FRAME_SAMPLES 320
#define FRAME_BYTES   7

Sound speaker;
Codec encoder;
Codec decoder;   // separate handle from the encoder, on purpose -- see Codec.h

int16_t toneBuf[FRAME_SAMPLES];
int16_t outBuf[FRAME_SAMPLES];
uint8_t bits[FRAME_BYTES];

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Codec test: tone -> encode -> decode -> speaker");

  if (!encoder.begin(CODEC2_MODE_1300) || !decoder.begin(CODEC2_MODE_1300)) {
    Serial.println("Codec.begin failed, halting.");
    while (true) delay(1000);
  }
  if (!encoder.geometryMatches(FRAME_SAMPLES, FRAME_BYTES)) {
    Serial.printf("geometry mismatch: %d samples / %d bytes\n",
                  encoder.samplesPerFrame(), encoder.bytesPerFrame());
    while (true) delay(1000);
  }

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
  Serial.println("Playing 1 kHz through the vocoder.");
}

void loop() {
  speaker.fillTone(toneBuf, FRAME_SAMPLES, 1000.0f, 8000);
  encoder.encode(bits, toneBuf);
  decoder.decode(outBuf, bits);
  speaker.playMono(outBuf, FRAME_SAMPLES, 4);
}
