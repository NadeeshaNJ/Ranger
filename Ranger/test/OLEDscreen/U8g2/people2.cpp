#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

const char* personOptions[] = {"Track", "Ping", "Message", "Share", "Talk"};
const int NUM_OPTIONS = 5;
const int VISIBLE_OPTION_ROWS = 5;
const int OPTION_ROW_HEIGHT = 11;
const int OPTION_ROW_TOP = 8;

int selectedOption = 0;
int optionScrollOffset = 0;

void moveOptionUp() {
    if (selectedOption > 0) selectedOption--;
}

void moveOptionDown() {
    if (selectedOption < NUM_OPTIONS - 1) selectedOption++;
}

void drawPeople2(const char* personName) {
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);

    // Person's name, fixed at top, not part of the scrolling list
    u8g2.setFont(u8g2_font_profont15_tr);
    u8g2.drawStr(6, 21, personName);

    // Keep selection inside visible window
    if (selectedOption < optionScrollOffset) {
        optionScrollOffset = selectedOption;
    }
    if (selectedOption >= optionScrollOffset + VISIBLE_OPTION_ROWS) {
        optionScrollOffset = selectedOption - VISIBLE_OPTION_ROWS + 1;
    }

    u8g2.setFont(u8g2_font_profont10_tr);

    for (int row = 0; row < VISIBLE_OPTION_ROWS; row++) {
        int idx = optionScrollOffset + row;
        if (idx >= NUM_OPTIONS) break;

        int yTop = OPTION_ROW_TOP + row * OPTION_ROW_HEIGHT;
        int textY = yTop + 8;
        bool selected = (idx == selectedOption);

        if (selected) {
            u8g2.setDrawColor(1);
            u8g2.drawBox(81, yTop, 47, 12);
            u8g2.setDrawColor(0);
        } else {
            u8g2.setDrawColor(1);
        }

        u8g2.drawStr(85, textY, personOptions[idx]);
    }

    u8g2.setDrawColor(1);
}

void loop(){
    u8g2.firstPage();
    do {
        drawPeople2("Police");
    } while (u8g2.nextPage());
};