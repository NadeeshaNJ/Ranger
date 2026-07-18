#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

struct MessageEntry {
    const char* name;
    const char* time;
    bool hasNewMessage;
};

MessageEntry messages[] = {
    {"People1", "12:01", true},
    {"People2", "12:02", true},
    {"People3", "12:03", true},
    {"People4", "12:04", false},
    {"People5", "12:05", false}
};
const int NUM_MESSAGES = 5;
const int VISIBLE_MSG_ROWS = 4;
const int MSG_ROW_HEIGHT = 11;
const int MSG_ROW_TOP = 8;

int selectedMessage = 0;
int msgScrollOffset = 0;

void moveMessageUp() {
    if (selectedMessage > 0) selectedMessage--;
}

void moveMessageDown() {
    if (selectedMessage < NUM_MESSAGES - 1) selectedMessage++;
}

void drawMessage(void) {
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);

    if (selectedMessage < msgScrollOffset) {
        msgScrollOffset = selectedMessage;
    }
    if (selectedMessage >= msgScrollOffset + VISIBLE_MSG_ROWS) {
        msgScrollOffset = selectedMessage - VISIBLE_MSG_ROWS + 1;
    }

    u8g2.setFont(u8g2_font_profont11_tr);

    for (int row = 0; row < VISIBLE_MSG_ROWS; row++) {
        int idx = msgScrollOffset + row;
        if (idx >= NUM_MESSAGES) break;

        int yTop = MSG_ROW_TOP + row * MSG_ROW_HEIGHT;
        int textY = yTop + 9;
        int dotY = yTop + 6;
        bool selected = (idx == selectedMessage);

        if (selected) {
            u8g2.setDrawColor(1);
            u8g2.drawBox(0, yTop, 127, 12);
            u8g2.setDrawColor(0);
        } else {
            u8g2.setDrawColor(1);
        }

        u8g2.drawStr(3, textY, messages[idx].name);
        u8g2.drawStr(68, textY, messages[idx].time);

        if (messages[idx].hasNewMessage) {
            u8g2.drawFilledEllipse(116, dotY, 2, 2);
        }
    }

    u8g2.setDrawColor(1);
}

void loop(){
    u8g2.firstPage();
    do {
        //drawStatus();
        //drawFoot("Back", "Messages", 46);
        drawMessage();        
    } while (u8g2.nextPage());
};