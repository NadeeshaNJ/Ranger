#include <Arduino.h>
#include <driver/i2s.h>

// --- Microphone Pins (INMP441) ---
#define I2S_MIC_SCK 32
#define I2S_MIC_WS  15  
#define I2S_MIC_SD  33

// --- Speaker Pins (MAX98357A) ---
#define I2S_SPK_BCLK 27
#define I2S_SPK_LRC  26
#define I2S_SPK_DIN  25

// --- Audio Settings ---
#define SAMPLE_RATE 8000
#define RECORD_TIME_SEC 2

// 8kHz * 2 seconds * 2 bytes (16-bit Mono) = 32,000 bytes
const size_t BUFFER_SIZE = SAMPLE_RATE * RECORD_TIME_SEC * 2;
uint8_t* audio_buffer = nullptr; 

void setupI2S() {
  // 1. Configure Microphone on I2S_NUM_0
  i2s_config_t i2s_mic_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, // MONO
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false
  };
  i2s_pin_config_t mic_pins = {
    .bck_io_num = I2S_MIC_SCK,
    .ws_io_num = I2S_MIC_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_MIC_SD
  };
  i2s_driver_install(I2S_NUM_0, &i2s_mic_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &mic_pins);

  // 2. Configure Speaker on I2S_NUM_1
  i2s_config_t i2s_spk_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, // MATCH THE MIC (MONO)
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false
  };
  i2s_pin_config_t spk_pins = {
    .bck_io_num = I2S_SPK_BCLK,
    .ws_io_num = I2S_SPK_LRC,
    .data_out_num = I2S_SPK_DIN,
    .data_in_num = I2S_PIN_NO_CHANGE
  };
  i2s_driver_install(I2S_NUM_1, &i2s_spk_config, 0, NULL);
  i2s_set_pin(I2S_NUM_1, &spk_pins);
  
  // Clear the speaker buffer so it doesn't make static noise on boot
  i2s_zero_dma_buffer(I2S_NUM_1); 
}

void setup() {
  Serial.begin(115200);
  
  // Allocate memory in RAM 
  audio_buffer = (uint8_t*)malloc(BUFFER_SIZE);
  if (audio_buffer == nullptr) {
    Serial.println("Failed to allocate memory! Halting.");
    while (true);
  }
  Serial.println("Memory allocated successfully.");

  setupI2S();
  Serial.println("I2S initialized successfully.");
}

void loop() {
  size_t bytes_read = 0;
  size_t bytes_written = 0;

  // --- 1. RECORDING PHASE ---
  Serial.println("Recording for 2 seconds...");
  
  // Read from I2S_NUM_0 (Microphone)
  i2s_read(I2S_NUM_0, audio_buffer, BUFFER_SIZE, &bytes_read, portMAX_DELAY);
  
  Serial.println("Recording finished.");
  delay(500);

  // --- 2. PLAYBACK PHASE ---
  Serial.println("Playing back audio...");

  // Write to I2S_NUM_1 (Speaker)
  i2s_write(I2S_NUM_1, audio_buffer, BUFFER_SIZE, &bytes_written, portMAX_DELAY);

  Serial.println("Playback finished.");
  
  // Push out empty zeros to prevent the speaker from buzzing while idle
  uint8_t silence[1024] = {0};
  i2s_write(I2S_NUM_1, silence, sizeof(silence), &bytes_written, portMAX_DELAY);

  Serial.println("Waiting 2 seconds before repeating...");
  delay(2000); 
}