
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

    // Simulate 10 seconds of varying RPM/Fuel
    for (int i = 0; i < 100; i++) {
        // Vary pulse duration and frequency based on i
        uint32_t pulse_len = 500 + i * 10; // 0.5ms to 1.5ms
        uint32_t gap_len = 20000 - i * 100; // 20ms to 10ms (3000 to 6000 RPM)

        // Simulate pulses for this 100ms slice
        uint32_t slice_micros = 0;
        while (slice_micros < 100000) {
            simulatePulse(2, 500); // Cam pulse (simplified)
            simulatePulse(3, pulse_len); // Injector
            simulatePulse(4, pulse_len);
            simulatePulse(5, pulse_len);

            _micros += gap_len;
            _millis = _micros / 1000;
            slice_micros += (pulse_len + gap_len);
        }

        if (_timer1_callback) _timer1_callback();

        loop();

        _millis += 10; // catch up
        _micros += 10000;
    }

    // Simulate engine stop
    std::cout << "--- Simulating engine stop ---" << std::endl;
    for (int i = 0; i < 30; i++) {
        _millis += 100;
        _micros += 100000;
        if (_timer1_callback) _timer1_callback();
        loop();
    }

    return 0;
}
