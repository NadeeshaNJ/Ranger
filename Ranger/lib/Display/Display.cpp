#include "Display.h"

static U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

// Lopaka icons (XBM)
static const uint8_t I_BATTERY[] = {0xff,0x07,0x01,0x04,0x01,0x0c,0x01,0x04,0xff,0x07};             // 12x5
static const uint8_t I_SAT[]     = {0x02,0x05,0x1a,0x1c,0x2c,0x50,0x20};                            // 7x7
static const uint8_t I_PIN[]     = {0x1c,0x3e,0x63,0x63,0x77,0x3e,0x1c,0x1c,0x08};                  // 7x9
static const uint8_t I_MSG[]     = {0xff,0x01,0x01,0x01,0x55,0x01,0x01,0x01,0xff,0x01,0x18,0x00,0x04,0x00,0x02,0x00}; // 9x8
static const uint8_t I_MIC[]     = {0x1c,0x1c,0x5d,0x5d,0x41,0x7f,0x08,0x1c};                       // 7x8

static const char* MENU[] = {"People", "Location", "Messages", "Broadcast", "Settings"};
static const char* COMPASS[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};

bool Display::begin() {
  oled.begin();
  oled.setFontMode(1);
  oled.setBitmapMode(1);
  return true;
}

void Display::clear()                { oled.clearBuffer(); oled.setDrawColor(1); }
void Display::show()                 { oled.sendBuffer(); }
void Display::setContrast(uint8_t c) { oled.setContrast(c); }

// ---------------- helpers ----------------

void Display::keepVisible(ListView& v, int n, int rows) {
  v.sel = constrain(v.sel, 0, max(0, n - 1));
  if (v.sel < v.top) v.top = v.sel;
  if (v.sel >= v.top + rows) v.top = v.sel - rows + 1;
}

void Display::selectRow(bool on, int x, int y, int w, int h) {   // inverted box for the selected row
  oled.setDrawColor(1);
  if (on) { oled.drawBox(x, y, w, h); oled.setDrawColor(0); }
}

static const char* compass(float deg) { return COMPASS[(int)(fmod(deg + 382.5, 360) / 45) % 8]; }

// ---------------- bars ----------------

void Display::statusBar(int gpsBars, int batteryPct, const char* time) {
  oled.setDrawColor(1);
  oled.drawLine(0, 8, 127, 8);
  oled.setFont(u8g2_font_haxrcorp4089_tr);
  oled.drawStr(53, 7, time);
  oled.drawXBMP(115, 1, 12, 5, I_BATTERY);
  int fill = batteryPct >= 90 ? 9 : batteryPct >= 65 ? 7 : batteryPct >= 40 ? 5 : batteryPct >= 15 ? 3 : batteryPct >= 5 ? 1 : 0;
  if (fill) oled.drawBox(116, 2, fill, 3);
  if (gpsBars > 0) { oled.drawXBMP(7, 0, 7, 7, I_SAT); oled.drawBox(6, 5, 2, 2); }
  if (gpsBars > 1) oled.drawBox(3, 3, 2, 4);
  if (gpsBars > 2) oled.drawBox(0, 1, 2, 6);
}

void Display::footBar(const char* left, const char* mid, const char* right) {
  oled.setDrawColor(1);
  oled.setFont(u8g2_font_5x7_tr);
  oled.drawStr(3, 62, left);
  oled.drawStr((128 - oled.getStrWidth(mid)) / 2, 62, mid);
  oled.drawStr(125 - oled.getStrWidth(right), 62, right);
}

void Display::toast(const char* text) {
  oled.setFont(u8g2_font_profont11_tr);
  int w = oled.getStrWidth(text) + 12, x = (128 - w) / 2;
  oled.setDrawColor(0); oled.drawBox(x - 1, 23, w + 2, 19);
  oled.setDrawColor(1); oled.drawFrame(x, 24, w, 17); oled.drawFrame(x + 1, 25, w - 2, 15);
  oled.drawStr(x + 6, 36, text);
}

// ---------------- screens ----------------

void Display::logo() {
  oled.setFont(u8g2_font_profont29_tr);
  oled.drawStr(17, 42, "RANGER");
}

void Display::menu(int sel) {                              // 2 columns
  oled.setFont(u8g2_font_profont11_tr);
  for (int i = 0; i < 5; i++) {
    int col = i % 2, row = i / 2;
    selectRow(i == sel, col ? 64 : 1, 11 + row * 14, 64, 15);
    oled.drawStr(col ? 71 : 8, 22 + row * 14, MENU[i]);
  }
  oled.setDrawColor(1);
}

