// Display: SSD1306 128x64 OLED, I2C 0x3C (SDA=IO21 SCL=IO22, started by Controls).
// Screens follow the Lopaka designs. Draw with clear() ... show().
#pragma once
#include <Arduino.h>
#include <U8g2lib.h>

struct ListView { int sel = 0, top = 0; };      // selected row, first row on screen

class Display {
public:
  bool begin();
  void clear();
  void show();
  void setContrast(uint8_t c);

  // top bar, bottom labels, pop-up
  void statusBar(int gpsBars, int batteryPct, const char* time);
  void footBar(const char* left, const char* mid = "", const char* right = "");
  void toast(const char* text);

  // screens
  void logo();
  void menu(int sel);
  void people(const char* const* names, const bool* hasGps, int n, ListView& v);
  void options(const char* title, const char* const* items, int n, ListView& v);
  void location(bool fix, double lat, double lon, float heading);
  void track(const char* name, float arrowDeg, float bearing, float meters, int bars);
  void messages(const char* const* names, const char* const* times, const bool* unread, int n, ListView& v);
  void settings(const char* const* labels, const char* const* values, int n, ListView& v, bool editing);
  void talk(const char* name, bool talking, int seconds);

  // chat: history + (optional) the line being typed
  void chat(const char* name, const char* typing = nullptr);
  void chatAdd(bool mine, const char* text);    // wraps long messages into lines
  void chatClear() { _lines = 0; _scroll = 0; }
  void chatScroll(int d);                       // + = older

private:
  void keepVisible(ListView& v, int n, int rows);
  void selectRow(bool on, int x, int y, int w, int h);
  void arrow(int cx, int cy, float deg);
  void addLine(bool mine, const char* t);

  static const int MAX_LINES = 30;
  char _line[MAX_LINES][32];
  bool _mine[MAX_LINES];
  int  _lines = 0, _scroll = 0;
};
