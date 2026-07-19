/*
  Record 2 seconds from INMP441, play it back on MAX98357A
  ----------------------------------------------------------
  Mic  (I2S_NUM_0, RX): SCK=32, WS=25, SD=33, L/R -> GND
  Speaker (I2S_NUM_1, TX): BCLK=27, LRC=26, DIN=15

  Flow: record 2s into RAM buffer -> play it back -> pause -> repeat
*/

#include <Arduino.h>
#include <driver/i2s.h>

// ---- Mic pins (I2S_NUM_0) ----
#define MIC_SCK_PIN   32
#define MIC_WS_PIN    25
#define MIC_SD_PIN    33

// ---- Speaker pins (I2S_NUM_1) ----
#define SPK_BCLK_PIN  27
#define SPK_LRC_PIN   26
#define SPK_DIN_PIN   15

#define SAMPLE_RATE       16000
#define RECORD_SECONDS    2
#define TOTAL_SAMPLES     (SAMPLE_RATE * RECORD_SECONDS)   // 32000 mono samples
#define READ_CHUNK        512                              // stereo int32 samples per i2s_read

// 32000 samples * 2 bytes = 64000 bytes, fine for ESP32 SRAM
static int16_t audioBuffer[TOTAL_SAMPLES];

int32_t rawReadBuf[READ_CHUNK];
int16_t writeBuf[READ_CHUNK]; // holds interleaved L/R for playback of one chunk

void setupMicI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = READ_CHUNK,
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
    .bck_io_num = SPK_BCLK_PIN,
    .ws_io_num = SPK_LRC_PIN,
    .data_out_num = SPK_DIN_PIN,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_NUM_1, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_1, &pin_config);
}

void recordAudio() {
  Serial.println("Recording for 2 seconds...");
  int samplesCollected = 0;
  size_t bytesRead = 0;

  while (samplesCollected < TOTAL_SAMPLES) {
    i2s_read(I2S_NUM_0, (void*)rawReadBuf, sizeof(rawReadBuf), &bytesRead, portMAX_DELAY);
    int stereoSamplesRead = bytesRead / sizeof(int32_t);

    // Data arrives as [Left, Right, Left, Right, ...]. Real mic data is
    // in the LEFT slot (index 0, 2, 4...) since L/R is tied to GND.
    for (int i = 0; i < stereoSamplesRead && samplesCollected < TOTAL_SAMPLES; i += 2) {
      // Raw mic sample is a 24-bit value left-justified in 32 bits.
      // Shifting right by 16 brings it down to a proper 16-bit range.
      int32_t sample = rawReadBuf[i] >> 16;

      if (sample > 32767) sample = 32767;
      if (sample < -32768) sample = -32768;

      audioBuffer[samplesCollected] = (int16_t)sample;
      samplesCollected++;
    }
  }

  Serial.println("Recording done.");
}

void playAudio() {
  Serial.println("Playing back...");
  size_t bytesWritten = 0;

  for (int i = 0; i < TOTAL_SAMPLES; i += (READ_CHUNK / 2)) {
    int chunkSamples = min(READ_CHUNK / 2, TOTAL_SAMPLES - i);

    // Duplicate mono sample into L and R slots, same as the tone-generator code
    for (int j = 0; j < chunkSamples; j++) {
      writeBuf[j * 2]     = audioBuffer[i + j];
      writeBuf[j * 2 + 1] = audioBuffer[i + j];
    }

    i2s_write(I2S_NUM_1, writeBuf, chunkSamples * 2 * sizeof(int16_t), &bytesWritten, portMAX_DELAY);
  }

  Serial.println("Playback done.");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  setupMicI2S();
  setupSpeakerI2S();
  Serial.println("Ready. Will record 2s, then play it back, on repeat.");
}

void loop() {
  recordAudio();
  playAudio();
  delay(1000); // pause before next cycle
}