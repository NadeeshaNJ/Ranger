#include "Mic.h"
#include <driver/i2s.h>

#define MIC_PORT  I2S_NUM_0
#define MIC_SCK   32
#define MIC_WS    25
#define MIC_SD    33
#define MIC_SHIFT 14    // mic gives 24-bit data in a 32-bit word; >>14 = 16-bit with some gain

bool Mic::begin() {
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
  cfg.sample_rate = 8000;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;   // "ONLY_LEFT" returns zeros on ESP32
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.dma_buf_count = 4;
  cfg.dma_buf_len = 160;

  i2s_pin_config_t pins = {};
  pins.mck_io_num = I2S_PIN_NO_CHANGE;               // keep MCLK off IO0 (boot pin)
  pins.bck_io_num = MIC_SCK;
  pins.ws_io_num = MIC_WS;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = MIC_SD;

  if (i2s_driver_install(MIC_PORT, &cfg, 0, NULL) != ESP_OK) return false;
  if (i2s_set_pin(MIC_PORT, &pins) != ESP_OK) return false;

  // Listen for ~160 ms (this also skips the mic's start-up time) and
  // find which of the two stereo slots carries the sound.
  int32_t buf[64];
  size_t bytes;
  int64_t level[2] = {0, 0};
  for (int k = 0; k < 40; k++) {
    i2s_read(MIC_PORT, buf, sizeof(buf), &bytes, portMAX_DELAY);
    for (size_t i = 0; i < bytes / 4; i++) level[i % 2] += abs(buf[i] >> 8);
  }
  _slot = level[1] > level[0] ? 1 : 0;
  return level[0] + level[1] > 0;
}

size_t Mic::read(int16_t* out, size_t count) {
  int32_t buf[64];                                   // 32 stereo pairs at a time
  size_t done = 0;
  while (done < count) {
    size_t pairs = min((size_t)32, count - done), bytes = 0;
    i2s_read(MIC_PORT, buf, pairs * 8, &bytes, portMAX_DELAY);
    for (size_t i = 0; i < bytes / 8; i++) {
      int32_t s = buf[2 * i + _slot] >> MIC_SHIFT;
      out[done++] = constrain(s, -32768, 32767);
    }
  }
  return done;
}

void Mic::sleep() { i2s_stop(MIC_PORT); }
void Mic::wake()  { i2s_start(MIC_PORT); delay(100); }   // mic needs ~64 ms to wake
