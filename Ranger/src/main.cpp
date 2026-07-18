#include <Arduino.h>
// #include <OLEDscreen.h>
/*********
  Rui Santos
  Complete project details at https://randomnerdtutorials.com  
*********/

#include <Wire.h>
#include <U8g2lib.h>
#include <math.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

void setup() {
    u8g2.begin();
}


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
void drawFoot(const char* left = "", const char* middle = "", int posXm = 49, const char* right = "", int posXr = 95) {
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);
    u8g2.setFont(u8g2_font_5x7_tr);
    // left side
    u8g2.drawStr(3, 62, left);
    // middle
    u8g2.drawStr(posXm, 62, middle);
    // right side
    u8g2.drawStr(posXr, 62, right);
   
}
void drawLogo(void) {
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);
    // logo
    u8g2.setFont(u8g2_font_profont29_tr);
    u8g2.drawStr(17, 42, "RANGER");
}

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
//--------------------------------------------------------------------
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
    u8g2.setFont(u8g2_font_profont11_tf);

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
    u8g2.drawStr(51, 62, "People");
}
//-------------------------------------------------------------------
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
//---------------------------------------------------------------
void drawLocation(void) {
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);
    // Back
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr(3, 62, "Back");
    // Location
    u8g2.drawStr(45, 62, "Location");
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
/*    u8g2.firstPage();
    do {
        drawLogo();
    } while (u8g2.nextPage());
    
    delay(2000);
    
    u8g2.firstPage();
    do {
        drawStatus();
        drawMenu();
    } while (u8g2.nextPage());
    
    delay(2000);
*/
    
    //if (selectedOption > 4) selectedOption = 0;
    if (selectedMessage >= NUM_MESSAGES) selectedMessage = 0;
    
    u8g2.firstPage();
    do {
        drawStatus();
        drawFoot("Back", "Messages", 46);
        drawMessage();
    } while (u8g2.nextPage());
    
    delay(2000);
    
    currentAngle += 5; // degrees per frame
    if (currentAngle >= 360) currentAngle = 0;
    selectedMessage++;
    delay(50); // controls rotation speed
};

