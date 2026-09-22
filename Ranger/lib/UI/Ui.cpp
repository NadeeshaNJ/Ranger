#include "Ui.h"

static const char* T9[10] = {" 0", ".,?!1", "abc2", "def3", "ghi4", "jkl5", "mno6", "pqrs7", "tuv8", "wxyz9"};
static const char* PERSON_MENU[] = {"Track", "Ping", "Message", "Share", "Talk"};
static const char* BROADCAST_MENU[] = {"Talk", "Message", "SOS"};
static const char* SETTING_NAMES[] = {"Volume", "Brightness", "Channel"};
static const uint8_t CONTRAST[] = {10, 60, 120, 190, 255};   // brightness 1..5

void Ui::begin(Controls& c, Display& d, Speaker& s) {
  _c = &c; _d = &d; _s = &s;
  draw();                                                  // logo
  _s->playTone(3);
}

void Ui::update() {
  InputEvent e;
  while (_c->poll(e)) {
    _dirty = true;
    if (_screen == LOGO) { _screen = MENU; continue; }     // any key skips the logo
    if (e.turn) onTurn(e.turn);
    else if (e.key == KEY_VOICE) onTalk(e.down);
    else if (e.down) onKey(e.key);
  }
  uint32_t now = millis();
  if (_screen == LOGO && now > 1500) { _screen = MENU; _dirty = true; }
  if (_toastUntil && now > _toastUntil) { _toastUntil = 0; _dirty = true; }
  if (_tapKey >= 0 && now - _tapTime > 800) _tapKey = -1;          // letter is final
  bool live = _screen == TRACK || _screen == TALK || _screen == LOCATION;
  if (_dirty || (live && now - _lastDraw > 250)) { draw(); _dirty = false; _lastDraw = now; }
}

// ---------------- data in ----------------

void Ui::setStatus(int gpsBars, int battery, const char* time) {
  if (gpsBars != _gpsBars || battery != _battery || strcmp(time, _time)) _dirty = true;
  _gpsBars = gpsBars; _battery = battery;
  strncpy(_time, time, 5);
}

void Ui::setPosition(bool fix, double lat, double lon, float heading) {
  _fix = fix; _lat = lat; _lon = lon; _heading = heading;
}

void Ui::addMessage(bool mine, const char* text) { _d->chatAdd(mine, text); _dirty = true; }

void Ui::toast(const char* text) {
  strncpy(_toast, text, 23);
  _toastUntil = millis() + 1300;
  _dirty = true;
}

const char* Ui::nameOf(int p) { return p == UI_ALL ? "Everyone" : (p < _nPeople ? _people[p].name : "?"); }

// ---------------- navigation ----------------

void Ui::go(Screen s) {
  if (_depth < 8) _stack[_depth++] = _screen;
  _screen = s;
  _lv[s] = ListView();                                     // new screen starts at the top
}

void Ui::back() {
  if (_screen == TRACK) act(A_TRACK_OFF, _person);
  if (_screen == SETTINGS && _editing) { _editing = false; act(A_SETTINGS, 0); }
  _screen = _depth ? _stack[--_depth] : MENU;
}

void Ui::onKey(uint8_t key) {
  if (_screen == TYPING) {                                 // every digit types letters
    if (key <= KEY_9) typeKey(key);
    if (key == KEY_KNOB && _typed[0]) {                    // knob press = send
      act(A_SEND_TEXT, _chatWith, _typed);
      _d->chatAdd(true, _typed);
      _typed[0] = 0;
      back();
    }
    return;
  }
  switch (key) {
    case KEY_5: case KEY_KNOB: onSelect(); break;
    case KEY_2: if (_screen == MENU) _menuSel = max(0, _menuSel - 2); else onTurn(-1); break;
    case KEY_8: if (_screen == MENU) _menuSel = min(4, _menuSel + 2); else onTurn(+1); break;
    case KEY_4: if (_screen == MENU) { if (_menuSel % 2) _menuSel--; }
                else if (!_talking) back();
                break;
    case KEY_6:
      if (_screen == MENU && _menuSel % 2 == 0 && _menuSel < 4) _menuSel++;
      if (_screen == LOCATION) { act(A_SHARE_POS, UI_ALL); toast("Location sent"); }
      if (_screen == TRACK)    { act(A_PING, _person);     toast("Ping sent"); }
      break;
  }
}

void Ui::onTurn(int n) {
  ListView& v = _lv[_screen];
  switch (_screen) {
    case MENU:      _menuSel = constrain(_menuSel + n, 0, 4); break;
    case CHAT:      _d->chatScroll(-n); break;
    case SETTINGS:  if (_editing) changeSetting(n); else v.sel = constrain(v.sel + n, 0, 2); break;
    case TYPING:
      for (int i = 0; i < -n; i++) {                       // knob left = delete
        int len = strlen(_typed);
        if (!len) { back(); return; }                      // nothing left: leave typing
        _typed[len - 1] = 0;
      }
      _tapKey = -1;
      break;
    default: {                                             // lists: stay inside the list
      int size = _screen == PEOPLE ? _nPeople : _screen == CHATS ? _nChats :
                 _screen == PERSON ? 5 : _screen == BROADCAST ? 3 : 0;
      v.sel = constrain(v.sel + n, 0, max(0, size - 1));
      break;
    }
  }
}

