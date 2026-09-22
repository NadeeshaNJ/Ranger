// Speaker: MAX98357A I2S amplifier.  BCLK=IO27  LRC=IO26  DIN=IO17  (I2S port 1)
// SD_MODE (amp on/off) is on the expander: see Controls::setAmp()
#pragma once
#include <Arduino.h>

class Speaker {
public:
  bool begin();
  void play(const int16_t* pcm, size_t count);   // 8 kHz mono; waits while the buffer is full
  void tone(uint16_t hz, uint16_t ms);          // hz = 0 plays silence
  void playTone(int n);                         // 1 talk start, 2 talk end, 3 power on, 4 alarm
  void setVolume(uint8_t v);                    // 0..100
  uint8_t volume() { return _vol; }
private:
  SemaphoreHandle_t _lock;                      // voice task and UI beeps may play at the same time
  uint8_t _vol = 70;
  int32_t _gain = 0;
};
