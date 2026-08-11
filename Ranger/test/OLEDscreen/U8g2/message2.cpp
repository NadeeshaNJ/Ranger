#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

#include <string.h>

struct ChatMessage {
    bool isOwn; // false = from contact, true = from me
    const char* text;
};

struct ChatLine {
    bool isOwn;
    char text[32];
};

ChatLine chatLines[100];
int numChatLines = 0;
int chatScrollOffset = 0;

const int VISIBLE_CHAT_LINES = 3;
const int CHAT_LINE_HEIGHT = 11;
const int CHAT_BOX_TOP = 19;
const int CHAT_BOX_HEIGHT = 36;
const int MAX_MSG_WIDTH = 95; // pixel width limit before wrapping to next line

// Greedy word-wrap: breaks one message into as many lines as needed
void wrapMessage(bool isOwn, const char* text) {
    char buffer[128];
    strncpy(buffer, text, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';

    char* word = strtok(buffer, " ");
    char currentLine[32] = "";

    while (word != NULL) {
        char testLine[32];
        if (strlen(currentLine) == 0) {
            strcpy(testLine, word);
        } else {
            snprintf(testLine, sizeof(testLine), "%s %s", currentLine, word);
        }

        int width = u8g2.getStrWidth(testLine);

        if (width <= MAX_MSG_WIDTH || strlen(currentLine) == 0) {
            strncpy(currentLine, testLine, sizeof(currentLine));
        } else {
            chatLines[numChatLines].isOwn = isOwn;
            strncpy(chatLines[numChatLines].text, currentLine, sizeof(chatLines[numChatLines].text));
            numChatLines++;
            strncpy(currentLine, word, sizeof(currentLine));
        }
        word = strtok(NULL, " ");
    }

    if (strlen(currentLine) > 0) {
        chatLines[numChatLines].isOwn = isOwn;
        strncpy(chatLines[numChatLines].text, currentLine, sizeof(chatLines[numChatLines].text));
        numChatLines++;
    }
}

// Call this once when a chat opens, converts raw messages into wrapped lines
void buildChatHistory(ChatMessage* history, int count) {
    numChatLines = 0;
    u8g2.setFont(u8g2_font_profont11_tr); // must match the font used when drawing
    for (int i = 0; i < count; i++) {
        wrapMessage(history[i].isOwn, history[i].text);
    }
    chatScrollOffset = max(0, numChatLines - VISIBLE_CHAT_LINES); // auto scroll to latest
}

void drawChatScrollbar() {
    if (numChatLines <= VISIBLE_CHAT_LINES) return; // everything fits, no scrollbar needed

    int trackTop = CHAT_BOX_TOP + 2;
    int trackBottom = CHAT_BOX_TOP + CHAT_BOX_HEIGHT - 2;
    int trackHeight = trackBottom - trackTop;

    int thumbHeight = max(3, trackHeight * VISIBLE_CHAT_LINES / numChatLines);
    int maxScroll = numChatLines - VISIBLE_CHAT_LINES;
    int thumbTop = trackTop + (trackHeight - thumbHeight) * chatScrollOffset / maxScroll;

    u8g2.drawVLine(125, thumbTop, thumbHeight);
}

void scrollChatUp() {
    if (chatScrollOffset > 0) chatScrollOffset--;
}

void scrollChatDown() {
    if (chatScrollOffset < numChatLines - VISIBLE_CHAT_LINES) chatScrollOffset++;
}

void drawMessage2(const char* contactName) {
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);

    u8g2.setFont(u8g2_font_profont10_tr);
    u8g2.drawStr(3, 16, contactName);

    u8g2.drawFrame(0, 19, 128, 36);

    u8g2.setFont(u8g2_font_profont11_tr);
    for (int row = 0; row < VISIBLE_CHAT_LINES; row++) {
        int idx = chatScrollOffset + row;
        if (idx >= numChatLines) break;

        int y = 29 + row * CHAT_LINE_HEIGHT;
        ChatLine &line = chatLines[idx];

        if (line.isOwn) {
            int w = u8g2.getStrWidth(line.text);
            u8g2.drawStr(122 - w, y, line.text); // right-aligned near the box edge
        } else {
            u8g2.drawStr(3, y, line.text); // left-aligned
        }
    }

    drawChatScrollbar();
}

ChatMessage history[] = {
    {false, "Hi! Welcome from people1."},
    {true, "Welcome Accepted"}
};
void setup() {
    u8g2.begin();
    buildChatHistory(history, 2);
}
void loop(){
    u8g2.firstPage();
    do {
        //drawStatus();
        //drawFoot("Back", "Messages", 46);
        drawMessage2("People1");        
    } while (u8g2.nextPage());
};