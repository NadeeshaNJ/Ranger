#include <Arduino.h>
#include <driver/i2s.h>

// I2S pin definitions for ESP32
#define I2S_BCK  26  // Bit clock pin
#define I2S_LRCK 25  // Left-right clock pin (word select / LRCK / WS)
#define I2S_DOUT 22  // Data out pin

void setup() {
  Serial.begin(115200);
  delay(100);

  // Configure I2S driver
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };

  esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.println("Failed to install I2S driver!");
    while (1);
  }

  // Set I2S pins
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_BCK,
    .ws_io_num = I2S_LRCK,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  err = i2s_set_pin(I2S_NUM_0, &pin_config);
  if (err != ESP_OK) {
    Serial.println("Failed to set I2S pins!");
    while (1);
  }

  Serial.println("I2S initialized successfully.");
}

void loop() {
  // Example: Send a simple sine wave to the speaker module
  static float phase = 0.0;
  const float frequency = 440.0; // Frequency of the sine wave (A4 note)
  const float amplitude = 16000.0; // Reduced amplitude for safety
  const float sampleRate = 44100.0; // Sampling rate in Hz

  // Generate a sine wave sample (mono interleaved into 16-bit frame)
  int16_t sample = (int16_t)(amplitude * sin(phase));
  phase += 2.0 * PI * frequency / sampleRate;
  if (phase >= 2.0 * PI) phase -= 2.0 * PI;

  // Write the sample to I2S (blocking)
  size_t bytes_written = 0;
  i2s_write(I2S_NUM_0, &sample, sizeof(sample), &bytes_written, portMAX_DELAY);
}