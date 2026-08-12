#include "UI.h"
#include <string.h>

// Layout copied from the tested test/OLEDscreen/U8g2/menu.cpp: five 64x15
// boxes, two columns. Edit here to change the menu.
static const MenuItem menuItems[] = {
    {1,  11, 64, 15,  8, 22, "People"},
    {64, 11, 64, 15, 71, 22, "Location"},
    {1,  25, 64, 15,  8, 36, "Messages"},
    {64, 25, 64, 15, 71, 36, "Broadcast"},
    {1,  39, 64, 15,  8, 50, "Settings"}
};
static const int MENU_COUNT = sizeof(menuItems) / sizeof(menuItems[0]);

// Menu index -> page. Parallel to menuItems above.
static const UiPage menuTargets[] = {
    PAGE_PEOPLE, PAGE_LOCATION, PAGE_MESSAGES, PAGE_BROADCAST, PAGE_SETTINGS
};

void UI::begin(Screen *s, Keypad *k) {
  screen = s;
  keypad = k;
  page = PAGE_MENU;
  selectedIndex = 0;
  dirty = true;
}

void UI::update() {
  if (keypad == nullptr) return;

  // PTT wins. Drain and discard so releasing the button does not replay a
  // burst of moves that happened while the user was talking.
  if (audioActive) {
    keypad->flush();
    return;
  }

  KeyEvent key;
  while ((key = keypad->read()) != KEY_NONE) {
    handleKey(key);
  }
}

void UI::handleKey(KeyEvent key) {
  if (page == PAGE_MENU) {
    // Two-column grid: left/right move across, up/down move by row. The last
    // row has a single item, so moves are clamped rather than wrapped -- with
    // an odd item count wrapping lands somewhere unpredictable.
    int next = selectedIndex;

    switch (key) {
      case KEY_UP:    next = selectedIndex - 2; break;
      case KEY_DOWN:  next = selectedIndex + 2; break;
      case KEY_LEFT:  next = selectedIndex - 1; break;
      case KEY_RIGHT: next = selectedIndex + 1; break;
      case KEY_ENTER:
        page = menuTargets[selectedIndex];
        dirty = true;
        return;
      default: return;
    }

    if (next >= 0 && next < MENU_COUNT && next != selectedIndex) {
      selectedIndex = next;
      dirty = true;
    }
    return;
  }

  // Any sub-page: ENTER or LEFT returns to the menu. Nothing else is wired up
  // yet -- the individual screens are still to be built.
  if (key == KEY_ENTER || key == KEY_LEFT) {
    page = PAGE_MENU;
    dirty = true;
  }
}

void UI::render() {
  if (screen == nullptr || !dirty) return;

  screen->beginFrame();
  screen->drawStatus(gpsBars, battery, clockText);

  switch (page) {
    case PAGE_MENU:
      screen->drawMenu(menuItems, MENU_COUNT, selectedIndex);
      screen->drawFoot("", "Select", 49, "", 95);
      break;

    case PAGE_TRANSMIT:
      screen->drawBanner("TX", "transmitting");
      break;

    case PAGE_RECEIVE:
      screen->drawLinkStats("RX", rssi, snr);
      break;

    // Sub-pages are placeholders until their screens are built. They share one
    // arm rather than five identical ones; give a page its own case when it
    // grows real content.
    default: {
      const char *label = "";
      for (int i = 0; i < MENU_COUNT; i++) {
        if (menuTargets[i] == page) { label = menuItems[i].label; break; }
      }
      screen->drawBanner(label);
      screen->drawFoot("Back", "", 49, "", 95);
      break;
    }
  }

  screen->endFrame();
  dirty = false;
}

void UI::setAudioActive(bool active, bool tx) {
  if (active == audioActive && tx == transmitting) return;

  audioActive = active;
  transmitting = tx;

  if (active) {
    page = tx ? PAGE_TRANSMIT : PAGE_RECEIVE;
    if (keypad != nullptr) keypad->setEnabled(false);
  } else {
    page = PAGE_MENU;
    if (keypad != nullptr) {
      keypad->flush();          // drop anything pressed during audio
      keypad->setEnabled(true);
    }
  }
  dirty = true;
}

void UI::setLinkStats(int r, float s) {
  rssi = r;
  snr = s;
  // Only a visible change is worth a redraw; the receive page is the only one
  // showing these numbers.
  if (page == PAGE_RECEIVE) dirty = true;
}

void UI::setBattery(float level) {
  // Quantise to the five drawn steps so small ADC jitter does not trigger a
  // redraw on every sample.
  int oldStep = (int)(battery * 20);
  int newStep = (int)(level * 20);
  battery = level;
  if (oldStep != newStep) dirty = true;
}

void UI::setGpsSignal(int bars) {
  if (bars == gpsBars) return;
  gpsBars = bars;
  dirty = true;
}

void UI::setClock(const char *hhmm) {
  if (hhmm == nullptr) return;
  if (strncmp(clockText, hhmm, sizeof(clockText)) == 0) return;
  strncpy(clockText, hhmm, sizeof(clockText) - 1);
  clockText[sizeof(clockText) - 1] = '\0';
  dirty = true;
}
