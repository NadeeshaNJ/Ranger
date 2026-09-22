// Ranger: starts every module and passes data between them. The modules do the work.
#include <Arduino.h>
#include "Controls.h"
#include "Display.h"
#include "Speaker.h"
#include "Mic.h"
#include "Radio.h"
#include "Net.h"
#include "Crypto.h"
#include "Voice.h"
#include "Gps.h"
#include "Battery.h"
#include "Ui.h"

#define MY_NAME    "Ranger-1"                 // change on each device
#define PASSPHRASE "rescue-team-2026"         // the same on every device

Controls ctl; Display oled; Speaker spk; Mic mic; Radio radio; Net net;
Crypto crypto; Voice voice; Gps gps; Battery battery; Ui ui;

// Messages: the last 20, and one chat per person (NET_ALL = the "everyone" chat)
struct Msg  { uint16_t peer; bool mine; char text[80]; };
struct Chat { uint16_t peer; char time[6]; bool unread; };
Msg  msgs[20]; int nMsgs = 0;
Chat chats[9]; int nChats = 0;
uint16_t openChat = 0;                         // chat last opened on screen
char timeNow[6] = "--:--";

uint16_t idOf(int person) { return person == UI_ALL ? NET_ALL : net.peer(person)->id; }
int personOf(uint16_t id) {
  for (int i = 0; i < net.peerCount(); i++) if (net.peer(i)->id == id) return i;
  return UI_ALL;
}

void remember(uint16_t peer, bool mine, const char* text) {
  if (nMsgs == 20) { memmove(msgs, msgs + 1, sizeof(Msg) * 19); nMsgs--; }
  msgs[nMsgs].peer = peer;
  msgs[nMsgs].mine = mine;
  strncpy(msgs[nMsgs++].text, text, 79);
  Chat* c = nullptr;
  for (int i = 0; i < nChats; i++) if (chats[i].peer == peer) c = &chats[i];
  if (!c && nChats < 9) { c = &chats[nChats++]; c->peer = peer; }
  if (c) { strcpy(c->time, timeNow); c->unread = !mine && peer != openChat; }
  if (!mine && peer == openChat) ui.addMessage(false, text);   // the UI shows my own lines itself
}

// ---------------- Net -> screen / speaker ----------------
void onNet(const NetEvent& e) {
  voice.onNetEvent(e);                                          // voice packets go to Voice
  Peer* p = net.findPeer(e.from);
  const char* name = p ? p->name : "?";
  char s[100];
  switch (e.type) {
    case EV_TEXT:
      if (e.toAll) snprintf(s, sizeof(s), "%s: %.*s", name, e.len, (const char*)e.data);
      else         snprintf(s, sizeof(s), "%.*s", e.len, (const char*)e.data);
      remember(e.toAll ? NET_ALL : e.from, false, s);
      snprintf(s, 24, "Msg: %s", name);
      ui.toast(s);
      spk.tone(1500, 80);
      break;
    case EV_SENT:   ui.toast("Delivered"); break;
    case EV_FAILED: ui.toast("Not delivered"); break;
    case EV_PONG:   snprintf(s, 24, "Pong %lu ms", (unsigned long)e.rttMs); ui.toast(s); break;
    case EV_SOS:    snprintf(s, 24, "SOS: %s", name); ui.toast(s); spk.playTone(4); break;
  }
}

// ---------------- UI -> Net / Voice ----------------
void onAction(UiAction a, int person, const char* text) {
  uint16_t id = idOf(person);
  switch (a) {
    case A_TALK_START: voice.talk(true, id); break;
    case A_TALK_STOP:  voice.talk(false); break;
    case A_PING:       net.ping(id); break;
    case A_SHARE_POS:  net.sharePosition(); break;
    case A_SOS:        net.sendSos("SOS"); break;
    case A_TRACK_ON:   net.track(id, true); break;
    case A_TRACK_OFF:  net.track(id, false); break;
    case A_SETTINGS:   radio.setChannel(ui.channel); break;
    case A_SEND_TEXT:
      if (net.sendText(id, text) < 0) ui.toast("Busy, try again");
      else remember(id, true, text);
      break;
    case A_OPEN_CHAT:                                           // show this chat's history
      openChat = id;
      for (int i = 0; i < nMsgs; i++) if (msgs[i].peer == id) ui.addMessage(msgs[i].mine, msgs[i].text);
      for (int i = 0; i < nChats; i++) if (chats[i].peer == id) chats[i].unread = false;
      break;
  }
}

// ---------------- once a second: GPS, battery, people -> screen ----------------
void refresh() {
  static UiPerson people[8];
  static UiChat list[9];
  bool fix = gps.hasFix();
  gps.timeText(timeNow);
  ui.setStatus(gps.bars(), battery.percent(), timeNow);
  ui.setPosition(fix, gps.lat(), gps.lon(), gps.course());
  net.setPosition(fix, gps.lat(), gps.lon());

  for (int i = 0; i < net.peerCount(); i++) {
    Peer* p = net.peer(i);
    bool both = fix && p->hasPos;
    float margin = p->snr + 12.5;                               // dB above the SF9 decoding limit
    people[i] = { p->name, p->hasPos,
                  both ? Net::distanceM(gps.lat(), gps.lon(), p->lat, p->lon) : -1,
                  both ? Net::bearingDeg(gps.lat(), gps.lon(), p->lat, p->lon) : 0,
                  !p->direct ? 1 : margin >= 15 ? 4 : margin >= 10 ? 3 : margin >= 5 ? 2 : 1 };
  }
  for (int i = 0; i < nChats; i++) list[i] = { personOf(chats[i].peer), chats[i].time, chats[i].unread };
  ui.setPeople(people, net.peerCount());
  ui.setChats(list, nChats);
}

void setup() {
  ctl.begin();                                  // also starts I2C for the OLED
  ctl.setAmp(true);
  oled.begin();
  spk.begin();
  mic.begin();
  battery.begin();
  radio.begin(ui.channel);
  crypto.begin();
  crypto.setPassphrase(PASSPHRASE);
  net.setCipher(&crypto);
  net.onEvent(onNet);
  net.begin(radio, MY_NAME);
  voice.begin(mic, spk, net);
  ui.onAction(onAction);
  ui.begin(ctl, oled, spk);                     // logo + start tone
  gps.begin();                                  // up to ~4 s while it finds the baud rate
}

void loop() {
  ui.update();
  net.update();
  voice.update();
  gps.update();
  static uint32_t last = 0;
  if (millis() - last >= 1000) { last = millis(); refresh(); }
}
