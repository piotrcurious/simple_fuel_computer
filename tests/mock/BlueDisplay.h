
#ifndef BLUE_DISPLAY_H
#define BLUE_DISPLAY_H

#include "Arduino.h"
#include "Adafruit_GFX.h"
#include <fstream>

#define RGB(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))
#ifndef BLACK
#define BLACK 0x0
#endif

class BlueDisplay : public Adafruit_GFX {
public:
    BlueDisplay() : Adafruit_GFX(320, 240) {}
    void connectToDisplay() {}
    void setOrientation(int o) {}
    void clearDisplay(uint16_t color) { std::fill(_buffer.begin(), _buffer.end(), (color > 0) ? 1 : 0); }

    // BlueDisplay uses different signatures for some primitives
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
        Adafruit_GFX::drawRect(x, y, w - x, h - y, color); // BlueDisplay rect is (x0, y0, x1, y1)
    }
};

#endif
