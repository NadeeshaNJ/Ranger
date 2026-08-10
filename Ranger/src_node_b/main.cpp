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

  Audio chain:
    mic 32-bit @8kHz -> 16-bit PCM -> Codec2 1300 -> LoRa -> Codec2 decode
    -> gain -> 16-bit stereo -> DAC

  Codec2 REQUIRES an 8 kHz sample rate, so both I2S peripherals run at 8 kHz
  here (the raw-sample sketches used 16 kHz).
*/

#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
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

// Codec2 only runs at 8 kHz.
#define SAMPLE_RATE   8000

// Mode 1300: 320 samples (40 ms) in, 52 bits = 7 bytes out, 25 frames/sec.
// Verified against codec2_samples_per_frame()/codec2_bits_per_frame() at boot.
#define CODEC2_MODE       CODEC2_MODE_1300
#define FRAME_SAMPLES     320
#define FRAME_BYTES       7

// A LoRa packet at SF7/125kHz costs ~36 ms of airtime almost regardless of
// payload size, so one frame per packet could never keep up with 25 frames/s.
// Batching 4 frames drops it to ~6.25 packets/sec at 28 bytes, which fits.
// The cost is ~160 ms of buffering latency each way.
#define FRAMES_PER_PACKET 4
#define PACKET_BYTES      (FRAME_BYTES * FRAMES_PER_PACKET)

// Playback gain. The PCM5100A is a line-level DAC with no amplifier, so this
// only maximises the signal it hands to whatever amp/headphones follow it --
// it cannot make a bare speaker loud. Raise carefully: too high just clips.
#define PLAYBACK_GAIN  6

struct CODEC2 *codec2State = NULL;

// Mic reads are stereo int32; only the left slot carries data (L/R tied low),
// so we need 2x the samples to fill one mono Codec2 frame.
int32_t rawBuffer[FRAME_SAMPLES * 2];
int16_t pcmFrame[FRAME_SAMPLES];
int16_t decodedFrame[FRAME_SAMPLES];
int16_t playBuf[FRAME_SAMPLES * 2];   // interleaved L/R
uint8_t packetBuf[PACKET_BYTES];

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
    .dma_buf_count = 8,
    .dma_buf_len = 64,
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

  codec2State = codec2_create(CODEC2_MODE);
  if (codec2State == NULL) {
    Serial.println("codec2_create failed, halting.");
    while (true) delay(1000);
  }

  // The buffer sizes above are compile-time constants; make sure the codec
  // actually agrees rather than silently overrunning them.
  int spf = codec2_samples_per_frame(codec2State);
  int bpf = (codec2_bits_per_frame(codec2State) + 7) / 8;
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
  i2s_read(I2S_NUM_0, (void*)rawBuffer, sizeof(rawBuffer), &bytesRead, portMAX_DELAY);

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

// Decode one frame and push it to the DAC with gain applied.
void playFrame(const uint8_t *bits) {
  codec2_decode(codec2State, decodedFrame, (unsigned char *)bits);

  for (int i = 0; i < FRAME_SAMPLES; i++) {
    // Widen to 32-bit before scaling so the multiply cannot wrap, then clamp.
    int32_t amplified = (int32_t)decodedFrame[i] * PLAYBACK_GAIN;
    if (amplified > 32767) amplified = 32767;
    if (amplified < -32768) amplified = -32768;

    playBuf[i * 2]     = (int16_t)amplified;
    playBuf[i * 2 + 1] = (int16_t)amplified;
  }

  size_t bytesWritten = 0;
  i2s_write(I2S_NUM_1, playBuf, FRAME_SAMPLES * 2 * sizeof(int16_t), &bytesWritten, portMAX_DELAY);
}

void loop() {
  bool talking = pttPressed();
  static bool wasTalking = false;

  if (talking && !wasTalking) {
    i2s_stop(I2S_NUM_1); // mute speaker the moment PTT is pressed
    Serial.println("PTT pressed, transmitting...");
  }
  if (!talking && wasTalking) {
    i2s_start(I2S_NUM_1); // bring speaker back to life when PTT is released
    Serial.println("PTT released, listening...");
  }
  wasTalking = talking;

  if (talking) {
    // ---- TRANSMIT: mic -> Codec2 -> batch of frames -> LoRa ----
    int framesReady = 0;

    while (framesReady < FRAMES_PER_PACKET) {
      if (!readMicFrame()) return;   // short read, drop this batch
      codec2_encode(codec2State, &packetBuf[framesReady * FRAME_BYTES], pcmFrame);
      framesReady++;

      // Bail out mid-batch if PTT was released, so we don't keep keying.
      if (!pttPressed()) break;
    }

    if (framesReady > 0) {
      LoRa.beginPacket();
      LoRa.write(packetBuf, framesReady * FRAME_BYTES);
      LoRa.endPacket();
    }

  } else {
    // ---- RECEIVE: LoRa -> Codec2 decode -> gain -> DAC ----
    static unsigned long lastReport = 0;
    static int packetsSinceReport = 0;
    static long bytesSinceReport = 0;
    static int lastRssi = 0;
    static float lastSnr = 0;

    int packetSize = LoRa.parsePacket();
    if (packetSize) {
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

      for (int f = 0; f < framesIn; f++) {
        playFrame(&packetBuf[f * FRAME_BYTES]);
      }
    }

    unsigned long now = millis();
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
      } else {
        Serial.println("RX idle (no packets)");
      }
      packetsSinceReport = 0;
      bytesSinceReport = 0;
    }
  }
}
