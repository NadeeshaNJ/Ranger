#include "Screen.h"

// [BEGIN lopaka generated] -- copied verbatim from test/OLEDscreen/U8g2/statusbar.cpp
static const unsigned char image_battery_bits[] U8X8_PROGMEM =
    {0xff, 0x07, 0x01, 0x04, 0x01, 0x0c, 0x01, 0x04, 0xff, 0x07};
static const unsigned char image_gps_symbol_bits[] U8X8_PROGMEM =
    {0x02, 0x05, 0x1a, 0x1c, 0x2c, 0x50, 0x20};
static const unsigned char image_gps_bar1_bits[] U8X8_PROGMEM = {0x03, 0x03};
static const unsigned char image_gps_bar2_bits[] U8X8_PROGMEM = {0x03, 0x03, 0x03, 0x03};
static const unsigned char image_gps_bar3_bits[] U8X8_PROGMEM =
    {0x03, 0x03, 0x03, 0x03, 0x03, 0x03};
// [END lopaka generated]

bool Screen::begin() {
  u8g2 = new U8G2_SSD1306_128X64_NONAME_F_HW_I2C(U8G2_R0, U8X8_PIN_NONE);
  if (u8g2 == nullptr) return false;

  // U8g2's begin() returns void and does not report a missing panel, so this
  // cannot detect absent hardware. A disconnected OLED shows up as a silent
  // no-op rather than a boot failure; that is a U8g2 limitation, not a choice.
  u8g2->begin();
  u8g2->setFontMode(1);
  u8g2->setBitmapMode(1);
  ready = true;
  return true;
}

void Screen::beginFrame() {
  if (!ready) return;
  u8g2->clearBuffer();
}

void Screen::endFrame() {
  if (!ready) return;
  u8g2->sendBuffer();
}

void Screen::drawStatus(int gpsSignal, float batteryLevel, const char *time) {
  if (!ready) return;

  u8g2->setFontMode(1);
  u8g2->setBitmapMode(1);
  // status_line
  u8g2->drawLine(0, 8, 127, 8);
  // time
  u8g2->setFont(u8g2_font_haxrcorp4089_tr);
  u8g2->drawStr(53, 7, time);

  // battery icon plus fill level
  u8g2->drawXBMP(115, 1, 12, 5, image_battery_bits);
  if (batteryLevel >= 0.05f) u8g2->drawFrame(116, 2, 1, 3);
  if (batteryLevel >= 0.15f) u8g2->drawBox(116, 2, 3, 3);
  if (batteryLevel >= 0.40f) u8g2->drawBox(116, 2, 5, 3);
  if (batteryLevel >= 0.65f) u8g2->drawBox(116, 2, 7, 3);
  if (batteryLevel >= 0.90f) u8g2->drawBox(116, 2, 9, 3);

  // GPS symbol and signal bars
  if (gpsSignal > 0)  u8g2->drawXBMP(7, 0, 7, 7, image_gps_symbol_bits);
  if (gpsSignal >= 1) u8g2->drawXBMP(6, 5, 2, 2, image_gps_bar1_bits);
  if (gpsSignal >= 2) u8g2->drawXBMP(3, 3, 2, 4, image_gps_bar2_bits);
  if (gpsSignal >= 3) u8g2->drawXBMP(0, 1, 2, 6, image_gps_bar3_bits);
}

void Screen::drawFoot(const char *left, const char *middle, int posXm,
                      const char *right, int posXr) {
  if (!ready) return;

  u8g2->setFontMode(1);
  u8g2->setBitmapMode(1);
  u8g2->setFont(u8g2_font_5x7_tr);
  u8g2->drawStr(3, 62, left);
  u8g2->drawStr(posXm, 62, middle);
  u8g2->drawStr(posXr, 62, right);
}

void Screen::drawMenu(const MenuItem *items, int count, int selectedIndex) {
  if (!ready) return;

  u8g2->setFontMode(1);
  u8g2->setBitmapMode(1);
  u8g2->setFont(u8g2_font_profont11_tr);

  for (int i = 0; i < count; i++) {
    const MenuItem &item = items[i];

    if (i == selectedIndex) {
      // Selected: filled box + black text
      u8g2->setDrawColor(1);
      u8g2->drawBox(item.boxX, item.boxY, item.boxW, item.boxH);
      u8g2->setDrawColor(0);
      u8g2->drawStr(item.textX, item.textY, item.label);
    } else {
      // Not selected: white text only
      u8g2->setDrawColor(1);
      u8g2->drawStr(item.textX, item.textY, item.label);
    }
  }

  u8g2->setDrawColor(1);   // reset, or every later draw inherits colour 0
}

void Screen::drawBanner(const char *title, const char *subtitle) {
  if (!ready) return;

  u8g2->setDrawColor(1);
  u8g2->setFont(u8g2_font_profont22_tr);
  // Centre by measured width rather than a guessed offset, so titles of
  // different lengths stay centred.
  int w = u8g2->getStrWidth(title);
  u8g2->drawStr((128 - w) / 2, 34, title);

  if (subtitle != nullptr && subtitle[0] != '\0') {
    u8g2->setFont(u8g2_font_5x7_tr);
    w = u8g2->getStrWidth(subtitle);
    u8g2->drawStr((128 - w) / 2, 48, subtitle);
  }
}

void Screen::drawLinkStats(const char *title, int rssi, float snr) {
  if (!ready) return;

  char buf[24];
  u8g2->setDrawColor(1);
  u8g2->setFont(u8g2_font_profont22_tr);
  int w = u8g2->getStrWidth(title);
  u8g2->drawStr((128 - w) / 2, 30, title);

  u8g2->setFont(u8g2_font_5x7_tr);
  snprintf(buf, sizeof(buf), "RSSI %d dBm", rssi);
  u8g2->drawStr(4, 44, buf);
  snprintf(buf, sizeof(buf), "SNR  %.1f dB", (double)snr);
  u8g2->drawStr(4, 54, buf);
}

void Screen::drawMessage(const char *line) {
  if (!ready) return;

  u8g2->setDrawColor(1);
  u8g2->setFont(u8g2_font_5x7_tr);
  u8g2->drawStr(2, 20, line);
}
