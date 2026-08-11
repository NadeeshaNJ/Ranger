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

// Typing area state
char typingBuffer[128] = "";
int typingLen = 0;
const int VISIBLE_CHAT_LINES = 3;
const int CHAT_LINE_HEIGHT = 11;
const int CHAT_BOX_TOP = 19;
const int CHAT_BOX_HEIGHT = 36;
const int MAX_MSG_WIDTH = 95; // pixel width limit before wrapping to next line

struct TypingLine {
    char text[32];
};
TypingLine typingLines[10];
int numTypingLines = 0;
int typingScrollOffset = 0;

const int TYPING_MAX_WIDTH = 118;
const int VISIBLE_TYPING_LINES = 1;
const int VISIBLE_CHAT_LINES_SMALL = 2;

// Re-wrap the typing buffer every time a character is added or removed
void rewrapTypingBuffer() {
    numTypingLines = 0;
    char buffer[128];
    strncpy(buffer, typingBuffer, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';

    u8g2.setFont(u8g2_font_profont11_tr);
    char* word = strtok(buffer, " ");
    char currentLine[32] = "";

    while (word != NULL) {
        char testLine[32];
        if (strlen(currentLine) == 0) {
            strcpy(testLine, word);
        } else {
            snprintf(testLine, sizeof(testLine), "%s %s", currentLine, word);
        }

        if (u8g2.getStrWidth(testLine) <= TYPING_MAX_WIDTH || strlen(currentLine) == 0) {
            strncpy(currentLine, testLine, sizeof(currentLine));
        } else {
            strncpy(typingLines[numTypingLines++].text, currentLine, sizeof(typingLines[0].text));
            strncpy(currentLine, word, sizeof(currentLine));
        }
        word = strtok(NULL, " ");
    }
    if (strlen(currentLine) > 0 || numTypingLines == 0) {
        strncpy(typingLines[numTypingLines++].text, currentLine, sizeof(typingLines[0].text));
    }

    typingScrollOffset = max(0, numTypingLines - VISIBLE_TYPING_LINES); // auto-scroll to latest line
}

void appendChar(char c) {
    if (typingLen < (int)sizeof(typingBuffer) - 1) {
        typingBuffer[typingLen++] = c;
        typingBuffer[typingLen] = '\0';
        rewrapTypingBuffer();
    }
}

void backspaceChar() {
    if (typingLen > 0) {
        typingBuffer[--typingLen] = '\0';
        rewrapTypingBuffer();
    }
}

void drawScrollbar(int x, int trackTop, int trackHeight, int visibleLines, int totalLines, int scrollOffset) {
    if (totalLines <= visibleLines) return;
    int thumbHeight = max(2, trackHeight * visibleLines / totalLines);
    int maxScroll = totalLines - visibleLines;
    int thumbTop = trackTop + (trackHeight - thumbHeight) * scrollOffset / maxScroll;
    u8g2.drawVLine(x, thumbTop, thumbHeight);
}

void drawMessageType(const char* contactName) {
    u8g2.setFontMode(1);
    u8g2.setBitmapMode(1);

    // Contact name
    u8g2.setFont(u8g2_font_profont10_tr);
    u8g2.drawStr(3, 16, contactName);

    // Outer box
    u8g2.drawFrame(0, 19, 128, 36);

    // Chat history section (top, 2 lines visible)
    u8g2.setFont(u8g2_font_profont11_tr);
    for (int row = 0; row < VISIBLE_CHAT_LINES_SMALL; row++) {
        int idx = chatScrollOffset + row;
        if (idx >= numChatLines) break;
        int y = 29 + row * 11;
        ChatLine &line = chatLines[idx];
        if (line.isOwn) {
            int w = u8g2.getStrWidth(line.text);
            u8g2.drawStr(122 - w, y, line.text);
        } else {
            u8g2.drawStr(3, y, line.text);
        }
    }
    drawScrollbar(125, 21, 10, VISIBLE_CHAT_LINES_SMALL, numChatLines, chatScrollOffset);

    // Divider between chat and typing area
    u8g2.drawLine(1, 43, 126, 43);

    // Typing area (bottom, 1 line visible + scroll)
    if (typingLen == 0) {
        u8g2.drawStr(3, 52, "Typing!...");
    } else {
        for (int row = 0; row < VISIBLE_TYPING_LINES; row++) {
            int idx = typingScrollOffset + row;
            if (idx >= numTypingLines) break;
            u8g2.drawStr(3, 52 + row * 11, typingLines[idx].text);
        }
    }
    drawScrollbar(125, 45, 3, VISIBLE_TYPING_LINES, numTypingLines, typingScrollOffset);
}

void loop() {
    u8g2.firstPage();
    do {
        drawMessageType("People1");
    } while (u8g2.nextPage());

    // Example: simulate typing, replace with real keyboard input handling
    if (Serial.available()) {
        char c = Serial.read();
        if (c == '\b') backspaceChar();
        else appendChar(c);
    }
}