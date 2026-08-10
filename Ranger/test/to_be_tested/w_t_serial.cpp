/*
  Walkie-Talkie over LoRa, Push-to-Talk
  ----------------------------------------------------------
  Flash this SAME sketch to both boards.
  Hold the PTT button to transmit your mic audio.
  Release it to listen and hear whatever the other board sends.

  Mic (INMP441, I2S_NUM_0, RX): SCK=32, WS=25, SD=33, L/R -> GND
  Speaker (PCM5100A, I2S_NUM_1, TX): WS(LRC)=26, BCK=27, DIN=15
  LoRa (Ra-01/SX1278): NSS=5, RST=14, DIO0=35
  PTT button: GPIO2 -> other leg to GND (uses internal pull-up)
*/

#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <driver/i2s.h>

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
// them floats and reads as permanently pressed. GPIO2 has a real pull-up.
#define PTT_PIN    12
#define DEBOUNCE_MS  50

// How often the receive path reports in. Per-packet logging would stall the
// I2S feed at 115200 baud, so packets are counted and summarised instead.
#define RX_REPORT_MS  1000

#define SAMPLE_RATE       16000
#define SAMPLES_PER_READ  512   // raw stereo int32 samples per mic read
#define CHUNK_SAMPLES     32    // quantized samples per LoRa packet, stays under payload limit

int32_t rawBuffer[SAMPLES_PER_READ];

void setupMicI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = SAMPLES_PER_READ,
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
    // ---- TRANSMIT: read mic, quantize, send over LoRa ----
    size_t bytesRead = 0;
    i2s_read(I2S_NUM_0, (void*)rawBuffer, sizeof(rawBuffer), &bytesRead, portMAX_DELAY);

    int samplesRead = bytesRead / sizeof(int32_t);
    if (samplesRead <= 0) return;

    // only every other index is real data (left channel)
    int usableCount = samplesRead / 2;
    int stride = usableCount / CHUNK_SAMPLES;
    if (stride < 1) stride = 1;

    int8_t chunk[CHUNK_SAMPLES];
    int chunkIndex = 0;

    for (int i = 0; i < samplesRead && chunkIndex < CHUNK_SAMPLES; i += 2 * stride) {
      // >>16 brings the 24-bit left-justified mic sample into 16-bit range (as
      // in the record/playback sketch); >>8 more scales that to int8 for the
      // link. The old >>14/>>8 pair discarded ~6 bits and quantized speech to 0.
      int32_t sample = rawBuffer[i] >> 16;
      int8_t q = (int8_t)constrain(sample >> 8, -128, 127);
      chunk[chunkIndex++] = q;
    }

    LoRa.beginPacket();
    LoRa.write((uint8_t*)chunk, chunkIndex);
    LoRa.endPacket();

  } else {
    // ---- RECEIVE: check for an incoming packet, play it immediately ----
    static unsigned long lastReport = 0;
    static int packetsSinceReport = 0;
    static long bytesSinceReport = 0;
    static int lastRssi = 0;
    static float lastSnr = 0;

    int packetSize = LoRa.parsePacket();
    if (packetSize) {
      int16_t playBuf[CHUNK_SAMPLES * 2]; // interleaved L/R
      int received = 0;

      while (LoRa.available() && received < CHUNK_SAMPLES) {
        int8_t sample = (int8_t)LoRa.read();
        int16_t expanded = ((int16_t)sample) << 8; // undo the quantization shift
        playBuf[received * 2]     = expanded;
        playBuf[received * 2 + 1] = expanded;
        received++;
      }

      // Sample the link quality here, before the next parsePacket() overwrites it.
      packetsSinceReport++;
      bytesSinceReport += packetSize;
      lastRssi = LoRa.packetRssi();
      lastSnr = LoRa.packetSnr();

      size_t bytesWritten = 0;
      i2s_write(I2S_NUM_1, playBuf, received * 2 * sizeof(int16_t), &bytesWritten, portMAX_DELAY);
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