// UI test: the full menu system with made-up people and chats.
// Use the real keys and knob. Every action is printed.
#include <Arduino.h>
#include "Controls.h"
#include "Display.h"
#include "Speaker.h"
#include "Ui.h"

Controls ctl;
Display oled;
Speaker spk;
Ui ui;

UiPerson people[] = {
  {"Police", true, 1234, 265, 3},
  {"Fire Dept", false, -1, 0, 1},
  {"Medic 2", true, 850, 40, 4},
};
UiChat chats[] = {{0, "12:01", true}, {2, "12:02", false}, {UI_ALL, "11:40", false}};
const char* ACTIONS[] = {"TALK_START", "TALK_STOP", "SEND_TEXT", "OPEN_CHAT", "PING",
                         "SHARE_POS", "SOS", "TRACK_ON", "TRACK_OFF", "SETTINGS"};

void onAction(UiAction a, int person, const char* text) {
  Serial.printf("%s  person %d  %s\n", ACTIONS[a], person, text ? text : "");
  if (a == A_OPEN_CHAT && person == 0) {       // pretend we had an earlier chat with Police
    ui.addMessage(false, "Where are you?");
    ui.addMessage(true, "North gate");
  }
}

void setup() {
  Serial.begin(115200);
  ctl.begin();
  oled.begin();
  spk.begin();
  ui.onAction(onAction);
  ui.setPeople(people, 3);
  ui.setChats(chats, 3);
  ui.setStatus(3, 80, "14:35");
  ui.setPosition(true, 6.927079, 79.861244, 0);
  ui.begin(ctl, oled, spk);
}

void loop() {
  ui.update();
}
