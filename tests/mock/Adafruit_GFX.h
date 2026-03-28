
#ifndef ADAFRUIT_GFX_H
#define ADAFRUIT_GFX_H

#include "Arduino.h"

class Adafruit_GFX {
public:
    void setTextSize(uint8_t s) {}
    void setTextColor(uint16_t c) {}
    void setCursor(int16_t x, int16_t y) {}
    void print(const char* s) { std::cout << s; }
    void print(float f) { std::cout << f; }
    void print(int i) { std::cout << i; }
    void println(const char* s) { std::cout << s << std::endl; }
    void println(float f) { std::cout << f << std::endl; }
    void println(int i) { std::cout << i << std::endl; }
    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {}
    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {}
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {}
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {}
    void drawPixel(int16_t x, int16_t y, uint16_t color) {}
    void getTextBounds(const char* s, int16_t x, int16_t y, int16_t* x1, int16_t* y1, uint16_t* w, uint16_t* h) {
        *x1 = x; *y1 = y; *w = 10; *h = 10;
    }
    void ssd1306_command(uint8_t c) {}
};

#endif
