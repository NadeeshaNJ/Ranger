#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

void drawLogo(void) {
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);
    // logo
    u8g2.setFont(u8g2_font_profont29_tr);
    u8g2.drawStr(17, 42, "RANGER");
}

void loop(){
    u8g2.firstPage();
    do {
        drawLogo();
    } while (u8g2.nextPage());
};