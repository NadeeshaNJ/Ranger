/*
  Walkie-Talkie over LoRa, Push-to-Talk
  ----------------------------------------------------------
  Flash this SAME sketch to both boards.
  Hold the PTT button to transmit your mic audio.
  Release it to listen and hear whatever the other board sends.

  Mic (INMP441, I2S_NUM_0, RX): SCK=32, WS=25, SD=33, L/R -> GND
  Speaker (PCM5100A, I2S_NUM_1, TX): WS(LRC)=26, BCK=27, DIN=25
  LoRa (Ra-01/SX1278): NSS=5, RST=14, DIO0=35
  PTT button: GPIO4 -> other leg to GND (uses internal pull-up)
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
#define SPK_DIN_PIN   25

// ---- LoRa pins ----
#define LORA_SS    5
#define LORA_RST   14
#define LORA_DIO0  35

// ---- PTT button ----
#define PTT_PIN    4

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

void loop() {
  bool talking = (digitalRead(PTT_PIN) == LOW);
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
      int32_t sample = rawBuffer[i] >> 14;
      int8_t q = (int8_t)constrain(sample >> 8, -128, 127);
      chunk[chunkIndex++] = q;
    }

    LoRa.beginPacket();
    LoRa.write((uint8_t*)chunk, chunkIndex);
    LoRa.endPacket();

  } else {
    // ---- RECEIVE: check for an incoming packet, play it immediately ----
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

      size_t bytesWritten = 0;
      i2s_write(I2S_NUM_1, playBuf, received * 2 * sizeof(int16_t), &bytesWritten, portMAX_DELAY);
    }
  }
}