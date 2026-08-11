#include "Display.h"
#include <Wire.h>

bool Display::begin(int width, int height, uint8_t i2cAddr) {
  panel = new Adafruit_SSD1306(width, height, &Wire, -1);
  if (panel == nullptr) return false;

  if (!panel->begin(SSD1306_SWITCHCAPVCC, i2cAddr)) {
    delete panel;
    panel = nullptr;
    return false;
  }

  panel->clearDisplay();
  panel->setTextColor(SSD1306_WHITE);
  panel->setTextSize(1);
  panel->display();
  ready = true;
  return true;
}

void Display::showStatus(const char *state, int rssi, float snr) {
  if (!ready) return;

  panel->clearDisplay();
  panel->setCursor(0, 0);
  panel->setTextSize(2);
  panel->println(state);

  panel->setTextSize(1);
  panel->setCursor(0, 20);
  panel->printf("RSSI %d dBm", rssi);
  panel->setCursor(0, 30);
  panel->printf("SNR  %.1f dB", (double)snr);
  panel->display();
}

void Display::showMessage(const char *line) {
  if (!ready) return;

  panel->clearDisplay();
  panel->setTextSize(1);
  panel->setCursor(0, 0);
  panel->println(line);
  panel->display();
}
