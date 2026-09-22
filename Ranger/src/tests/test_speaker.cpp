// Speaker test: plays the 4 tones, then keys in the serial monitor:
//   1..4 = tone 1..4    b = 1 kHz beep    + / - = volume
#include <Arduino.h>
#include "Speaker.h"
#include "Controls.h"

Speaker spk;
Controls ctl;                                  // only to switch the amp on (SD_MODE)

void setup() {
  Serial.begin(115200);
  ctl.begin();
  ctl.setAmp(true);
  Serial.println(spk.begin() ? "Speaker OK" : "Speaker FAILED");
  for (int t = 1; t <= 4; t++) { Serial.printf("tone %d\n", t); spk.playTone(t); delay(400); }
}

void loop() {
  if (!Serial.available()) return;
  char c = Serial.read();
  if (c >= '1' && c <= '4') spk.playTone(c - '0');
  if (c == 'b') spk.tone(1000, 300);
  if (c == '+') spk.setVolume(spk.volume() + 10);
  if (c == '-') spk.setVolume(spk.volume() >= 10 ? spk.volume() - 10 : 0);
  if (c == '+' || c == '-') { Serial.printf("volume %d\n", spk.volume()); spk.tone(1000, 150); }
}
