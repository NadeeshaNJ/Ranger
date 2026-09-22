#include "Speaker.h"
#include <driver/i2s.h>

#define SPK_PORT I2S_NUM_1
#define SPK_BCLK 27
#define SPK_LRC  26
#define SPK_DIN  17
#define SPK_RATE 8000

// Tone melodies: {Hz, ms} pairs, 0 Hz = pause. Edit freely.
static const uint16_t TONES[4][4][2] = {
  {{800, 60}, {1200, 60}},                        // 1 talk start
  {{1200, 60}, {800, 60}},                        // 2 talk end
  {{523, 100}, {659, 100}, {784, 160}},           // 3 power on
  {{2000, 200}, {0, 40}, {1500, 200}, {0, 40}},   // 4 alarm
};

bool Speaker::begin() {
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = SPK_RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;   // same sample goes to both slots
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.dma_buf_count = 4;
  cfg.dma_buf_len = 160;
  cfg.tx_desc_auto_clear = true;                     // plays silence (not a buzz) when starved

  i2s_pin_config_t pins = {};
  pins.mck_io_num = I2S_PIN_NO_CHANGE;
  pins.bck_io_num = SPK_BCLK;
  pins.ws_io_num = SPK_LRC;
  pins.data_out_num = SPK_DIN;
  pins.data_in_num = I2S_PIN_NO_CHANGE;

  _lock = xSemaphoreCreateMutex();
  setVolume(_vol);
  return i2s_driver_install(SPK_PORT, &cfg, 0, NULL) == ESP_OK &&
         i2s_set_pin(SPK_PORT, &pins) == ESP_OK;
}

void Speaker::setVolume(uint8_t v) {
  _vol = min(v, (uint8_t)100);
  _gain = (int32_t)_vol * _vol * 32768 / 10000;     // squared: each step sounds similar
}

void Speaker::play(const int16_t* pcm, size_t count) {
  int16_t stereo[128];                               // 64 samples at a time, copied to L and R
  xSemaphoreTake(_lock, portMAX_DELAY);
  for (size_t i = 0; i < count; ) {
    size_t n = min((size_t)64, count - i), written;
    for (size_t k = 0; k < n; k++) stereo[2 * k] = stereo[2 * k + 1] = (int32_t)pcm[i + k] * _gain >> 15;
    i2s_write(SPK_PORT, stereo, n * 4, &written, portMAX_DELAY);
    i += n;
  }
  xSemaphoreGive(_lock);
}

void Speaker::tone(uint16_t hz, uint16_t ms) {
  int16_t buf[80];
  int total = SPK_RATE * ms / 1000;
  float phase = 0, step = 2 * PI * hz / SPK_RATE;
  for (int i = 0; i < total; ) {
    int n = min(80, total - i);
    for (int k = 0; k < n; k++, i++) {
      float fade = min(1.0f, min(i, total - i) / 40.0f);   // 5 ms fade in/out: no clicks
      buf[k] = hz ? 12000 * fade * sinf(phase) : 0;
      phase += step;
    }
    play(buf, n);
  }
}

void Speaker::playTone(int n) {
  if (n < 1 || n > 4) return;
  for (auto& note : TONES[n - 1]) if (note[1]) tone(note[0], note[1]);
}
