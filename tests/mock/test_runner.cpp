
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
std::map<int, InterruptInfo> _interrupts;
voidFuncPtr _timer1_callback = nullptr;
SerialMock Serial;
SPI_Mock SPI;
TwoWire Wire;

// Function to simulate a pulse on the injector pin
void simulatePulse(int pin, uint32_t duration_us) {
    digitalWrite(pin, HIGH);
    _micros += duration_us;
    _millis = _micros / 1000;
    digitalWrite(pin, LOW);
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
    uint32_t last_loop_micros = _micros;

    // Static state to carry over between profiles
    static uint32_t last_inj_us = 0;
    static uint32_t last_cam_us = 0;

    while (_millis - profile_start_ms < profile.duration_ms) {
        float progress = (float)(_millis - profile_start_ms) / profile.duration_ms;
        uint32_t current_rpm = profile.start_rpm + (uint32_t)(progress * (profile.end_rpm - profile.start_rpm));
        float current_pulse_ms = profile.start_pulse_ms + progress * (profile.end_pulse_ms - profile.start_pulse_ms);

        if (current_rpm == 0) {
            _micros += 10000;
            _millis = _micros / 1000;
        } else {
            // High fidelity pulse train
            uint32_t cam_interval_us = 1000000 / (current_rpm * 36 / 60);
            uint32_t inj_interval_us = 1000000 / (current_rpm * 2 / 60);

            // Advance time to the next event (cam, inj, or loop)
            uint32_t next_cam = last_cam_us + cam_interval_us;
            uint32_t next_inj = last_inj_us + inj_interval_us;
            uint32_t next_loop = last_loop_micros + 10000; // 10ms loop

            uint32_t next_event = std::min({next_cam, next_inj, next_loop});

            if (next_event > _micros) {
                _micros = next_event;
                _millis = _micros / 1000;
            }

            if (_micros >= next_cam) {
                simulatePulse(2, 100);
                last_cam_us = _micros;
            }
            if (_micros >= next_inj) {
                simulatePulse(3, (uint32_t)(current_pulse_ms * 1000));
                last_inj_us = _micros;
            }
        }

        if (_micros - last_loop_micros >= 10000) {
            if (_timer1_callback) _timer1_callback();
            loop();
            last_loop_micros = _micros;
        }
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

    // Long simulations to fill 128-pixel and 320-pixel graphs
    std::vector<SimulationProfile> profiles = {
        {"Idling", 20000, 800, 800, 1.0, 1.0},
        {"Accelerating", 40000, 800, 5000, 1.0, 5.0},
        {"Cruising", 40000, 5000, 5000, 2.5, 2.5},
        {"Decelerating", 30000, 5000, 800, 0.5, 0.5},
        {"Engine Stop", 10000, 0, 0, 0, 0}
    };

    for (const auto& p : profiles) {
        runProfile(p);
    }

    return 0;
}
