// Mic: ICS-43434 I2S microphone.  SCK=IO32  WS=IO25  SD=IO33  (I2S port 0)
#pragma once
#include <Arduino.h>

class Mic {
public:
  bool begin();                              // 8 kHz, returns false if the mic stays silent
  size_t read(int16_t* out, size_t count);   // waits until 'count' samples are read
  void sleep();                              // stops the clock, mic sleeps
  void wake();
private:
  int _slot = 0;                             // which stereo slot the mic uses
};
