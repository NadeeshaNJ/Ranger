#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// [BEGIN lopaka generated]
static const unsigned char image_battery_bits[] U8X8_PROGMEM = {0xff,0x07,0x01,0x04,0x01,0x0c,0x01,0x04,0xff,0x07};
static const unsigned char image_gps_bar1_bits[] U8X8_PROGMEM = {0x03,0x03};
static const unsigned char image_gps_bar2_bits[] U8X8_PROGMEM = {0x03,0x03,0x03,0x03};
static const unsigned char image_gps_bar3_bits[] U8X8_PROGMEM = {0x03,0x03,0x03,0x03,0x03,0x03};
static const unsigned char image_gps_symbol_bits[] U8X8_PROGMEM = {0x02,0x05,0x1a,0x1c,0x2c,0x50,0x20};



void drawStatus(void) {
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);
    // gps_symbol
    u8g2.drawXBMP(7, 0, 7, 7, image_gps_symbol_bits);
    // status_line
    u8g2.drawLine(0, 8, 127, 8);
    // battery
    u8g2.drawXBMP(115, 1, 12, 5, image_battery_bits);
    // gps_bar1
    u8g2.drawXBMP(6, 5, 2, 2, image_gps_bar1_bits);
    // time
    u8g2.setFont(u8g2_font_haxrcorp4089_tr);
    u8g2.drawStr(53, 7, "14:35");
    // gps_bar2
    u8g2.drawXBMP(3, 3, 2, 4, image_gps_bar2_bits);
    // gps_bar3
    u8g2.drawXBMP(0, 1, 2, 6, image_gps_bar3_bits);
}

void loop(){
    u8g2.firstPage();
    do {
        drawStatus();
    } while (u8g2.nextPage());
};
