
#ifndef ARDUINO_H
#define ARDUINO_H

#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <map>
#include <chrono>
#include <stdint.h>
#include <stdio.h>
#include <cstring>

#define HIGH 0x1
#define LOW  0x0

#define INPUT 0x0
#define OUTPUT 0x1
#define INPUT_PULLUP 0x2

#define RISING 0x01
#define FALLING 0x02
#define CHANGE 0x03

#define WHITE 0xF
#define BLACK 0x0
#define SSD1306_WHITE 0xF
#define SSD1306_BLACK 0x0
#define SSD1306_SWITCHCAPVCC 0x2

typedef bool boolean;
typedef uint8_t byte;

extern uint32_t _millis;
extern uint32_t _micros;

inline uint32_t millis() { return _millis; }
inline uint32_t micros() { return _micros; }

inline void delay(uint32_t ms) { _millis += ms; _micros += (uint32_t)ms * 1000; }
inline void delayMicroseconds(uint32_t us) { _micros += us; _millis = _micros / 1000; }

inline void pinMode(int pin, int mode) {}
extern std::map<int, int> _pin_states;

typedef void (*voidFuncPtr)(void);
struct InterruptInfo {
    voidFuncPtr callback;
    int mode;
};
extern std::map<int, InterruptInfo> _interrupts;

inline void attachInterrupt(int pin, voidFuncPtr callback, int mode) {
    _interrupts[pin] = {callback, mode};
}
inline int digitalPinToInterrupt(int pin) { return pin; }

inline int digitalRead(int pin) { return _pin_states[pin]; }
inline void digitalWrite(int pin, int val) {
    int old_val = _pin_states[pin];
    _pin_states[pin] = val;
    if (_interrupts.count(pin)) {
        auto& info = _interrupts[pin];
        if (info.mode == CHANGE && val != old_val) info.callback();
        else if (info.mode == RISING && val == HIGH && old_val == LOW) info.callback();
        else if (info.mode == FALLING && val == LOW && old_val == HIGH) info.callback();
    }
}

inline int analogRead(int pin) { return 512; }

#define ICACHE_RAM_ATTR

class SerialMock {
public:
    void begin(int baud) {}
    void print(const char* s) { std::cout << s; }
    void print(float f, int p = 2) { printf("%.*f", p, f); }
    void print(int i) { std::cout << i; }
    void print(uint32_t i) { std::cout << i; }
    void println(const char* s) { std::cout << s << std::endl; }
    void println(float f, int p = 2) { printf("%.*f\n", p, f); }
    void println(int i) { std::cout << i << std::endl; }
    void println(uint32_t i) { std::cout << i << std::endl; }
    void println() { std::cout << std::endl; }
};

extern SerialMock Serial;

inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
  if (in_max == in_min) return out_min;
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

inline void noInterrupts() {}
inline void interrupts() {}

#define D1 5
#define D2 4
#define D3 0
#define D5 14
#define D7 13
#define D8 15
#define A0 0

#define TIM_DIV16 1
#define TIM_EDGE 1
#define TIM_LOOP 1
extern voidFuncPtr _timer1_callback;
inline void timer1_attachInterrupt(voidFuncPtr callback) { _timer1_callback = callback; }
inline void timer1_enable(int a, int b, int c) {}
inline void timer1_write(int val) {}

#define F(s) s

namespace ESP {
    inline void restart() { exit(0); }
}

#endif
