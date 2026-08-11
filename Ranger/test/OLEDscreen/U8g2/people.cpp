#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

struct Person {
    const char* name;
    bool hasGps;
};

Person people[] = {
    {"People1", true},
    {"People2", false},
    {"People3", true},
    {"People4", false},
    {"People5", true},
    {"People6", false}
};
const int NUM_PEOPLE = 6;
const int VISIBLE_ROWS = 4;
const int ROW_HEIGHT = 11;
const int ROW_TOP = 9;

int selectedIndex = 0;
int scrollOffset = 0;

static const unsigned char image_gps_bits[] U8X8_PROGMEM = {0x1c,0x3e,0x63,0x63,0x77,0x3e,0x1c,0x1c,0x08};
static const unsigned char image_message_bits[] U8X8_PROGMEM = {0xff,0x01,0x01,0x01,0x55,0x01,0x01,0x01,0xff,0x01,0x18,0x00,0x04,0x00,0x02,0x00};
static const unsigned char image_mic_bits[] U8X8_PROGMEM = {0x1c,0x1c,0x5d,0x5d,0x41,0x7f,0x08,0x1c};

void moveSelectionUp() {
    if (selectedIndex > 0) selectedIndex--;
}

void moveSelectionDown() {
    if (selectedIndex < NUM_PEOPLE - 1) selectedIndex++;
}

void drawPeople(void) {
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);
    u8g2.setFont(u8g2_font_profont11_tr);

    // Keep selected row inside the visible window
    if (selectedIndex < scrollOffset) {
        scrollOffset = selectedIndex;
    }
    if (selectedIndex >= scrollOffset + VISIBLE_ROWS) {
        scrollOffset = selectedIndex - VISIBLE_ROWS + 1;
    }

    for (int row = 0; row < VISIBLE_ROWS; row++) {
        int idx = scrollOffset + row;
        if (idx >= NUM_PEOPLE) break;

        int yTop = ROW_TOP + row * ROW_HEIGHT;
        int textY = yTop + 9;
        int iconY = yTop + 2;
        bool selected = (idx == selectedIndex);

        if (selected) {
            u8g2.setDrawColor(1);
            u8g2.drawBox(1, yTop + 1, 126, ROW_HEIGHT);
            u8g2.setDrawColor(0);
        } else {
            u8g2.setDrawColor(1);
        }

        u8g2.drawStr(6, textY, people[idx].name);

        // Icons keep the same draw color as the text above
        u8g2.drawXBMP(105, iconY, 9, 8, image_message_bits);
        u8g2.drawXBMP(118, iconY, 7, 8, image_mic_bits);
        if (people[idx].hasGps) {
            u8g2.drawXBMP(94, iconY, 7, 9, image_gps_bits);
        }
    }

    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(3, 62, "Back");
    u8g2.drawStr(49, 62, "People");
}
void loop(){
    u8g2.firstPage();
    do {
        drawPeople();
    } while (u8g2.nextPage());
};