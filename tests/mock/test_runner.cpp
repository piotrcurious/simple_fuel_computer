
#include "Arduino.h"
#include <map>
#include "Adafruit_SSD1306.h"

// External declarations for setup and loop
extern void setup();
extern void loop();

// Global mocks
uint32_t _millis = 0;
uint32_t _micros = 0;
std::map<int, int> _pin_states;
std::map<int, voidFuncPtr> _interrupts;
voidFuncPtr _timer1_callback = nullptr;
SerialMock Serial;
SPI_Mock SPI;
TwoWire Wire;

// Function to simulate a pulse on the injector pin
void simulatePulse(int pin, uint32_t duration_us) {
    _pin_states[pin] = HIGH;
    if (_interrupts.count(pin)) _interrupts[pin]();

    _micros += duration_us;
    _millis = _micros / 1000;

    _pin_states[pin] = LOW;
    if (_interrupts.count(pin)) _interrupts[pin]();
}

int main() {
    setup();

    for (int i = 0; i < 100; i++) {
        // Simulate some pulses (e.g., at 3000 RPM)
        for (int p = 0; p < 10; p++) {
            simulatePulse(2, 1000); // Pulse on pin 2
            simulatePulse(3, 1000); // Pulse on pin 3
            simulatePulse(4, 1000); // Pulse on pin 4
            simulatePulse(5, 1000); // Pulse on pin 5
            _micros += 19000;
            _millis = _micros / 1000;
        }

        if (_timer1_callback) _timer1_callback();

        loop();

        _millis += 100;
        _micros += 100000;
    }

    return 0;
}
