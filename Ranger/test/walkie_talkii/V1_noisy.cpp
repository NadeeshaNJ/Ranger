/*
  Walkie-Talkie over LoRa, Push-to-Talk, Codec2 compressed
  ----------------------------------------------------------
  Flash this SAME sketch to both boards.
  Hold the PTT button to transmit your mic audio.
  Release it to listen and hear whatever the other board sends.

  Mic (INMP441, I2S_NUM_0, RX): SCK=32, WS=25, SD=33, L/R -> GND
  Speaker (PCM5100A, I2S_NUM_1, TX): WS(LRC)=26, BCK=27, DIN=15
  LoRa (Ra-01/SX1278): NSS=5, RST=14, DIO0=4
  PTT button: GPIO12 -> other leg to GND (uses internal pull-up)

  Pin assignments and the PTT/debounce logic come from w_t_serial.cpp, which
  is known working on hardware. What is new here is the Codec2 audio path:

    mic 32-bit @8kHz -> 16-bit PCM -> Codec2 1300 -> LoRa -> Codec2 decode
    -> gain -> 16-bit stereo -> DAC

  The raw-sample version needed ~500 packets/sec to keep up at 16 kHz, but
  SF7/125kHz tops out near 28 -- hence the choppy audio. Codec2 compresses
  40 ms of speech into 7 bytes, which fits the link with room to spare.

  Capture runs in its own task rather than inline in loop(). The mic clocks
  continuously whether or not anyone reads it, so any stretch spent inside
  LoRa.endPacket() was a stretch where the I2S DMA ring kept filling with
  nobody draining it. Once those four buffers wrap, the oldest audio is
  overwritten by the driver and the speech develops a gap every packet.
  micTask now owns the mic and never stops reading; loop() only ever pulls
  already-encoded frames out of a queue, so transmit time no longer steals
  from capture time.
*/

#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <string.h>
#include <driver/i2s.h>
#include <codec2.h>

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
// GPIO34-39 are input-only and have NO internal pull-up, so a button on one of
// them floats and reads as permanently pressed. GPIO12 has a real pull-up.
#define PTT_PIN    12
#define DEBOUNCE_MS  50

// How often the receive path reports in. Per-packet logging would stall the
// I2S feed at 115200 baud, so packets are counted and summarised instead.
#define RX_REPORT_MS  1000

// Squelch: how long to wait after the last packet before muting the speaker.
// A sender releasing PTT just goes quiet -- there is no end-of-transmission
// marker -- so the receiver has to notice the silence for itself. Without this
// the DAC keeps clocking out whatever was last written and the speaker howls
// until reset. One packet is ~160 ms of audio, so 400 ms tolerates a dropped
// packet mid-sentence without chopping the tail off a real transmission.
#define RX_SQUELCH_MS  400

// Codec2 only runs at 8 kHz -- the raw-sample sketches used 16 kHz.
#define SAMPLE_RATE   8000

// Mode 1300: 320 samples (40 ms) in, 52 bits = 7 bytes out, 25 frames/sec.
// Checked against codec2_samples_per_frame()/codec2_bits_per_frame() at boot.
#define CODEC2_MODE       CODEC2_MODE_1300
#define FRAME_SAMPLES     320
#define FRAME_BYTES       7

// A LoRa packet at SF7/125kHz costs ~36 ms of airtime almost regardless of
// payload size, so one frame per packet could never keep up with 25 frames/s.
// Batching 4 frames drops it to ~6.25 packets/sec at 28 bytes, which fits.
// The cost is ~160 ms of buffering latency each way.
#define FRAMES_PER_PACKET 4
#define PACKET_BYTES      (FRAME_BYTES * FRAMES_PER_PACKET)

// Software playback gain, same idea as the record/playback sketch. The
// PCM5100A is a line-level DAC with no amplifier, so this only maximises the
// signal handed to whatever amp/headphones follow -- it cannot make a bare
// speaker loud. Too high and the clamp below just flattens the peaks.
#define PLAYBACK_GAIN  6

// Depth of the encoded-frame queue between micTask and loop(). Two packets'
// worth: enough to ride out one LoRa transmit without stalling capture, small
// enough that a backlog cannot grow into audible lag. At 40 ms per frame this
// caps added latency at ~320 ms.
#define FRAME_QUEUE_LEN   (FRAMES_PER_PACKET * 2)

