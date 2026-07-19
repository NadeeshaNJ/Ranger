#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include "codec2.h"

#define ss   5
#define rst  14
#define dio0 2

struct CODEC2 *codec2;
int samplesPerFrame;
int bytesPerFrame;

uint8_t encodedFrame[16];
int16_t pcmFrame[160];

void codec2Task(void *parameter) {
  Serial.println("codec2Task (receiver) running on core: " + String(xPortGetCoreID()));

  while (1) {
    int packetSize = LoRa.parsePacket();

    if (packetSize == bytesPerFrame) {
      for (int i = 0; i < bytesPerFrame; i++) {
        encodedFrame[i] = LoRa.read();
      }

      codec2_decode(codec2, pcmFrame, encodedFrame);

      Serial.print("RSSI ");
      Serial.print(LoRa.packetRssi());
      Serial.print(" decoded samples: ");
      for (int i = 0; i < samplesPerFrame; i += 20) {
        Serial.print(pcmFrame[i]);
        Serial.print(" ");
      }
      Serial.println();

      // later: write pcmFrame out to the I2S DAC here for actual audio playback

    } else if (packetSize > 0) {
      Serial.print("Unexpected packet size: ");
      Serial.println(packetSize);
      while (LoRa.available()) LoRa.read();
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Codec2 voice receiver starting...");

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
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.enableCrc();
  Serial.println("LoRa Initializing OK!");

  // Pinned to Core 1, same as the sender, keeps Core 0 free for system/interrupt work
  xTaskCreatePinnedToCore(codec2Task, "codec2_task", 32768, NULL, 5, NULL, 1);

  Serial.println("setup() complete, codec2Task launched on Core 1");
}

void loop() {
  delay(1000);  // idle, all real work happens in codec2Task
}