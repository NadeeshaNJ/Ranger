// Display test: shows every screen with made-up data, 2 seconds each.
#include <Arduino.h>
#include "Controls.h"
#include "Display.h"

Controls ctl;                                  // starts I2C
Display oled;

const char* names[] = {"Police", "Fire Dept", "Medic 2", "Kamal"};
const bool  gps[]   = {true, false, true, true};
const char* times[] = {"12:01", "12:02", "12:05", "12:09"};
const bool  unread[] = {true, false, false, true};
const char* menuP[] = {"Track", "Ping", "Message", "Share", "Talk"};
const char* labels[] = {"Volume", "Brightness", "Channel"};
const char* values[] = {"70", "3/5", "9:2450"};

void setup() {
  Serial.begin(115200);
  ctl.begin();
  oled.begin();
  oled.chatAdd(false, "Where are you?");
  oled.chatAdd(true, "North gate");
  oled.chatAdd(false, "Need water and first aid at the north bridge");
}

void loop() {
  static int screen = 0;
  static float heading = 0;
  ListView v;
  v.sel = 1;
  oled.clear();
  if (screen > 0) oled.statusBar(3, 80, "14:35");
  switch (screen) {
    case 0: oled.logo(); break;
    case 1: oled.menu(2); break;
    case 2: oled.people(names, gps, 4, v); oled.footBar("Back", "People"); break;
    case 3: oled.options("Police", menuP, 5, v); break;
    case 4: oled.track("Police", heading, 265, 1234, 3); oled.footBar("Back", "Track", "Ping"); break;
    case 5: oled.location(true, 6.927079, 79.861244, heading); oled.footBar("Back", "Location", "Share"); break;
    case 6: oled.messages(names, times, unread, 4, v); oled.footBar("Back", "Messages"); break;
    case 7: oled.chat("Police"); oled.footBar("Back", "Reply"); break;
    case 8: oled.chat("Police", "On my way"); oled.footBar("<Del", "Send"); break;
    case 9: oled.settings(labels, values, 3, v, false); oled.footBar("Back", "Edit"); break;
    case 10: oled.talk("Everyone", true, 7); break;
    case 11: oled.menu(0); oled.toast("Ping sent"); break;
  }
  oled.show();
  heading += 30;
  delay(2000);
  screen = (screen + 1) % 12;
}
