/*
  Screen -- SSD1306 128x64 OLED via U8g2.

  The drawing code is lifted from test/OLEDscreen/U8g2/ (statusbar.cpp,
  menu.cpp, footbar.cpp) with the geometry and lopaka-generated bitmaps kept
  exactly as they were tested. What changed is only the plumbing: those files
  were standalone sketches that each declared their own global u8g2 and their
  own loop(), which cannot coexist in one firmware. Here they share one panel
  and are callable as methods.

  Rendering uses U8g2's full-frame buffer (the _F_ variant), so a redraw is
  build-then-flush rather than incremental. Redraws are not free -- a full
  128x64 flush over I2C takes a few milliseconds -- so call render() on state
  changes, not every loop iteration. Anything hard real-time (the audio path)
  should hold off redraws entirely; see lib/UI.
*/

#ifndef RANGER_SCREEN_H
#define RANGER_SCREEN_H

#include <Arduino.h>
#include <U8g2lib.h>

// Items rendered by drawMenu(). Geometry matches the tested menu.cpp layout:
// five 64x15 boxes in two columns.
#define SCREEN_MAX_MENU_ITEMS 5

struct MenuItem {
  int boxX, boxY, boxW, boxH;   // box position/size
  int textX, textY;             // text position
  const char *label;
};

class Screen {
public:
  // Wire.begin() must have been called first -- the PCF8575 shares the bus, so
  // whoever owns I2C setup does it once rather than each device doing its own.
  bool begin();

  // ---- Frame control ----
  // U8g2 full-buffer mode: beginFrame() clears, the draw* calls compose, and
  // endFrame() pushes the whole buffer out in one transfer.
  void beginFrame();
  void endFrame();

  // ---- Components, ported unchanged from the tested sketches ----
  // Top status bar: GPS bars, clock, battery. gpsSignal 0-3, battery 0.0-1.0.
  void drawStatus(int gpsSignal = 0, float batteryLevel = 0.5f,
                  const char *time = "");

  // Bottom soft-key labels.
  void drawFoot(const char *left = "", const char *middle = "", int posXm = 49,
                const char *right = "", int posXr = 95);

  // Menu list with the selected row inverted.
  void drawMenu(const MenuItem *items, int count, int selectedIndex);

  // ---- Whole pages ----
  // Big centred banner, used for TX/RX where the screen must be readable at a
  // glance and nothing else matters.
  void drawBanner(const char *title, const char *subtitle = "");

  // Link quality readout for the receive state.
  void drawLinkStats(const char *title, int rssi, float snr);

  // Single line, for boot progress and fatal errors.
  void drawMessage(const char *line);

  bool isReady() const { return ready; }

private:
  // The _F_ (full buffer) variant is what the tested sketches used; the page
  // variants would need the draw calls restructured into a firstPage/nextPage
  // loop, which is exactly the coupling this class exists to remove.
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C *u8g2 = nullptr;
  bool ready = false;
};

#endif  // RANGER_SCREEN_H