// Two separate Codec2 instances, one per task. A CODEC2 handle carries mutable
// internal state (pitch/energy history, LSP predictor memory) that encode and
// decode both write through. Sharing a single handle across the two cores let
// micTask's encode reshuffle state while loop() was inside decode, which
// crashed with LoadProhibited on the first PTT press after a receive.
// Two handles cost ~5 kB extra RAM and make the paths genuinely independent.
struct CODEC2 *encodeState = NULL;   // micTask / core 0 only
struct CODEC2 *decodeState = NULL;   // loop() / core 1 only

// One encoded Codec2 frame in flight between the two tasks.
typedef struct {
  uint8_t bytes[FRAME_BYTES];
} EncodedFrame;

// micTask -> loop(). Carries encoded frames, not raw PCM: 7 bytes per frame
// instead of 640, so the queue costs almost nothing and the expensive
// codec2_encode() happens on the capture side where the deadline actually is.
QueueHandle_t frameQueue = NULL;
TaskHandle_t micTaskHandle = NULL;

// Set true by loop() while PTT is held. micTask watches this instead of
// reading the pin itself, so debounce stays in one place.
volatile bool captureEnabled = false;

// Whether I2S_NUM_1 is currently running. Both the PTT handler and the receive
// squelch start and stop the speaker, and calling i2s_start on an already
// started peripheral (or stop on a stopped one) is not harmless -- tracking the
// state in one place keeps the two paths from fighting.
bool speakerLive = false;

// Overrun bookkeeping, all written by micTask and read by loop() for the
// periodic report. Counters are single-writer so they need no lock; they are
// only ever incremented in one task and printed in the other.
volatile uint32_t framesCaptured = 0;
volatile uint32_t framesDropped = 0;   // dropped because the queue was full
volatile uint32_t shortReads = 0;      // mic delivered less than a full frame

// Mic reads are stereo int32; only the left slot carries data (L/R tied low),
// so we need 2x the samples to fill one mono Codec2 frame.
// rawBuffer/pcmFrame belong to micTask; decodedFrame/playBuf/packetBuf belong
// to loop(). With encode/decode split onto their own Codec2 handles above,
// the only thing crossing tasks is frameQueue, which is already thread-safe.
int32_t rawBuffer[FRAME_SAMPLES * 2];
int16_t pcmFrame[FRAME_SAMPLES];
int16_t decodedFrame[FRAME_SAMPLES];
int16_t playBuf[FRAME_SAMPLES * 2];   // interleaved L/R
uint8_t packetBuf[PACKET_BYTES];

// Defined below setup(), which starts the task and drains the queue.
void micTask(void *param);
void flushFrameQueue();

void setupMicI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = FRAME_SAMPLES,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = MIC_SCK_PIN,
    .ws_io_num = MIC_WS_PIN,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = MIC_SD_PIN
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
  i2s_zero_dma_buffer(I2S_NUM_0);
}

void setupSpeakerI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    // One playFrame() writes FRAME_SAMPLES stereo pairs. The old 8 x 64 = 512
    // sample ring was smaller than that, so every write blocked partway
    // through and the audio came out chopped. Six buffers of one mono frame
    // each holds ~3 frames of stereo with room to absorb packet jitter.
    .dma_buf_count = 6,
    .dma_buf_len = FRAME_SAMPLES,
    .use_apll = false,
    .tx_desc_auto_clear = true,   // emit silence on underrun, not the last buffer
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = SPK_BCK_PIN,
    .ws_io_num = SPK_WS_PIN,
    .data_out_num = SPK_DIN_PIN,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_NUM_1, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_1, &pin_config);
}


