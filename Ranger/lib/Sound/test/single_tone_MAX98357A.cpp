#include <Arduino.h>
#include <driver/i2s.h>
#include <math.h>

#define MAX98357A_I2S_DIN  15
#define MAX98357A_I2S_LRC  26
#define MAX98357A_I2S_BCLK 27

#define SAMPLE_RATE 44100
#define TONE_FREQ   864
#define AMPLITUDE   16384 //max amplitude for 16-bit audio is 32767 and 16384 is around 50%

// MAX98357A I2S amplifier wiring
// LRC  -> IO26  (Left/Right Clock)
// BCLK -> IO27  (Bit Clock)
// DIN  -> IO25  (Digital Audio Data In)
//
// Gain select:
// - connect GAIN to GND for 12 dB gain
// - connect GAIN to VDD for 6 dB gain
//
// SD   -> leave unconnected or control with a GPIO if you want shutdown control
// GND  -> ground
// VIN  -> 5V supply

void setup() {
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
    .bck_io_num = MAX98357A_I2S_BCLK,
    .ws_io_num = MAX98357A_I2S_LRC,
    .data_out_num = MAX98357A_I2S_DIN,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
}

void loop() {
  int16_t buffer[128];
  static float phase = 0.0;
  float phase_inc = (TWO_PI * TONE_FREQ) / SAMPLE_RATE;
  size_t bytes_written;

  for (int i = 0; i < 64; i++) {
    int16_t val = (int16_t)(sin(phase) * AMPLITUDE);
    buffer[i * 2] = val;
    buffer[i * 2 + 1] = val;
    phase += phase_inc;
    if (phase >= TWO_PI) phase -= TWO_PI;
  }

  i2s_write(I2S_NUM_0, buffer, sizeof(buffer), &bytes_written, portMAX_DELAY);
}