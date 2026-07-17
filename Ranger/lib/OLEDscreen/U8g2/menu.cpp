#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

struct MenuItem {
    int boxX, boxY, boxW, boxH; // box position/size
    int textX, textY;           // text position
    const char* label;
};

MenuItem menuItems[] = {
    {1, 11, 64, 15, 8, 22, "People"},
    {64, 11, 64, 15, 71, 22, "Location"},
    {1, 25, 64, 15, 8, 36, "Messages"},
    {64, 25, 64, 15, 71, 36, "Broadcast"},
    {1, 39, 64, 15, 8, 50, "Settings"}
};
const int NUM_ITEMS = 5;

int selectedIndex = 0; // change this on button press

void drawMenu(void) {
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);
    u8g2.setFont(u8g2_font_profont11_tr);

    for (int i = 0; i < NUM_ITEMS; i++) {
        MenuItem item = menuItems[i];

        if (i == selectedIndex) {
            // Selected: filled box + black text
            u8g2.setDrawColor(1);
            u8g2.drawBox(item.boxX, item.boxY, item.boxW, item.boxH);
            u8g2.setDrawColor(0);
            u8g2.drawStr(item.textX, item.textY, item.label);
        } else {
            // Not selected: just outline + white text
            u8g2.setDrawColor(1);
            u8g2.drawStr(item.textX, item.textY, item.label);
        }
    }

    u8g2.setDrawColor(1); // reset
}

void loop(){
    u8g2.firstPage();
    do {
        drawMenu();
    } while (u8g2.nextPage());
};