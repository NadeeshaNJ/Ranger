#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// [BEGIN lopaka generated]
void drawFoot(const char* left = "", const char* middle = "", int posXmiddle = 49, const char* right = "", int posXright = 95) {
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);
    u8g2.setFont(u8g2_font_5x7_tr);
    // left side
    u8g2.drawStr(3, 62, left);
    // middle
    u8g2.drawStr(posXmiddle, 62, middle);
    // right side
    u8g2.drawStr(posXright, 62, right);
   
}
// [END lopaka generated]

void loop(){
    u8g2.firstPage();
    do {
        drawFoot();
    } while (u8g2.nextPage());
};