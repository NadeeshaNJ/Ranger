#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

#include <math.h>

struct Point {
    float x, y;
};
float currentAngle = 0;
// Rotate a point around origin by angle (in degrees), then shift to center
Point rotatePoint(float x, float y, float angleDeg, float centerX, float centerY) {
    float rad = angleDeg * PI / 180.0;
    float cosA = cos(rad);
    float sinA = sin(rad);
    Point result;
    result.x = centerX + (x * cosA - y * sinA);
    result.y = centerY + (x * sinA + y * cosA);
    return result;
}

void drawDirectionArrow(int centerX, int centerY, int size, float angleDeg) {
    // Local shape, pointing right (angle = 0), before rotation
    float tipX = size,        tipY = 0;
    float backTopX = -size,   backTopY = -size * 0.9;
    float notchX = -size*0.6, notchY = 0;
    float backBotX = -size,   backBotY = size * 0.9;

    Point tip     = rotatePoint(tipX, tipY, angleDeg, centerX, centerY);
    Point backTop = rotatePoint(backTopX, backTopY, angleDeg, centerX, centerY);
    Point notch   = rotatePoint(notchX, notchY, angleDeg, centerX, centerY);
    Point backBot = rotatePoint(backBotX, backBotY, angleDeg, centerX, centerY);

    u8g2.setDrawColor(1);
    // Two triangles sharing the tip and notch, forming the concave dart shape
    u8g2.drawTriangle(tip.x, tip.y, backTop.x, backTop.y, notch.x, notch.y);
    u8g2.drawTriangle(tip.x, tip.y, notch.x, notch.y, backBot.x, backBot.y);
}
static const unsigned char image_signal_bar1_bits[] U8X8_PROGMEM = {0x03,0x03};
static const unsigned char image_signal_bar2_bits[] U8X8_PROGMEM = {0x03,0x03,0x03,0x03};
static const unsigned char image_signal_bar3_bits[] U8X8_PROGMEM = {0x03,0x03,0x03,0x03,0x03,0x03};
static const unsigned char image_signal_bar4_bits[] U8X8_PROGMEM = {0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03};
void drawTrack(const char* personName, float bearingAngle, const char* distance, float signalStrength, float angle) {
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);

    // people1
    u8g2.setFont(u8g2_font_profont15_tr);
    u8g2.drawStr(6, 21, personName);

    // DirectionArrow (replaces the static triangle)
    // Original triangle: tip(99,17), base(84,45)-(114,45)
    // centerX=99, centerY=31 (midpoint of tip and base), size=14
    // +90 offset because 0 deg = right, but arrow should point up at 0 deg
    drawDirectionArrow(99, 31, 14, bearingAngle - 90);

    // distance
    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.drawStr(11, 33, distance);
    // direction
    while (angle < 0) angle += 360;
    while (angle >= 360) angle -= 360;

    const char* directions[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    int index = (int)((angle + 22.5) / 45.0) % 8;

    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(8, 43, directions[index]);
    // angle
    u8g2.setFont(u8g2_font_6x12_tf);
    u8g2.drawUTF8(29, 43, (String(angle, 0) + "°").c_str());
    // signal_bar1
    // strength: 0 to 4 (0 = no signal, 4 = full signal)

    if (signalStrength >= 1) u8g2.drawXBMP(33, 52, 2, 2, image_signal_bar1_bits);
    if (signalStrength >= 2) u8g2.drawXBMP(29, 50, 2, 4, image_signal_bar2_bits);
    if (signalStrength >= 3) u8g2.drawXBMP(25, 48, 2, 6, image_signal_bar3_bits);
    if (signalStrength >= 4) u8g2.drawXBMP(21, 46, 2, 8, image_signal_bar4_bits);
}

void loop(){
    u8g2.firstPage();
    do {
        // drawStatus();
        // drawFoot("Back", "People", 49);
        drawTrack("Police", currentAngle , "1.2km", 3, currentAngle);
    } while (u8g2.nextPage());

    currentAngle += 5; // degrees per frame
    if (currentAngle >= 360) currentAngle = 0;

    delay(50); // controls rotation speed
};

