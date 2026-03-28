
#include "Arduino.h"
#include <map>
#include "Adafruit_SSD1306.h"
#include "BlueDisplay.h"
#include <vector>

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

struct SimulationProfile {
    std::string name;
    uint32_t duration_ms;
    uint32_t start_rpm;
    uint32_t end_rpm;
    float start_pulse_ms;
    float end_pulse_ms;
};

// Pointer to the active display for frame capturing
Adafruit_GFX* _active_display = nullptr;

void runProfile(const SimulationProfile& profile) {
    std::cout << "--- Starting Profile: " << profile.name << " ---" << std::endl;
    uint32_t profile_start_ms = _millis;

    while (_millis - profile_start_ms < profile.duration_ms) {
        float progress = (float)(_millis - profile_start_ms) / profile.duration_ms;
        uint32_t current_rpm = profile.start_rpm + (uint32_t)(progress * (profile.end_rpm - profile.start_rpm));
        float current_pulse_ms = profile.start_pulse_ms + progress * (profile.end_pulse_ms - profile.start_pulse_ms);

        // Simulating 4 cylinders (2 pulses per rev)
        uint32_t revs_per_sec = current_rpm / 60;
        if (revs_per_sec == 0) {
            _micros += 100000;
            _millis = _micros / 1000;
        } else {
            uint32_t micros_per_rev = 1000000 / revs_per_sec;
            uint32_t micros_per_pulse = micros_per_rev / 2;

            uint32_t pulse_us = (uint32_t)(current_pulse_ms * 1000);
            if (pulse_us > micros_per_pulse) pulse_us = micros_per_pulse;

            simulatePulse(2, 500); // Cam (approx)
            simulatePulse(3, pulse_us); // Inj
            simulatePulse(4, pulse_us);
            simulatePulse(5, pulse_us);

            _micros += (micros_per_pulse - pulse_us);
            _millis = _micros / 1000;
        }

        if (_timer1_callback) _timer1_callback();
        loop();
    }

    // Capture frame at end of profile
    if (_active_display) {
        std::string filename = "output_" + profile.name + ".pbm";
        for(auto &c : filename) if(c == ' ') c = '_';
        _active_display->savePBM(filename.c_str());
    }
}

int main() {
#if defined(HAS_SSD1306)
    extern Adafruit_SSD1306 display;
    _active_display = &display;
#elif defined(HAS_BLUEDISPLAY)
    extern BlueDisplay myDisplay;
    _active_display = &myDisplay;
#endif

    setup();

    std::vector<SimulationProfile> profiles = {
        {"Idling", 2000, 800, 800, 1.0, 1.0},
        {"Accelerating", 5000, 800, 5000, 1.0, 5.0},
        {"Cruising", 3000, 5000, 5000, 2.5, 2.5},
        {"Decelerating", 3000, 5000, 800, 0.5, 0.5},
        {"Engine Stop", 2000, 0, 0, 0, 0}
    };

    for (const auto& p : profiles) {
        runProfile(p);
    }

    return 0;
}
