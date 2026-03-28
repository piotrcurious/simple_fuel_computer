
#ifndef ADAFRUIT_SSD1306_H
#define ADAFRUIT_SSD1306_H

#include "Arduino.h"
#include "Adafruit_GFX.h"

class SPI_Mock {};
extern SPI_Mock SPI;

class TwoWire {};
extern TwoWire Wire;

class Adafruit_SSD1306 : public Adafruit_GFX {
public:
    Adafruit_SSD1306(uint8_t w, uint8_t h, SPI_Mock* s, int8_t dc, int8_t r, int8_t cs) {}
    Adafruit_SSD1306(uint8_t w, uint8_t h, TwoWire* wire, int8_t reset) {}
    Adafruit_SSD1306(int8_t reset) {}
    bool begin(uint8_t vcc, uint8_t addr = 0x3C) { return true; }
    void clearDisplay() {}
    void display() {}
};

#define SSD1306_SETCONTRAST 0x81

#endif
