#include <Arduino.h>
#include <driver/i2s.h>
#include <SPI.h>
#include <LoRa.h>

#define I2S_PORT      I2S_NUM_0
#define I2S_SCK_PIN   32
#define I2S_WS_PIN    25
#define I2S_SD_PIN    33

#define SAMPLE_RATE      16000
#define SAMPLES_PER_READ 512
#define CHUNK_SAMPLES    32   // stays well under LoRa's packet size limit

#define ss   5
#define rst  14
#define dio0 2

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
  Serial.println("Voice over LoRa sender starting...");

  setupI2S();

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
}

void loop() {
  size_t bytesRead = 0;
  i2s_read(I2S_PORT, (void*)rawBuffer, sizeof(rawBuffer), &bytesRead, portMAX_DELAY);

  int samplesRead = bytesRead / sizeof(int32_t);
  if (samplesRead <= 0) return;

  int8_t chunk[CHUNK_SAMPLES];
  int chunkIndex = 0;

  // only every other index is real data (left channel), so usable count is samplesRead/2
  int usableCount = samplesRead / 2;
  int stride = usableCount / CHUNK_SAMPLES;
  if (stride < 1) stride = 1;

  for (int i = 0; i < samplesRead && chunkIndex < CHUNK_SAMPLES; i += 2 * stride) {
    int32_t sample = rawBuffer[i] >> 14;
    int8_t q = (int8_t)constrain(sample >> 8, -128, 127);
    chunk[chunkIndex++] = q;
  }

  LoRa.beginPacket();
  LoRa.write((uint8_t*)chunk, chunkIndex);
  LoRa.endPacket();

  Serial.print("Sent voice chunk, bytes: ");
  Serial.println(chunkIndex);
}