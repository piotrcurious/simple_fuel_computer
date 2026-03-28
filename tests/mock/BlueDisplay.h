
#ifndef BLUE_DISPLAY_H
#define BLUE_DISPLAY_H

#include "Arduino.h"
#include "Adafruit_SSD1306.h"

#define RGB(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))
#ifndef BLACK
#define BLACK 0x0
#endif

class BlueDisplay {
public:
    void connectToDisplay() {}
    void setOrientation(int o) {}
    void clearDisplay(uint16_t color) {}
    void drawRect(int x0, int y0, int x1, int y1, uint16_t color) {}
    void fillRect(int x0, int y0, int x1, int y1, uint16_t color) {}
    void drawLine(int x0, int y0, int x1, int y1, uint16_t color) {}
};

#endif
