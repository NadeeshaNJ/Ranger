/*
  Walkie-Talkie over LoRa, Push-to-Talk, Codec2 compressed
  ----------------------------------------------------------
  Flash this SAME sketch to both boards.
  Hold the PTT button to transmit your mic audio.
  Release it to listen and hear whatever the other board sends.

  Hardware lives in lib/: Mic (INMP441), Sound (PCM5100A), Radio (SX1278),
  Codec (Codec2), Buttons (PTT). This file is just the state machine and the
  wiring between them.

    mic 32-bit @8kHz -> 16-bit PCM -> Codec2 1300 -> LoRa -> Codec2 decode
    -> gain -> 16-bit stereo -> DAC

  Capture runs in its own task rather than inline in loop(). The mic clocks
  continuously whether or not anyone reads it, so any stretch spent inside a
  blocking LoRa transmit was a stretch where the I2S DMA ring kept filling
  with nobody draining it, and the speech developed a gap every packet.
  micTask now owns the mic and never stops reading; loop() only ever pulls
  already-encoded frames out of a queue.
*/

#include <Arduino.h>
#include <string.h>

#include <Buttons.h>
#include <Codec.h>
#include <Mic.h>
#include <Radio.h>
#include <Sound.h>

// ---- Mic pins (I2S_NUM_0) ----
#define MIC_SCK_PIN   32
#define MIC_WS_PIN    25
#define MIC_SD_PIN    33

// ---- Speaker pins (I2S_NUM_1, PCM5100A) ----
#define SPK_WS_PIN    26
#define SPK_BCK_PIN   27
// DIN must NOT be 25 -- that is the mic's WS line. Sharing one GPIO between
// I2S_NUM_0 and I2S_NUM_1 leaves the DAC with no data and the speaker silent.
#define SPK_DIN_PIN   15

// ---- LoRa pins ----
#define LORA_SS    5
#define LORA_RST   14
#define LORA_DIO0  4

// ---- PTT button ----
#define PTT_PIN    12
#define DEBOUNCE_MS  50

#define RX_REPORT_MS  1000

// Squelch: how long after the last packet before muting the speaker. A sender
// releasing PTT just goes quiet -- there is no end-of-transmission marker --
// so the receiver has to notice the silence itself. One packet is ~160 ms of
// audio, so 400 ms tolerates a dropped packet without chopping a real tail.
#define RX_SQUELCH_MS  400

#define SAMPLE_RATE   8000

// Mode 1300: 320 samples (40 ms) in, 52 bits = 7 bytes out, 25 frames/sec.
#define CODEC2_MODE       CODEC2_MODE_1300
#define FRAME_SAMPLES     320
#define FRAME_BYTES       7

// Batching 4 frames gives ~6.25 packets/sec at 28 bytes, which the link can
// carry. The cost is ~160 ms of buffering latency each way.
#define FRAMES_PER_PACKET 4
#define PACKET_BYTES      (FRAME_BYTES * FRAMES_PER_PACKET)

// ---- Diagnostic tone injection ----
// 0 = normal; 1 = tone replaces the mic; 2 = tone replaces the decoder.
// See lib/Sound/test/tone_sweep.cpp for a DAC-only version of this test.
#define TEST_TONE       0
#define TONE_HZ         1000
#define TONE_AMPLITUDE  8000

// The PCM5100A is line level with no amplifier, so this only maximises the
// signal handed to whatever amp follows -- it cannot make a bare speaker loud.
#define PLAYBACK_GAIN  6

// Two packets' worth: enough to ride out one transmit without stalling
// capture, small enough that a backlog cannot grow into audible lag.
#define FRAME_QUEUE_LEN   (FRAMES_PER_PACKET * 2)

Mic mic;
Sound speaker;
Radio radio;
Button ptt;

// Two Codec instances, never shared between tasks. A CODEC2 handle carries
// mutable per-frame state that both encode and decode write through; sharing
// one across cores corrupts it and crashes. See lib/Codec/src/Codec.h.
Codec encoder;   // micTask / core 0 only
Codec decoder;   // loop() / core 1 only

typedef struct {
  uint8_t bytes[FRAME_BYTES];
} EncodedFrame;

QueueHandle_t frameQueue = NULL;
TaskHandle_t micTaskHandle = NULL;

volatile bool captureEnabled = false;

volatile uint32_t framesCaptured = 0;
volatile uint32_t framesDropped = 0;
volatile uint32_t shortReads = 0;

int16_t pcmFrame[FRAME_SAMPLES];      // micTask only
int16_t decodedFrame[FRAME_SAMPLES];  // loop() only
uint8_t packetBuf[PACKET_BYTES];      // loop() only

void micTask(void *param);
void flushFrameQueue();

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Walkie-talkie starting...");
#if TEST_TONE == 1
  Serial.printf("*** TEST_TONE 1: %d Hz replaces the mic ***\n", TONE_HZ);
#elif TEST_TONE == 2
  Serial.printf("*** TEST_TONE 2: %d Hz replaces the decoder ***\n", TONE_HZ);
