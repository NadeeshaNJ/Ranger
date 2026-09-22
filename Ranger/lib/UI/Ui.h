// Ui: menus, typing and push-to-talk. Info comes in with set...(),
// what the user does goes out through onAction().
//   Keys (silkscreen): 2 up, 8 down, 4 back (menu: left), 6 action (menu: right), 5 select
//   Knob: turn = move, press = select.   Typing: T9 keys, knob left = delete, knob press = send
//   VOICE: push-to-talk (to the person on screen, otherwise everyone)
#pragma once
#include "Controls.h"
#include "Display.h"
#include "Speaker.h"

#define UI_ALL -1                         // "everyone" instead of a person number

enum UiAction : uint8_t { A_TALK_START, A_TALK_STOP, A_SEND_TEXT, A_OPEN_CHAT, A_PING,
                          A_SHARE_POS, A_SOS, A_TRACK_ON, A_TRACK_OFF, A_SETTINGS };

struct UiPerson { const char* name; bool hasGps; float distanceM, bearing; int bars; };
struct UiChat   { int person; const char* time; bool unread; };    // person number or UI_ALL

class Ui {
public:
  void begin(Controls& c, Display& d, Speaker& s);
  void update();                                           // call in every loop()
  void onAction(void (*fn)(UiAction a, int person, const char* text)) { _cb = fn; }

  void setPeople(const UiPerson* p, int n) { _people = p; _nPeople = n; }
  void setChats(const UiChat* c, int n)    { _chats = c; _nChats = n; }
  void setStatus(int gpsBars, int battery, const char* time);
  void setPosition(bool fix, double lat, double lon, float heading);
  void addMessage(bool mine, const char* text);            // add to the open chat
  void toast(const char* text);

  int volume = 70, brightness = 3, channel = 9;            // Settings screen values

private:
  enum Screen : uint8_t { LOGO, MENU, PEOPLE, PERSON, TRACK, TALK, LOCATION, CHATS, CHAT, TYPING, BROADCAST, SETTINGS };
  void go(Screen s);
  void back();
  void onKey(uint8_t key);
  void onTurn(int clicks);
  void onSelect();
  void onTalk(bool down);
  void openChat(int person, bool typing);
  void typeKey(int digit);
  void changeSetting(int d);
  void draw();
  const char* nameOf(int person);
  void act(UiAction a, int person, const char* text = nullptr) { if (_cb) _cb(a, person, text); }

  Controls* _c; Display* _d; Speaker* _s;
  void (*_cb)(UiAction, int, const char*) = nullptr;

  const UiPerson* _people = nullptr; int _nPeople = 0;
  const UiChat*   _chats = nullptr;  int _nChats = 0;
  int _gpsBars = 0, _battery = -1;
  char _time[6] = "--:--";
  bool _fix = false; double _lat = 0, _lon = 0; float _heading = 0;

  Screen _screen = LOGO, _stack[8];
  int  _depth = 0, _menuSel = 0;
  ListView _lv[12];                                        // selection, one per screen
  int  _person = 0, _chatWith = UI_ALL, _talkTo = UI_ALL;
  bool _talking = false, _talkOpened = false, _editing = false, _dirty = true;
  uint32_t _talkStart = 0, _lastDraw = 0, _toastUntil = 0;
  char _toast[24] = "";
  char _typed[128] = "";
  int  _tapKey = -1, _tapCount = 0;
  uint32_t _tapTime = 0;
};
