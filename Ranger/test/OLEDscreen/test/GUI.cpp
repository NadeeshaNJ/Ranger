#include <Arduino.h>
// #include <OLEDscreen.h>
/*********
  Rui Santos
  Complete project details at https://randomnerdtutorials.com  
*********/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void drawStatusBar(int signalBars, int batteryPercent, const char* timeStr) {
  // Signal bars (top-left), 4 bars of increasing height
  int barX = 2;
  for (int i = 0; i < 4; i++) {
    int barHeight = 2 + (i * 2);
    if (i < signalBars) {
      display.fillRect(barX, 8 - barHeight, 2, barHeight, WHITE);
    } else {
      display.drawRect(barX, 8 - barHeight, 2, barHeight, WHITE);
    }
    barX += 3;
  }

  // Clock, centered
  display.setTextSize(1);
  display.setTextColor(WHITE);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(timeStr, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 0);
  display.print(timeStr);

  // Battery icon (top-right)
  int battX = SCREEN_WIDTH - 20;
  display.drawRect(battX, 1, 16, 7, WHITE);       // battery body
  display.fillRect(battX + 16, 3, 2, 3, WHITE);   // battery nub
  int fillWidth = map(batteryPercent, 0, 100, 0, 14);
  display.fillRect(battX + 1, 2, fillWidth, 5, WHITE);

  // Divider line under status bar
  display.drawLine(0, 10, SCREEN_WIDTH - 1, 10, WHITE);
}
void drawMenuScreen(const char* items[], int itemCount, int selectedIndex) {
  int startY = 14;
  int rowHeight = 12;

  for (int i = 0; i < itemCount; i++) {
    int y = startY + (i * rowHeight);
    if (y > SCREEN_HEIGHT - rowHeight) break; // stop if off-screen

    if (i == selectedIndex) {
      display.fillRect(0, y - 1, SCREEN_WIDTH, rowHeight, WHITE);
      display.setTextColor(BLACK);
    } else {
      display.setTextColor(WHITE);
    }

    display.setCursor(4, y + 1);
    display.print(items[i]);
  }
}
void drawSoftKeys(const char* leftLabel, const char* rightLabel) {
  display.setTextColor(WHITE);
  display.setTextSize(1);

  display.setCursor(2, SCREEN_HEIGHT - 8);
  display.print(leftLabel);

  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(rightLabel, 0, 0, &x1, &y1, &w, &h);
  display.setCursor(SCREEN_WIDTH - w - 2, SCREEN_HEIGHT - 8);
  display.print(rightLabel);
}
void drawHomeScreen() {
  display.setTextSize(1);
  display.setTextColor(WHITE);

  const char* userLabel = "Ranger Node";
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(userLabel, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 24);
  display.print(userLabel);

  const char* subLabel = "No new messages";
  display.getTextBounds(subLabel, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 36);
  display.print(subLabel);
}
void setup() {
  Serial.begin(115200);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  delay(2000);
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 10);
  // Display static text
  
}
enum ScreenState { SCREEN_HOME, SCREEN_MENU, SCREEN_MESSAGE };
ScreenState currentScreen = SCREEN_HOME;

const char* menuItems[] = {"Contacts", "Messages", "Settings", "GPS Locate"};
int selectedIndex = 0;

unsigned long lastSwitch = 0;
const unsigned long switchInterval = 2000; // change screen every 2s, for demo

void loop() {
  unsigned long now = millis();

  // Demo only: auto-cycle screens and selection since there are no buttons yet
  if (now - lastSwitch > switchInterval) {
    lastSwitch = now;
    selectedIndex = (selectedIndex + 1) % 4;
    if (selectedIndex == 0) {
      currentScreen = (ScreenState)((currentScreen + 1) % 3);
    }
  }

  display.clearDisplay();
  drawStatusBar(3, 72, "14:35");

  switch (currentScreen) {
    case SCREEN_HOME:
      drawHomeScreen();
      drawSoftKeys("Menu", "");
      break;
    case SCREEN_MENU:
      drawMenuScreen(menuItems, 4, selectedIndex);
      drawSoftKeys("Select", "Back");
      break;
    case SCREEN_MESSAGE:
      display.setCursor(4, 20);
      display.print("From: Node 02");
      display.setCursor(4, 32);
      display.print("Status check OK");
      drawSoftKeys("Reply", "Back");
      break;
  }

  display.display();
  delay(50);
}