void Display::people(const char* const* names, const bool* hasGps, int n, ListView& v) {
  oled.setFont(u8g2_font_profont11_tr);
  if (n == 0) { oled.drawStr(6, 32, "No one nearby"); return; }
  keepVisible(v, n, 4);
  for (int r = 0; r < 4 && v.top + r < n; r++) {
    int i = v.top + r, y = 9 + r * 11;
    selectRow(i == v.sel, 1, y + 1, 126, 11);
    oled.drawStr(6, y + 9, names[i]);
    oled.drawXBMP(105, y + 2, 9, 8, I_MSG);
    oled.drawXBMP(118, y + 2, 7, 8, I_MIC);
    if (hasGps[i]) oled.drawXBMP(94, y + 2, 7, 9, I_PIN);
  }
  oled.setDrawColor(1);
}

void Display::options(const char* title, const char* const* items, int n, ListView& v) {
  oled.setFont(u8g2_font_profont15_tr);
  oled.drawStr(6, 21, title);
  oled.setFont(u8g2_font_profont10_tr);
  keepVisible(v, n, 5);
  for (int r = 0; r < 5 && v.top + r < n; r++) {
    int i = v.top + r, y = 8 + r * 11;
    selectRow(i == v.sel, 81, y, 47, 12);
    oled.drawStr(85, y + 8, items[i]);
  }
  oled.setDrawColor(1);
}

void Display::location(bool fix, double lat, double lon, float heading) {
  char s[32];
  oled.setFont(u8g2_font_profont12_tf);
  if (fix) {
    for (int k = 0; k < 2; k++) {                          // degrees, minutes, seconds
      double d = fabs(k ? lon : lat);
      long sec = lround(d * 3600);
      char hemi = k ? (lon >= 0 ? 'E' : 'W') : (lat >= 0 ? 'N' : 'S');
      snprintf(s, sizeof(s), "%ld\xC2\xB0%02ld'%02ld %c", sec / 3600, sec / 60 % 60, sec % 60, hemi);
      oled.drawUTF8(k ? 4 : 10, k ? 43 : 27, s);
    }
  } else {
    oled.drawStr(10, 27, "No GPS fix");
    oled.drawStr(10, 43, "searching..");
  }
  oled.setFont(u8g2_font_profont17_tf);
  snprintf(s, sizeof(s), "%d\xC2\xB0", (int)lround(heading) % 360);
  oled.drawUTF8(87, 30, s);
  oled.drawStr(91, 43, compass(heading));
  oled.drawLine(76, 13, 76, 50);
}

void Display::arrow(int cx, int cy, float deg) {          // dart shape, 0 deg = up
  float r = (deg - 90) * PI / 180, c = cos(r), s = sin(r);
  const float P[4][2] = {{14, 0}, {-14, -12.6}, {-8.4, 0}, {-14, 12.6}};   // tip, back top, notch, back bottom
  int x[4], y[4];
  for (int i = 0; i < 4; i++) { x[i] = lround(cx + P[i][0] * c - P[i][1] * s); y[i] = lround(cy + P[i][0] * s + P[i][1] * c); }
  oled.drawTriangle(x[0], y[0], x[1], y[1], x[2], y[2]);
  oled.drawTriangle(x[0], y[0], x[2], y[2], x[3], y[3]);
}

void Display::track(const char* name, float arrowDeg, float bearing, float meters, int bars) {
  char s[16];
  oled.setFont(u8g2_font_profont15_tr);
  oled.drawStr(6, 21, name);
  arrow(99, 31, arrowDeg);
  if (meters < 0)          strcpy(s, "--");
  else if (meters < 1000)  snprintf(s, sizeof(s), "%dm", (int)meters);
  else                     snprintf(s, sizeof(s), "%.1fkm", meters / 1000);
  oled.setFont(u8g2_font_6x12_tr);
  oled.drawStr(11, 33, s);
  oled.setFont(u8g2_font_6x10_tr);
  oled.drawStr(8, 43, compass(bearing));
  snprintf(s, sizeof(s), "%d\xC2\xB0", (int)lround(bearing) % 360);
  oled.setFont(u8g2_font_6x12_tf);
  oled.drawUTF8(29, 43, s);
  for (int b = 1; b <= bars && b <= 4; b++) oled.drawBox(35 - b * 4, 54 - b * 2, 2, b * 2);   // signal bars
}

