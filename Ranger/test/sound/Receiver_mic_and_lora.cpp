#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>

#define ss   5
#define rst  14
#define dio0 2

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Voice over LoRa receiver starting...");

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
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    Serial.print("Voice chunk (");
    Serial.print(packetSize);
    Serial.print(" bytes) RSSI ");
    Serial.print(LoRa.packetRssi());
    Serial.print(" : ");

    for (int i = 0; i < packetSize; i++) {
      int8_t sample = (int8_t)LoRa.read();
      Serial.print(sample);
      Serial.print(" ");
    }
    Serial.println();
  }
}