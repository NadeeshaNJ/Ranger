/*
  Display -- SSD1306 OLED status readout.

  Deliberately minimal: the walkie-talkie's audio path is hard real-time, and
  I2C writes are slow enough that redrawing on every loop iteration would eat
  into it. Call showStatus() on state changes, not continuously.

  The previous lib/OLEDscreen sources were empty placeholders; the U8g2 screens
  under test/OLEDscreen/U8g2/ are standalone sketches, not a library, so this
  starts from the Adafruit driver already in lib_deps.
*/

#ifndef RANGER_DISPLAY_H
#define RANGER_DISPLAY_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>

class Display {
public:
  // width/height must match the panel; 128x32 and 128x64 are the common ones.
  // i2cAddr is usually 0x3C. Returns false if the panel did not answer.
  bool begin(int width = 128, int height = 32, uint8_t i2cAddr = 0x3C);

  // Idle/receive/transmit banner plus link quality. Cheap to call on a state
  // change; do not call it per audio frame.
  void showStatus(const char *state, int rssi, float snr);

  // Free-form single line, for boot messages and errors.
  void showMessage(const char *line);

  bool isReady() const { return ready; }

private:
  Adafruit_SSD1306 *panel = nullptr;
  bool ready = false;
};

#endif  // RANGER_DISPLAY_H