void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Walkie-talkie starting...");

  pinMode(PTT_PIN, INPUT_PULLUP);

  encodeState = codec2_create(CODEC2_MODE);
  decodeState = codec2_create(CODEC2_MODE);
  if (encodeState == NULL || decodeState == NULL) {
    Serial.println("codec2_create failed, halting.");
    while (true) delay(1000);
  }

  // The buffer sizes above are compile-time constants; make sure the codec
  // actually agrees rather than silently overrunning them.
  int spf = codec2_samples_per_frame(encodeState);
  int bpf = (codec2_bits_per_frame(encodeState) + 7) / 8;
  if (spf != FRAME_SAMPLES || bpf != FRAME_BYTES) {
    Serial.printf("Codec2 geometry mismatch: %d samples / %d bytes, halting.\n", spf, bpf);
    while (true) delay(1000);
  }
  Serial.printf("Codec2 ready: %d samples -> %d bytes per frame\n", spf, bpf);

  setupMicI2S();
  setupSpeakerI2S();
  i2s_stop(I2S_NUM_1); // stay silent until we're actually listening

  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  while (!LoRa.begin(433E6)) {
    Serial.println(".");
    delay(500);
  }
  LoRa.setSyncWord(0xF3);
  LoRa.setTxPower(20, PA_OUTPUT_PA_BOOST_PIN);
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.enableCrc();

  frameQueue = xQueueCreate(FRAME_QUEUE_LEN, sizeof(EncodedFrame));
  if (frameQueue == NULL) {
    Serial.println("frameQueue alloc failed, halting.");
    while (true) delay(1000);
  }

  // Pinned to core 0; the Arduino loop() runs on core 1. Splitting them is what
  // lets capture continue during a blocking LoRa.endPacket().
  // Priority 2 puts it above loop() (priority 1) so a ready frame is encoded
  // promptly rather than waiting on the transmit side.
  //
  // 32 kB stack. Codec2 does NOT keep its working buffers in the handle; it
  // puts large FFT arrays on the stack at every level. Frame sizes read from
  // the built firmware's `entry` instructions:
  //
  //   analyse_one_frame  12352 B   (three COMP[FFT_ENC] = 3 * 512 * 8)
  //     nlp               8256 B   (two COMP[PE_FFT_SIZE])
  //
  // so the encode chain alone needs ~20.6 kB before call overhead. Earlier
  // guesses of 4 kB and 20 kB both tripped the canary -- the second one landed
  // just under the true peak. 32 kB leaves real headroom; watch the
  // high-water print on PTT release rather than trusting this number.
  BaseType_t ok = xTaskCreatePinnedToCore(
      micTask, "micTask", 32768, NULL, 2, &micTaskHandle, 0);
  if (ok != pdPASS) {
    Serial.println("micTask create failed, halting.");
    while (true) delay(1000);
  }

  Serial.println("Ready. Hold the PTT button to talk.");
}

// Debounced PTT read. The raw pin is sampled every pass; the timer restarts on
// every observed change, so the returned state only flips once the pin has held
// steady for DEBOUNCE_MS. Between bounces the last settled state is returned.
bool pttPressed() {
  static bool lastRaw = HIGH;      // pin idles HIGH thanks to the pull-up
  static bool settled = HIGH;
  static unsigned long lastChange = 0;

  bool raw = digitalRead(PTT_PIN);
  unsigned long now = millis();

  if (raw != lastRaw) {
    lastRaw = raw;
    lastChange = now;              // still bouncing, restart the window
  } else if (now - lastChange > DEBOUNCE_MS) {
    settled = raw;                 // held steady long enough, accept it
  }

  return (settled == LOW);
}

// Read exactly one frame of mono 16-bit PCM from the mic.
// Returns false if the mic delivered short.
bool readMicFrame() {
  size_t bytesRead = 0;
  // A bounded wait rather than portMAX_DELAY: one frame is only 40 ms of audio,
  // so 200 ms is generous. If the mic ever stops clocking this returns short
  // instead of blocking forever and taking the watchdog down with it.
  i2s_read(I2S_NUM_0, (void*)rawBuffer, sizeof(rawBuffer), &bytesRead, pdMS_TO_TICKS(200));

  int stereoSamples = bytesRead / sizeof(int32_t);
  int collected = 0;

  // [L, R, L, R, ...] with real data in the left slot. >>16 brings the 24-bit
  // left-justified mic sample into 16-bit range, matching the record/playback
  // sketch. Codec2 wants plain 16-bit PCM, no extra attenuation.
  for (int i = 0; i < stereoSamples && collected < FRAME_SAMPLES; i += 2) {
    int32_t sample = rawBuffer[i] >> 16;
    if (sample > 32767) sample = 32767;
    if (sample < -32768) sample = -32768;
    pcmFrame[collected++] = (int16_t)sample;
  }

  return (collected == FRAME_SAMPLES);
}