#endif

  ptt.begin(PTT_PIN, DEBOUNCE_MS, true);

  if (!encoder.begin(CODEC2_MODE) || !decoder.begin(CODEC2_MODE)) {
    Serial.println("Codec.begin failed, halting.");
    while (true) delay(1000);
  }
  if (!encoder.geometryMatches(FRAME_SAMPLES, FRAME_BYTES)) {
    Serial.printf("Codec2 geometry mismatch: %d samples / %d bytes, halting.\n",
                  encoder.samplesPerFrame(), encoder.bytesPerFrame());
    while (true) delay(1000);
  }
  Serial.printf("Codec2 ready: %d samples -> %d bytes per frame\n",
                encoder.samplesPerFrame(), encoder.bytesPerFrame());

  MicConfig micCfg = {};
  micCfg.sckPin = MIC_SCK_PIN;
  micCfg.wsPin = MIC_WS_PIN;
  micCfg.sdPin = MIC_SD_PIN;
  micCfg.sampleRate = SAMPLE_RATE;
  micCfg.port = I2S_NUM_0;
  micCfg.dmaBufCount = 4;
  micCfg.dmaBufLen = FRAME_SAMPLES;
  micCfg.frameSamples = FRAME_SAMPLES;
  if (!mic.begin(micCfg)) {
    Serial.println("Mic.begin failed, halting.");
    while (true) delay(1000);
  }

  SoundConfig spkCfg = {};
  spkCfg.bclkPin = SPK_BCK_PIN;
  spkCfg.lrcPin = SPK_WS_PIN;
  spkCfg.dinPin = SPK_DIN_PIN;
  spkCfg.sampleRate = SAMPLE_RATE;
  spkCfg.port = I2S_NUM_1;
  // One playFrame writes FRAME_SAMPLES stereo pairs, so a buffer smaller than
  // that would block partway through every write and chop the audio.
  spkCfg.dmaBufCount = 6;
  spkCfg.dmaBufLen = FRAME_SAMPLES;
  if (!speaker.begin(spkCfg)) {
    Serial.println("Sound.begin failed, halting.");
    while (true) delay(1000);
  }

  RadioConfig radioCfg = {};
  radioCfg.ssPin = LORA_SS;
  radioCfg.rstPin = LORA_RST;
  radioCfg.dio0Pin = LORA_DIO0;
  radioCfg.frequencyHz = 433E6;
  radioCfg.syncWord = 0xF3;
  radioCfg.txPower = 20;
  radioCfg.spreadingFactor = 7;
  radioCfg.bandwidthHz = 125E3;
  radioCfg.enableCrc = true;
  if (!radio.begin(radioCfg)) {
    Serial.println("Radio.begin failed, halting.");
    while (true) delay(1000);
  }

  frameQueue = xQueueCreate(FRAME_QUEUE_LEN, sizeof(EncodedFrame));
  if (frameQueue == NULL) {
    Serial.println("frameQueue alloc failed, halting.");
    while (true) delay(1000);
  }

  // Pinned to core 0; loop() runs on core 1. Splitting them is what lets
  // capture continue during a blocking transmit. Priority 2 puts it above
  // loop() so a ready frame is encoded promptly.
  // Stack size comes from Codec.h -- codec2 keeps large FFT buffers on the
  // caller's stack, not in its handle.
  BaseType_t ok = xTaskCreatePinnedToCore(
      micTask, "micTask", CODEC_ENCODE_STACK, NULL, 2, &micTaskHandle, 0);
  if (ok != pdPASS) {
    Serial.println("micTask create failed, halting.");
    while (true) delay(1000);
  }

  Serial.println("Ready. Hold the PTT button to talk.");
}

// Capture side. Runs forever, pinned opposite loop() so a blocking transmit on
// one core cannot delay the mic on the other.
void micTask(void *param) {
  (void)param;

  for (;;) {
    if (!captureEnabled) {
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }

#if TEST_TONE == 1
    // Tone stands in for the mic; everything downstream runs normally.
    speaker.fillTone(pcmFrame, FRAME_SAMPLES, (float)TONE_HZ, TONE_AMPLITUDE);
    // Pace at real time; without the mic's blocking read this would spin.
    vTaskDelay(pdMS_TO_TICKS(1000 * FRAME_SAMPLES / SAMPLE_RATE));
#else
    if (!mic.readFrame(pcmFrame)) {
      shortReads++;
      continue;
    }
#endif

    EncodedFrame frame;
    encoder.encode(frame.bytes, pcmFrame);
    framesCaptured++;

    // Drop-oldest on a full queue. Blocking here would just move the overflow
    // back into the I2S DMA ring where it is invisible; discarding the stale
    // frame keeps latency bounded and sends the most recent speech.
    if (xQueueSend(frameQueue, &frame, 0) != pdTRUE) {
      EncodedFrame discard;
      if (xQueueReceive(frameQueue, &discard, 0) == pdTRUE) framesDropped++;
      if (xQueueSend(frameQueue, &frame, 0) != pdTRUE) framesDropped++;
    }
  }
}

