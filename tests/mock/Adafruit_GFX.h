
#ifndef ADAFRUIT_GFX_H
#define ADAFRUIT_GFX_H

#include "Arduino.h"
#include <vector>
#include <fstream>

class Adafruit_GFX {
protected:
    int16_t _width, _height;
    std::vector<uint8_t> _buffer;

public:
    Adafruit_GFX(int16_t w, int16_t h) : _width(w), _height(h), _buffer(w * h, 0) {}

    void setTextSize(uint8_t s) {}
    void setTextColor(uint16_t c) {}
    void setCursor(int16_t x, int16_t y) {}
    void print(const char* s) { std::cout << s; }
    void print(float f, int p = 2) { std::cout << f; }
    void print(int i) { std::cout << i; }
    void println(const char* s) { std::cout << s << std::endl; }
    void println(float f, int p = 2) { std::cout << f << std::endl; }
    void println(int i) { std::cout << i << std::endl; }

    virtual void drawPixel(int16_t x, int16_t y, uint16_t color) {
        if (x < 0 || x >= _width || y < 0 || y >= _height) return;
        _buffer[y * _width + x] = (color > 0) ? 1 : 0;
    }

    void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
        // Simple Bresenham or just enough for our graphs
        int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = dx + dy, e2;
        for (;;) {
            drawPixel(x0, y0, color);
            if (x0 == x1 && y0 == y1) break;
            e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }

    void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
        for (int16_t i = x; i < x + w; i++) { drawPixel(i, y, color); drawPixel(i, y + h - 1, color); }
        for (int16_t i = y; i < y + h; i++) { drawPixel(x, i, color); drawPixel(x + w - 1, i, color); }
    }

    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
        for (int16_t i = x; i < x + w; i++)
            for (int16_t j = y; j < y + h; j++)
                drawPixel(i, j, color);
    }

    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
        for (int16_t i = y; i < y + h; i++) drawPixel(x, i, color);
    }

    void getTextBounds(const char* s, int16_t x, int16_t y, int16_t* x1, int16_t* y1, uint16_t* w, uint16_t* h) {
        *x1 = x; *y1 = y; *w = 10; *h = 10;
    }

    void ssd1306_command(uint8_t c) {}

    void savePBM(const char* filename) {
        std::ofstream f(filename);
        f << "P1\n" << _width << " " << _height << "\n";
        for (int i = 0; i < _width * _height; i++) {
            f << (int)_buffer[i] << (i % _width == _width - 1 ? "\n" : " ");
        }
        f.close();
    }
};

#endif