// Capture side. Runs forever, pinned opposite the Arduino loop so a blocking
// LoRa transmit on one core cannot delay the mic on the other.
//
// While PTT is held this reads back-to-back with no delay of its own: i2s_read
// blocks until a frame's worth of audio exists, which paces the loop at exactly
// 25 Hz and yields to the scheduler for free. That is the whole point -- the
// mic is never left unread.
void micTask(void *param) {
  (void)param;

  for (;;) {
    if (!captureEnabled) {
      // Idle. Nothing is draining I2S_NUM_0 here, but that is harmless while
      // not transmitting -- the DMA ring just recycles stale audio, and the
      // zero below discards it so the next PTT press starts clean rather than
      // sending a few frames of whatever was captured before it.
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }

    if (!readMicFrame()) {
      shortReads++;
      continue;    // mic stalled; try again rather than emitting a partial frame
    }

    EncodedFrame frame;
    codec2_encode(encodeState, frame.bytes, pcmFrame);
    framesCaptured++;

    // Drop-oldest on a full queue. The alternative -- blocking here -- would
    // just move the overflow back into the I2S DMA ring where it is invisible.
    // Discarding the stale frame instead keeps latency bounded and guarantees
    // that what goes out is the most recent speech.
    if (xQueueSend(frameQueue, &frame, 0) != pdTRUE) {
      EncodedFrame discard;
      if (xQueueReceive(frameQueue, &discard, 0) == pdTRUE) {
        framesDropped++;
      }
      // Retry once. If it still fails, loop() drained concurrently and the
      // next iteration will find room, so the frame is simply let go.
      if (xQueueSend(frameQueue, &frame, 0) != pdTRUE) {
        framesDropped++;
      }
    }
  }
}

// Discard anything left in the queue. Called on PTT edges so a new
// transmission never starts by sending audio captured before the press.
void flushFrameQueue() {
  EncodedFrame discard;
  while (xQueueReceive(frameQueue, &discard, 0) == pdTRUE) {
    // drain
  }
}

// Decode one frame and push it to the DAC with gain applied.
void playFrame(const uint8_t *bits) {
  codec2_decode(decodeState, decodedFrame, (unsigned char *)bits);

  for (int i = 0; i < FRAME_SAMPLES; i++) {
    // Widen to 32-bit before scaling so the multiply cannot wrap, then clamp.
    int32_t amplified = (int32_t)decodedFrame[i] * PLAYBACK_GAIN;
    if (amplified > 32767) amplified = 32767;
    if (amplified < -32768) amplified = -32768;

    playBuf[i * 2]     = (int16_t)amplified;
    playBuf[i * 2 + 1] = (int16_t)amplified;
  }

  size_t bytesWritten = 0;
  i2s_write(I2S_NUM_1, playBuf, FRAME_SAMPLES * 2 * sizeof(int16_t), &bytesWritten, pdMS_TO_TICKS(200));
}

