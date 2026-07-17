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

static const unsigned char image_gps_bits[] U8X8_PROGMEM = {0x1c,0x3e,0x63,0x63,0x36,0x3e,0x1c,0x08};
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
            u8g2.drawXBMP(94, iconY, 7, 8, image_gps_bits);
        }
    }

    u8g2.setDrawColor(1);
    u8g2.drawStr(3, 62, "Back");
    u8g2.drawStr(51, 62, "People");
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
    
    if (selectedIndex > 4) selectedIndex = 0;
    u8g2.firstPage();
    do {
        drawStatus();
        drawPeople();
    } while (u8g2.nextPage());
    
    delay(2000);
    selectedIndex++;
  
};

