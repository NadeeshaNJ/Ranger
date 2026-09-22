// Battery: LiPo voltage through the R15/R16 divider (100k/100k, so half) on IO14 (ADC2).
#pragma once
#include <Arduino.h>

class Battery {
public:
  void  begin();
  float volts();     // battery voltage
  int   percent();   // 0..100
};
