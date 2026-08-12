/*
  UI -- glue between Keypad and Screen.

  This is the layer to edit when you want to change what the device shows or
  how navigation behaves. Screen knows how to draw; Keypad knows what was
  pressed; neither knows about the other. UI owns the menu structure, the
  current selection, and the redraw policy.

  Redraw policy matters more than it looks. A full 128x64 flush over I2C costs
  several milliseconds, and the audio path has a 40 ms frame deadline. So:

    - render() only touches the panel when something actually changed (the
      dirty flag), not on every call.
    - setAudioActive(true) freezes navigation and switches to a TX/RX banner.
      Keypresses during audio are discarded rather than queued, so releasing
      PTT does not replay a burst of menu moves.

  Adding a screen: extend UiPage, add its label to menuItems in UI.cpp, and
  handle it in render(). Nothing outside this file needs to change.
*/

#ifndef RANGER_UI_H
#define RANGER_UI_H

#include <Arduino.h>
#include <Keypad.h>
#include <Screen.h>

enum UiPage {
  PAGE_MENU = 0,
  PAGE_PEOPLE,
  PAGE_LOCATION,
  PAGE_MESSAGES,
  PAGE_BROADCAST,
  PAGE_SETTINGS,
  PAGE_TRANSMIT,     // driven by PTT, not reachable from the menu
  PAGE_RECEIVE       // driven by an incoming packet
};

class UI {
public:
  // Both must already be begun. UI does not own them; it only drives them.
  void begin(Screen *screen, Keypad *keypad);

  // Poll the keypad and advance the menu. Cheap when nothing was pressed.
  // Does nothing while audio is active -- PTT wins.
  void update();

  // Draw if anything changed since the last call. Safe to call every loop
  // iteration; it is a no-op unless the dirty flag is set.
  void render();

  // Audio arbitration. While active the menu is frozen, queued keypresses are
  // discarded, and the screen shows a TX or RX page instead of the menu.
  void setAudioActive(bool active, bool transmitting);

  // Link quality shown on the receive page. Setting these marks the screen
  // dirty only when the page is actually visible.
  void setLinkStats(int rssi, float snr);

  // Status bar fields, drawn on every page.
  void setBattery(float level);
  void setGpsSignal(int bars);
  void setClock(const char *hhmm);

  UiPage currentPage() const { return page; }

  // Force a redraw on the next render(), e.g. after showing a boot message.
  void invalidate() { dirty = true; }

private:
  void handleKey(KeyEvent key);

  Screen *screen = nullptr;
  Keypad *keypad = nullptr;

  UiPage page = PAGE_MENU;
  int selectedIndex = 0;
  bool dirty = true;

  bool audioActive = false;
  bool transmitting = false;

  int rssi = 0;
  float snr = 0.0f;
  float battery = 1.0f;
  int gpsBars = 0;
  char clockText[8] = "";
};

#endif  // RANGER_UI_H
