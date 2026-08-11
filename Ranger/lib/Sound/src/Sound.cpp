#include "Sound.h"
#include <math.h>

bool Sound::begin(const SoundConfig &cfg) {
  config = cfg;

  i2s_config_t i2s_config = {};
  i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  i2s_config.sample_rate = cfg.sampleRate;
  i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  i2s_config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  i2s_config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  i2s_config.intr_alloc_flags = 0;
  i2s_config.dma_buf_count = cfg.dmaBufCount;
  i2s_config.dma_buf_len = cfg.dmaBufLen;
  i2s_config.use_apll = false;
  // Emit silence on underrun instead of replaying the last DMA buffer. Without
  // this a stalled feed turns into a loud repeating fragment rather than a gap.
  i2s_config.tx_desc_auto_clear = true;
  i2s_config.fixed_mclk = 0;

  i2s_pin_config_t pin_config = {};
  pin_config.bck_io_num = cfg.bclkPin;
  pin_config.ws_io_num = cfg.lrcPin;
  pin_config.data_out_num = cfg.dinPin;
  pin_config.data_in_num = I2S_PIN_NO_CHANGE;

  if (i2s_driver_install(cfg.port, &i2s_config, 0, NULL) != ESP_OK) return false;
  if (i2s_set_pin(cfg.port, &pin_config) != ESP_OK) return false;

  // One frame of stereo. dmaBufLen is in samples and is sized to hold at least
  // one audio frame, so this covers any playMono() the caller can make.
  stereoCapacity = cfg.dmaBufLen * 2;
  stereoBuf = (int16_t *)malloc(stereoCapacity * sizeof(int16_t));
  if (stereoBuf == nullptr) return false;

  installed = true;
  i2s_zero_dma_buffer(cfg.port);
  i2s_stop(cfg.port);      // stay silent until the caller asks for audio
  running = false;
  return true;
}

void Sound::start() {
  if (!installed || running) return;
  i2s_zero_dma_buffer(config.port);   // do not open with stale audio
  i2s_start(config.port);
  running = true;
}

void Sound::stop() {
  if (!installed || !running) return;
  i2s_zero_dma_buffer(config.port);   // do not leave the tail of a frame looping
  i2s_stop(config.port);
  running = false;
}

void Sound::zeroBuffer() {
  if (installed) i2s_zero_dma_buffer(config.port);
}

size_t Sound::playMono(const int16_t *samples, int count, int gain, uint32_t timeoutMs) {
  if (!installed || stereoBuf == nullptr) return 0;
  if (count * 2 > stereoCapacity) count = stereoCapacity / 2;

  for (int i = 0; i < count; i++) {
    // Widen before scaling so the multiply cannot wrap, then clamp.
    int32_t v = (int32_t)samples[i] * gain;
    if (v > 32767) v = 32767;
    if (v < -32768) v = -32768;
    stereoBuf[i * 2]     = (int16_t)v;
    stereoBuf[i * 2 + 1] = (int16_t)v;
  }

  size_t written = 0;
  i2s_write(config.port, stereoBuf, count * 2 * sizeof(int16_t), &written,
            pdMS_TO_TICKS(timeoutMs));
  return written;
}

void Sound::fillTone(int16_t *dst, int count, float toneHz, int16_t amplitude) {
  const float step = 2.0f * (float)M_PI * toneHz / (float)config.sampleRate;

  for (int i = 0; i < count; i++) {
    dst[i] = (int16_t)(amplitude * sinf(tonePhase));
    tonePhase += step;
    if (tonePhase >= 2.0f * (float)M_PI) tonePhase -= 2.0f * (float)M_PI;
  }
}
