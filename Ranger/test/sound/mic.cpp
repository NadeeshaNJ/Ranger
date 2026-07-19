/*
  INMP441 I2S Mic Test for ESP32 (PlatformIO)
  ---------------------------------------------
  No Serial Plotter needed. This prints a live ASCII bar meter
  to the normal Serial Monitor so you can SEE the mic responding
  to sound in real time.

  Wiring (matches Ranger project pin plan):
    INMP441 SCK  -> GPIO32
    INMP441 WS   -> GPIO25
    INMP441 SD   -> GPIO33
    INMP441 L/R  -> GND   (selects left channel)
    INMP441 VDD  -> 3.3V
    INMP441 GND  -> GND

  platformio.ini:
    [env:esp32dev]
    platform = espressif32
    board = esp32dev
    framework = arduino
    monitor_speed = 115200
*/

#include <Arduino.h>
#include <driver/i2s.h>

#define I2S_PORT      I2S_NUM_0
#define I2S_SCK_PIN   32
#define I2S_WS_PIN    25
#define I2S_SD_PIN    33

#define SAMPLE_RATE   16000
#define SAMPLES_PER_READ 512

int32_t rawBuffer[SAMPLES_PER_READ];

void setupI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = SAMPLES_PER_READ,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK_PIN,
    .ws_io_num = I2S_WS_PIN,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD_PIN
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
  i2s_zero_dma_buffer(I2S_PORT);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("INMP441 I2S mic test starting...");
  setupI2S();
}

void loop() {
  size_t bytesRead = 0;
  i2s_read(I2S_PORT, (void*)rawBuffer, sizeof(rawBuffer), &bytesRead, portMAX_DELAY);

  int samplesRead = bytesRead / sizeof(int32_t);
  if (samplesRead <= 0) return;

  int32_t peak = 0;
  double sumSquares = 0;
  int usedSamples = 0;

  // Data now arrives as [Left, Right, Left, Right, ...] pairs.
  // With L/R pin tied to GND, the mic's real data is in the LEFT slot,
  // which is index 0, 2, 4... Step by 2 and skip the empty right slot.
  for (int i = 0; i < samplesRead; i += 2) {
    // INMP441 outputs 24-bit data left-justified in a 32-bit word.
    // Shifting right brings it into a usable range for peak/RMS math.
    int32_t sample = rawBuffer[i] >> 14;

    int32_t absSample = abs(sample);
    if (absSample > peak) peak = absSample;
    sumSquares += (double)sample * (double)sample;
    usedSamples++;
  }

  double rms = (usedSamples > 0) ? sqrt(sumSquares / usedSamples) : 0;

  // Scale peak (0 to ~131071 after the >>14 shift) down to a 0 to 50 char bar
  int barLength = map(peak, 0, 20000, 0, 50);
  if (barLength > 50) barLength = 50;
  if (barLength < 0) barLength = 0;

  Serial.print("[");
  for (int i = 0; i < 50; i++) {
    Serial.print(i < barLength ? '#' : '-');
  }
  Serial.print("] ");
  Serial.print("Peak:");
  Serial.print(peak);
  Serial.print("  RMS:");
  Serial.print((int)rms);

  if (peak == 0 && rms == 0) {
    Serial.print("  <-- SILENCE. Check wiring (SCK/WS/SD pins, L/R to GND, 3.3V power)");
  }

  Serial.println();
}