void loop() {
  bool talking = pttPressed();
  static bool wasTalking = false;

  if (talking && !wasTalking) {
    if (speakerLive) {   // mute the moment PTT is pressed, if it was playing
      i2s_zero_dma_buffer(I2S_NUM_1);
      i2s_stop(I2S_NUM_1);
      speakerLive = false;
    }
    flushFrameQueue();   // discard pre-press audio
    framesCaptured = 0;
    framesDropped = 0;
    shortReads = 0;
    captureEnabled = true;
    Serial.println("PTT pressed, transmitting...");
  }
  if (!talking && wasTalking) {
    captureEnabled = false;
    // High-water marks are the smallest free stack ever seen, in bytes. If
    // either approaches zero the canary is about to trip, so report them
    // alongside the frame counters rather than waiting for a crash.
    Serial.printf("PTT released: %u frames captured, %u dropped, %u short reads\n",
                  (unsigned)framesCaptured, (unsigned)framesDropped,
                  (unsigned)shortReads);
    Serial.printf("  stack free: micTask %u B, loop %u B\n",
                  (unsigned)uxTaskGetStackHighWaterMark(micTaskHandle),
                  (unsigned)uxTaskGetStackHighWaterMark(NULL));
    flushFrameQueue();   // do not transmit leftovers after the release
    // The speaker is deliberately left stopped here. The receive path starts
    // it on the first arriving packet and the squelch stops it again when they
    // stop, so an idle radio means a silent DAC rather than one clocking out
    // an empty ring.
    Serial.println("Listening...");
  }
  wasTalking = talking;

  if (talking) {
    // ---- TRANSMIT: drain encoded frames from micTask -> LoRa ----
    // No mic access here at all. This blocks only on the queue, and micTask
    // keeps filling it throughout the LoRa transmit below.
    int framesReady = 0;

    while (framesReady < FRAMES_PER_PACKET) {
      EncodedFrame frame;
      // One frame is 40 ms of audio; 200 ms of patience means a genuinely
      // stalled mic falls through with a partial batch instead of hanging.
      if (xQueueReceive(frameQueue, &frame, pdMS_TO_TICKS(200)) != pdTRUE) {
        break;
      }
      memcpy(&packetBuf[framesReady * FRAME_BYTES], frame.bytes, FRAME_BYTES);
      framesReady++;

      // Bail out mid-batch if PTT was released, so we don't keep keying.
      if (!pttPressed()) break;
    }

    if (framesReady > 0) {
      LoRa.beginPacket();
      LoRa.write(packetBuf, framesReady * FRAME_BYTES);
      LoRa.endPacket();
    }

    // endPacket() is blocking, so yield to let IDLE1 run and keep the Timer
    // Group watchdog quiet (TG1WDT_SYS_RESET without this).
    vTaskDelay(1);

  } else {
    // ---- RECEIVE: LoRa -> Codec2 decode -> gain -> DAC ----
    static unsigned long lastReport = 0;
    static int packetsSinceReport = 0;
    static long bytesSinceReport = 0;
    static int lastRssi = 0;
    static float lastSnr = 0;
    static unsigned long lastPacketMs = 0;

    int packetSize = LoRa.parsePacket();
    if (packetSize) {
      lastPacketMs = millis();
      if (!speakerLive) {
        // First packet of a transmission. Start from a zeroed ring so the
        // opening frame is not prefixed with whatever was left over.
        i2s_zero_dma_buffer(I2S_NUM_1);
        i2s_start(I2S_NUM_1);
        speakerLive = true;
      }
      // Only whole frames are decodable; a partial tail is discarded.
      int framesIn = packetSize / FRAME_BYTES;
      if (framesIn > FRAMES_PER_PACKET) framesIn = FRAMES_PER_PACKET;

      int read = 0;
      while (LoRa.available() && read < framesIn * FRAME_BYTES) {
        packetBuf[read++] = (uint8_t)LoRa.read();
      }

      // Sample the link quality here, before the next parsePacket() overwrites it.
      packetsSinceReport++;
      bytesSinceReport += packetSize;
      lastRssi = LoRa.packetRssi();
      lastSnr = LoRa.packetSnr();

      // No delay between frames. The four frames in a packet are contiguous
      // audio, and a tick of silence inserted between each one is audible.
      // i2s_write blocks on the DMA ring once it is full, which yields to the
      // scheduler on its own and paces this loop at real-time playback speed.
      for (int f = 0; f < framesIn; f++) {
        playFrame(&packetBuf[f * FRAME_BYTES]);
      }
    }

    unsigned long now = millis();

    // Squelch. The sender gives no end-of-transmission marker, so a gap in
    // packets is the only signal that it stopped talking. Zero the ring before
    // stopping so the DAC is not left holding the tail of the last frame.
    if (speakerLive && (now - lastPacketMs >= RX_SQUELCH_MS)) {
      i2s_zero_dma_buffer(I2S_NUM_1);
      i2s_stop(I2S_NUM_1);
      speakerLive = false;
      Serial.println("Squelch: transmission ended, speaker muted.");
    }

    if (now - lastReport >= RX_REPORT_MS) {
      lastReport = now;
      if (packetsSinceReport > 0) {
        Serial.print("RX ");
        Serial.print(packetsSinceReport);
        Serial.print(" pkt/s, ");
        Serial.print(bytesSinceReport);
        Serial.print(" B, RSSI ");
        Serial.print(lastRssi);
        Serial.print(" dBm, SNR ");
        Serial.println(lastSnr);
      } else if (framesDropped > 0 || shortReads > 0) {
        // Surface capture problems from the last transmission even while idle,
        // so a dropping mic is not hidden behind a quiet receive path.
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
