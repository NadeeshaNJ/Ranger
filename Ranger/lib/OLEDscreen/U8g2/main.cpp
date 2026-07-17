#include <Arduino.h>
// #include <OLEDscreen.h>
/*********
  Rui Santos
  Complete project details at https://randomnerdtutorials.com  
*********/

#include <Wire.h>
#include <U8g2lib.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// [BEGIN lopaka generated]
static const unsigned char image_paint_1_bits[] U8X8_PROGMEM = {0x00,0x01,0x83,0x02,0x03,0x0d,0x1b,0x0e,0x1b,0x16,0xdb,0x28,0xdb,0x10};
static const unsigned char image_paint_31_bits[] U8X8_PROGMEM = {0x01};



void drawMain(void) {
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);
    // paint 1
    u8g2.drawXBMP(0, 0, 14, 7, image_paint_1_bits);
    // rect 2
    u8g2.drawFrame(115, 1, 11, 5);
    // line 4
    u8g2.drawLine(0, 8, 127, 8);
    // string 6
    u8g2.setFont(u8g2_font_haxrcorp4089_tr);
    u8g2.drawStr(53, 7, "14:35");
    // paint 31
    u8g2.drawXBMP(126, 3, 1, 1, image_paint_31_bits);
}


void setup() {
    u8g2.begin();
}
void loop(){
    u8g2.firstPage();
    do {
        drawMain();
    } while (u8g2.nextPage());
};


// [END lopaka generated]
