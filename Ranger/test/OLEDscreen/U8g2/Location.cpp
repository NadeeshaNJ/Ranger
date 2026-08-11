#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// [BEGIN lopaka generated]
void drawLocation(void) {
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);
    // longitude
    u8g2.setFont(u8g2_font_profont12_tf);
    u8g2.drawUTF8(10, 27, "61°11'46 N");
    // latitude
    u8g2.drawUTF8(4, 43, "149°58'34 W");
    // angle
    u8g2.setFont(u8g2_font_profont17_tf);
    u8g2.drawUTF8(87, 30, "225°");
    // direction
    u8g2.drawStr(91, 43, "SE");
    // seperate_bar
    u8g2.drawLine(76, 13, 76, 50);
}
// [END lopaka generated]

void loop(){
    u8g2.firstPage();
    do {
        drawLocation();
    } while (u8g2.nextPage());
};