void flushFrameQueue() {
  EncodedFrame discard;
  while (xQueueReceive(frameQueue, &discard, 0) == pdTRUE) { }
}

// Decode one frame and push it to the DAC.
void playFrame(const uint8_t *bits) {
#if TEST_TONE == 2
  // Ignore what arrived and play a local tone. Packets are still received and
  // counted, so this proves out the DAC and wiring without trusting the
  // payload or the vocoder.
  (void)bits;
  speaker.fillTone(decodedFrame, FRAME_SAMPLES, (float)TONE_HZ, TONE_AMPLITUDE);
#else
  decoder.decode(decodedFrame, bits);
#endif
  speaker.playMono(decodedFrame, FRAME_SAMPLES, PLAYBACK_GAIN);
}

void loop() {
  bool talking = ptt.isPressed();
  static bool wasTalking = false;

  if (talking && !wasTalking) {
    speaker.stop();       // idempotent; safe whether or not it was playing
    flushFrameQueue();
    mic.flush();          // do not open with audio captured before the press
    framesCaptured = 0;
    framesDropped = 0;
    shortReads = 0;
    captureEnabled = true;
    Serial.println("PTT pressed, transmitting...");
  }
  if (!talking && wasTalking) {
    captureEnabled = false;
    Serial.printf("PTT released: %u frames captured, %u dropped, %u short reads\n",
                  (unsigned)framesCaptured, (unsigned)framesDropped,
                  (unsigned)shortReads);
    // High-water marks are the smallest free stack ever seen, in bytes. If
    // either approaches zero the canary is about to trip.
    Serial.printf("  stack free: micTask %u B, loop %u B\n",
                  (unsigned)uxTaskGetStackHighWaterMark(micTaskHandle),
                  (unsigned)uxTaskGetStackHighWaterMark(NULL));
    flushFrameQueue();
    // Speaker deliberately left stopped: the receive path starts it on the
    // first arriving packet and the squelch stops it when they stop.
    Serial.println("Listening...");
  }
  wasTalking = talking;

  if (talking) {
    // ---- TRANSMIT: drain encoded frames from micTask -> LoRa ----
    int framesReady = 0;

    while (framesReady < FRAMES_PER_PACKET) {
      EncodedFrame frame;
      if (xQueueReceive(frameQueue, &frame, pdMS_TO_TICKS(200)) != pdTRUE) break;
      memcpy(&packetBuf[framesReady * FRAME_BYTES], frame.bytes, FRAME_BYTES);
      framesReady++;
      if (!ptt.isPressed()) break;   // released mid-batch, stop keying
    }

    if (framesReady > 0) {
      radio.sendPacket(packetBuf, framesReady * FRAME_BYTES);
    }

    // sendPacket blocks, so yield to let IDLE1 run and keep the Timer Group
    // watchdog quiet (TG1WDT_SYS_RESET without this).
    vTaskDelay(1);

  } else {
    // ---- RECEIVE: LoRa -> Codec2 decode -> gain -> DAC ----
    static unsigned long lastReport = 0;
    static int packetsSinceReport = 0;
    static long bytesSinceReport = 0;
    static unsigned long lastPacketMs = 0;

    int packetSize = radio.receivePacket(packetBuf, PACKET_BYTES);
    if (packetSize > 0) {
      lastPacketMs = millis();
      speaker.start();   // idempotent; zeroes the ring on the real transition

      int framesIn = packetSize / FRAME_BYTES;
      if (framesIn > FRAMES_PER_PACKET) framesIn = FRAMES_PER_PACKET;

      packetsSinceReport++;
      bytesSinceReport += packetSize;

      // No delay between frames: the frames in a packet are contiguous audio
      // and a tick of silence between each is audible. playMono blocks on the
      // DMA ring, which yields and paces this at real playback speed.
      for (int f = 0; f < framesIn; f++) {
        playFrame(&packetBuf[f * FRAME_BYTES]);
      }
    }

    unsigned long now = millis();

    // Squelch. A gap in packets is the only signal the sender stopped talking.
    if (speaker.isRunning() && (now - lastPacketMs >= RX_SQUELCH_MS)) {
      speaker.stop();
      Serial.println("Squelch: transmission ended, speaker muted.");
    }

    if (now - lastReport >= RX_REPORT_MS) {
      lastReport = now;
      if (packetsSinceReport > 0) {
        Serial.printf("RX %d pkt/s, %ld B, RSSI %d dBm, SNR %.1f\n",
                      packetsSinceReport, bytesSinceReport,
                      radio.lastRssi(), (double)radio.lastSnr());
      } else if (framesDropped > 0 || shortReads > 0) {
        Serial.printf("RX idle (last TX: %u dropped, %u short reads)\n",
                      (unsigned)framesDropped, (unsigned)shortReads);
      } else {
        Serial.println("RX idle (no packets)");
      }
      packetsSinceReport = 0;
      bytesSinceReport = 0;
    }
  }
}
