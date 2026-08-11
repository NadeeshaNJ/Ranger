#include "Mic.h"

bool Mic::begin(const MicConfig &cfg) {
  config = cfg;

  i2s_config_t i2s_config = {};
  i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
  i2s_config.sample_rate = cfg.sampleRate;
  i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  i2s_config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  i2s_config.dma_buf_count = cfg.dmaBufCount;
  i2s_config.dma_buf_len = cfg.dmaBufLen;
  i2s_config.use_apll = false;
  i2s_config.tx_desc_auto_clear = false;
  i2s_config.fixed_mclk = 0;

  i2s_pin_config_t pin_config = {};
  pin_config.bck_io_num = cfg.sckPin;
  pin_config.ws_io_num = cfg.wsPin;
  pin_config.data_out_num = I2S_PIN_NO_CHANGE;
  pin_config.data_in_num = cfg.sdPin;

  if (i2s_driver_install(cfg.port, &i2s_config, 0, NULL) != ESP_OK) return false;
  if (i2s_set_pin(cfg.port, &pin_config) != ESP_OK) return false;

  rawCapacity = cfg.frameSamples * 2;   // stereo pairs
  rawBuf = (int32_t *)malloc(rawCapacity * sizeof(int32_t));
  if (rawBuf == nullptr) return false;

  installed = true;
  i2s_zero_dma_buffer(cfg.port);
  return true;
}

void Mic::flush() {
  if (installed) i2s_zero_dma_buffer(config.port);
}

bool Mic::readFrame(int16_t *dst, uint32_t timeoutMs) {
  if (!installed || rawBuf == nullptr) return false;

  size_t bytesRead = 0;
  // A bounded wait rather than portMAX_DELAY: one frame is a few tens of ms of
  // audio, so this is generous. If the mic stops clocking, this returns short
  // instead of blocking forever and taking the watchdog down with it.
  i2s_read(config.port, (void *)rawBuf, rawCapacity * sizeof(int32_t),
           &bytesRead, pdMS_TO_TICKS(timeoutMs));

  int stereoSamples = bytesRead / sizeof(int32_t);
  int collected = 0;

  // The frame arrives as [L, R, L, R, ...]. With the INMP441's L/R pin tied
  // low only the left slot carries data, so every second int32 is skipped.
  //
  // The device is 24-bit, left-justified in a 32-bit slot: the sample occupies
  // bits 31..8 and the low 8 bits are zero padding. Shifting right by 16 keeps
  // the top 16 bits, which is the correct 16-bit representation of a 24-bit
  // left-justified sample.
  for (int i = 0; i < stereoSamples && collected < config.frameSamples; i += 2) {
    int32_t sample = rawBuf[i] >> 16;
    if (sample > 32767) sample = 32767;
    if (sample < -32768) sample = -32768;
    dst[collected++] = (int16_t)sample;
  }

  return (collected == config.frameSamples);
}
