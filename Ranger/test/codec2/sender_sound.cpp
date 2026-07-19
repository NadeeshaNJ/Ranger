#include <Arduino.h>
#include <driver/i2s.h>
#include <SPI.h>
#include <LoRa.h>
#include "codec2.h"

#define I2S_PORT      I2S_NUM_0
#define I2S_SCK_PIN   32
#define I2S_WS_PIN    25
#define I2S_SD_PIN    33

#define SAMPLE_RATE   8000

#define ss   5
#define rst  14
#define dio0 2

struct CODEC2 *codec2;
int samplesPerFrame;
int bytesPerFrame;

int32_t rawBuffer[512];
int16_t pcmFrame[160];
uint8_t encodedFrame[16];

void setupI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 160,
    .use_apll = true,
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

void codec2Task(void *parameter) {
  Serial.println("codec2Task (sender) running on core: " + String(xPortGetCoreID()));

  while (1) {
    size_t bytesRead = 0;
    i2s_read(I2S_PORT, (void*)rawBuffer, samplesPerFrame * sizeof(int32_t), &bytesRead, pdMS_TO_TICKS(1000));

    int samplesRead = bytesRead / sizeof(int32_t);
    if (samplesRead < samplesPerFrame) {
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }

    for (int i = 0; i < samplesPerFrame; i++) {
      int32_t sample = rawBuffer[i] >> 14;
      pcmFrame[i] = (int16_t)constrain(sample, -32768, 32767);
    }

    codec2_encode(codec2, encodedFrame, pcmFrame);

    LoRa.beginPacket();
    LoRa.write(encodedFrame, bytesPerFrame);
    LoRa.endPacket();

    Serial.print("Sent encoded frame, bytes: ");
    Serial.println(bytesPerFrame);

    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Codec2 voice sender starting...");

  setupI2S();

  codec2 = codec2_create(CODEC2_MODE_3200);
  samplesPerFrame = codec2_samples_per_frame(codec2);
  bytesPerFrame = (codec2_bits_per_frame(codec2) + 7) / 8;

  Serial.print("Samples per frame: ");
  Serial.println(samplesPerFrame);
  Serial.print("Bytes per frame: ");
  Serial.println(bytesPerFrame);

  LoRa.setPins(ss, rst, dio0);
  while (!LoRa.begin(433E6)) {
    Serial.println(".");
    delay(500);
  }
  LoRa.setSyncWord(0xF3);
  LoRa.setTxPower(20, PA_OUTPUT_PA_BOOST_PIN);
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.enableCrc();
  Serial.println("LoRa Initializing OK!");

  // Pinned to Core 1, leaves Core 0 free for I2S DMA interrupts and system tasks
  xTaskCreatePinnedToCore(codec2Task, "codec2_task", 32768, NULL, 5, NULL, 1);

  Serial.println("setup() complete, codec2Task launched on Core 1");
}

void loop() {
  delay(1000);  // Arduino's default loop task is now idle, all work happens in codec2Task
}