void Display::messages(const char* const* names, const char* const* times, const bool* unread, int n, ListView& v) {
  oled.setFont(u8g2_font_profont11_tr);
  if (n == 0) { oled.drawStr(6, 32, "No messages"); return; }
  keepVisible(v, n, 4);
  for (int r = 0; r < 4 && v.top + r < n; r++) {
    int i = v.top + r, y = 8 + r * 11;
    selectRow(i == v.sel, 0, y, 127, 12);
    oled.drawStr(3, y + 9, names[i]);
    oled.drawStr(68, y + 9, times[i]);
    if (unread[i]) oled.drawFilledEllipse(116, y + 6, 2, 2);
  }
  oled.setDrawColor(1);
}

void Display::settings(const char* const* labels, const char* const* values, int n, ListView& v, bool editing) {
  char s[20];
  oled.setFont(u8g2_font_profont11_tr);
  keepVisible(v, n, 4);
  for (int r = 0; r < 4 && v.top + r < n; r++) {
    int i = v.top + r, y = 8 + r * 11;
    selectRow(i == v.sel, 0, y, 127, 12);
    oled.drawStr(3, y + 9, labels[i]);
    snprintf(s, sizeof(s), (i == v.sel && editing) ? "<%s>" : "%s", values[i]);
    oled.drawStr(124 - oled.getStrWidth(s), y + 9, s);
  }
  oled.setDrawColor(1);
}

void Display::talk(const char* name, bool talking, int seconds) {
  oled.setFont(u8g2_font_profont15_tr);
  oled.drawStr(6, 21, name);
  if (talking) {
    char s[16];
    oled.drawBox(0, 25, 128, 18);
    oled.setDrawColor(0);
    oled.drawStr(34, 39, "TALKING");
    oled.setDrawColor(1);
    snprintf(s, sizeof(s), "%d:%02d", seconds / 60, seconds % 60);
    oled.setFont(u8g2_font_profont11_tr);
    oled.drawStr(52, 53, s);
  } else {
    oled.setFont(u8g2_font_profont11_tr);
    oled.drawStr(6, 38, "Hold VOICE");
    oled.drawStr(6, 50, "to talk");
  }
}

// ---------------- chat ----------------

void Display::addLine(bool mine, const char* t) {
  if (_lines == MAX_LINES) {                               // full: forget the oldest line
    memmove(_line[0], _line[1], sizeof(_line[0]) * (MAX_LINES - 1));
    memmove(_mine, _mine + 1, MAX_LINES - 1);
    _lines--;
  }
  strcpy(_line[_lines], t);
  _mine[_lines++] = mine;
}

void Display::chatAdd(bool mine, const char* text) {
  // Word wrap: add words to the line while it fits in 95 px, then start a new line
  oled.setFont(u8g2_font_profont11_tr);
  char line[32] = "";
  while (true) {
    while (*text == ' ') text++;
    int w = strcspn(text, " ");                            // length of the next word
    char test[64];
    snprintf(test, sizeof(test), "%s%s%.*s", line, *line ? " " : "", w, text);
    if (w && strlen(test) < 32 && oled.getStrWidth(test) <= 95) {
      strcpy(line, test);                                  // the word fits: take it
      text += w;
      continue;
    }
    if (!*line && w) {                                     // one word wider than a line: cut it
      int k = min(w, 30);
      do { snprintf(line, sizeof(line), "%.*s", k, text); } while (oled.getStrWidth(line) > 95 && --k > 1);
      text += k;
    }
    if (*line) addLine(mine, line);
    line[0] = 0;
    if (!*text) break;
  }
  _scroll = 0;                                             // jump to the newest
}

void Display::chatScroll(int d) { _scroll = constrain(_scroll + d, 0, max(0, _lines - 2)); }

void Display::chat(const char* name, const char* typing) {
  int rows = typing ? 2 : 3;
  oled.setFont(u8g2_font_profont10_tr);
  oled.drawStr(3, 16, name);
  oled.drawFrame(0, 19, 128, 36);
  oled.setFont(u8g2_font_profont11_tr);
  int first = max(0, _lines - rows - _scroll);
  for (int r = 0; r < rows && first + r < _lines; r++) {
    const char* t = _line[first + r];
    oled.drawStr(_mine[first + r] ? 122 - oled.getStrWidth(t) : 3, 29 + r * 11, t);   // mine: right side
  }
  if (typing) {
    oled.drawLine(1, 43, 126, 43);
    const char* t = typing;
    while (*t && oled.getStrWidth(t) > 118) t++;           // show the end of what is typed
    oled.drawStr(3, 52, *typing ? t : "Typing!...");
  }
}