void Ui::onSelect() {
  int sel = _lv[_screen].sel;
  switch (_screen) {
    case MENU: {
      const Screen dest[] = {PEOPLE, LOCATION, CHATS, BROADCAST, SETTINGS};
      go(dest[_menuSel]);
      break;
    }
    case PEOPLE:
      if (_nPeople) { _person = sel; go(PERSON); }
      break;
    case PERSON:
      if (sel == 0) { go(TRACK); act(A_TRACK_ON, _person); }
      if (sel == 1) { act(A_PING, _person); toast("Ping sent"); }
      if (sel == 2) openChat(_person, true);
      if (sel == 3) { act(A_SHARE_POS, _person); toast("Location sent"); }
      if (sel == 4) { _talkTo = _person; go(TALK); }
      break;
    case CHATS:
      if (_nChats) openChat(_chats[sel].person, false);
      break;
    case CHAT:
      go(TYPING);
      break;
    case BROADCAST:
      if (sel == 0) { _talkTo = UI_ALL; go(TALK); }
      if (sel == 1) openChat(UI_ALL, true);
      if (sel == 2) { act(A_SOS, UI_ALL); _s->playTone(4); toast("SOS sent"); }
      break;
    case SETTINGS:
      _editing = !_editing;
      if (!_editing) act(A_SETTINGS, 0);
      break;
    default: break;
  }
}

void Ui::openChat(int person, bool typing) {
  _chatWith = person;
  _d->chatClear();
  act(A_OPEN_CHAT, person);                                // app adds the history with addMessage()
  go(CHAT);
  if (typing) go(TYPING);
}

void Ui::typeKey(int d) {
  // Multi-tap: the same key again within 0.8 s changes the last letter (2,2 = b)
  int len = strlen(_typed);
  if (d == _tapKey && len) {
    _tapCount = (_tapCount + 1) % strlen(T9[d]);
    _typed[len - 1] = T9[d][_tapCount];
  } else if (len < 126) {
    _tapCount = 0;
    _typed[len] = T9[d][0];
    _typed[len + 1] = 0;
  }
  _tapKey = d;
  _tapTime = millis();
}

void Ui::changeSetting(int d) {
  int row = _lv[SETTINGS].sel;
  if (row == 0) { volume = constrain(volume + d * 5, 0, 100); _s->setVolume(volume); }
  if (row == 1) { brightness = constrain(brightness + d, 1, 5); _d->setContrast(CONTRAST[brightness - 1]); }
  if (row == 2) channel = constrain(channel + d, 0, 15);
}

void Ui::onTalk(bool down) {
  if (down && !_talking) {
    // talk to whoever is on screen, otherwise to everyone
    int to = UI_ALL;
    if (_screen == PERSON || _screen == TRACK) to = _person;
    if (_screen == CHAT || _screen == TYPING)  to = _chatWith;
    if (_screen == TALK)                       to = _talkTo;
    _talkTo = to;
    _s->playTone(1);                                       // beep before the mic opens
    act(A_TALK_START, to);
    _talking = true;
    _talkStart = millis();
    _talkOpened = _screen != TALK;
    if (_talkOpened) go(TALK);
  }
  if (!down && _talking) {
    act(A_TALK_STOP, _talkTo);
    _s->playTone(2);
    _talking = false;
    if (_talkOpened) back();
  }
}

// ---------------- drawing ----------------

void Ui::draw() {
  _d->clear();
  if (_screen == LOGO) { _d->logo(); _d->show(); return; }
  _d->statusBar(_gpsBars, _battery, _time);
  ListView& v = _lv[_screen];

  switch (_screen) {
    case MENU:
      _d->menu(_menuSel);
      break;
    case PEOPLE: {
      const char* names[16]; bool gps[16];
      int n = min(_nPeople, 16);
      for (int i = 0; i < n; i++) { names[i] = _people[i].name; gps[i] = _people[i].hasGps; }
      _d->people(names, gps, n, v);
      _d->footBar("Back", "People");
      break;
    }
    case PERSON:
      _d->options(nameOf(_person), PERSON_MENU, 5, v);
      break;
    case TRACK: {
      const UiPerson& p = _people[_person];
      _d->track(p.name, p.bearing - _heading, p.bearing, p.distanceM, p.bars);
      _d->footBar("Back", "Track", "Ping");
      break;
    }
    case TALK:
      _d->talk(nameOf(_talkTo), _talking, (millis() - _talkStart) / 1000);
      if (!_talking) _d->footBar("Back", "Talk");
      break;
    case LOCATION:
      _d->location(_fix, _lat, _lon, _heading);
      _d->footBar("Back", "Location", "Share");
      break;
    case CHATS: {
      const char* names[16]; const char* times[16]; bool unread[16];
      int n = min(_nChats, 16);
      for (int i = 0; i < n; i++) { names[i] = nameOf(_chats[i].person); times[i] = _chats[i].time; unread[i] = _chats[i].unread; }
      _d->messages(names, times, unread, n, v);
      _d->footBar("Back", "Messages");
      break;
    }
    case CHAT:
      _d->chat(nameOf(_chatWith));
      _d->footBar("Back", "Reply");
      break;
    case TYPING:
      _d->chat(nameOf(_chatWith), _typed);
      _d->footBar("<Del", "Send");
      break;
    case BROADCAST:
      _d->options("Broadcast", BROADCAST_MENU, 3, v);
      break;
    case SETTINGS: {
      char vol[8], bri[8], ch[24];
      snprintf(vol, sizeof(vol), "%d", volume);
      snprintf(bri, sizeof(bri), "%d/5", brightness);
      snprintf(ch, sizeof(ch), "%d:%d", channel, 2405 + 5 * channel);
      const char* values[] = {vol, bri, ch};
      _d->settings(SETTING_NAMES, values, 3, v, _editing);
      _d->footBar("Back", _editing ? "Done" : "Edit");
      break;
    }
    default: break;
  }
  if (_toastUntil) _d->toast(_toast);
  _d->show();
}
