// Fuel consumption monitor for ESP8266 + SSD1306 OLED
// Measures injector pulse width via interrupt, computes ml/min,
// displays a rolling graph and numeric readout.

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>   // Included only to shut the radio off in setup()

// ── Pin definitions ─────────────────────────────────────────────────────────
// BUG 3 FIX: GPIO3 is the Serial RX pin — use D5 (GPIO14) instead.
// Original: #define INJECTOR_PIN 3  // comment claimed GPIO5 but 3 = RX
#define INJECTOR_PIN  D5   // GPIO14

#define KNOB_PIN      A0   // Analog pin for timebase knob

// ── Display ──────────────────────────────────────────────────────────────────
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64

#define OLED_DC     D2
#define OLED_CS     D8
#define OLED_RESET  D3
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
                          &SPI, OLED_DC, OLED_RESET, OLED_CS);

// ── Text overlay feature flags ───────────────────────────────────────────────
#define DRAW_TEXT
#define DRAW_TEXT_TIMEBASE
//#define DRAW_TEXT_SHADOW
#define DRAW_TEXT_RECTANGLE

#define TEXT_LPKM_X     0
#define TEXT_LPKM_Y     (SCREEN_HEIGHT - 8)
#define TEXT_TIMEBASE_X 60
#define TEXT_TIMEBASE_Y (SCREEN_HEIGHT - 8)

// ── ISR state ────────────────────────────────────────────────────────────────
volatile unsigned long pulseStart      = 0;
volatile uint32_t      totalPulseWidth = 0;

// ── Measurement state ────────────────────────────────────────────────────────
uint32_t pulseWidthSnapshot    = 0;
float    fuelConsumptionMLsec  = 0.0f;
float    injectorFlowRateMLmin = 200.0f;  // ml/min at 100 % duty cycle
float    fuel_avg1_MLmin       = 0.0f;   // exponential moving average

// ── Timing ───────────────────────────────────────────────────────────────────
uint32_t previousMillis       = 0;
uint32_t previousMillis_graph = 0;
uint32_t interval             = 100;    // fuel-calc update period [ms]
uint32_t graph_interval       = 1000;   // graph update period [ms], set by knob

// ── Knob / timebase ──────────────────────────────────────────────────────────
uint16_t knobValue = 0;
// BUG 1 FIX: was uint8_t — map() returns up to 1000, which overflows uint8_t.
uint16_t timeBase  = 100;   // graph update interval in ms (10 – 1000)

// ── Rolling graph ────────────────────────────────────────────────────────────
uint8_t  graphX = 0;
uint8_t  graphY = 0;
uint8_t  graphW = SCREEN_WIDTH;
uint8_t  graphH = SCREEN_HEIGHT - 1;

uint32_t graphMin = 0;
uint32_t graphMax = 100;

uint32_t graphData[SCREEN_WIDTH];   // circular buffer (left-shift scroll)

// ── ISR ───────────────────────────────────────────────────────────────────────
void ICACHE_RAM_ATTR injectorISR() {
  if (digitalRead(INJECTOR_PIN) == HIGH) {
    pulseStart = micros();
  } else {
    // Guard: only accumulate if a rising edge was seen first
    if (pulseStart != 0) {
      uint32_t width = (uint32_t)(micros() - pulseStart);
      totalPulseWidth += width;
      pulseStart = 0;   // arm for next rising edge
    }
  }
}

// ── setup() ──────────────────────────────────────────────────────────────────
void setup() {
  // BUG 6 FIX: disable WiFi radio (was included but never shut off)
  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();

  // BUG 8 FIX: 9600 is needlessly slow on an 80 MHz ESP8266
  Serial.begin(115200);

  if (!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.display();
  delay(2000);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Setting up..."));
  display.display();

  memset(graphData, 0, sizeof(graphData));   // zero-fill graph buffer

  pinMode(INJECTOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(INJECTOR_PIN), injectorISR, CHANGE);

  display.setCursor(0, 16);
  display.println(F("Done!"));
  display.display();
}

// ── updateDisplay() ───────────────────────────────────────────────────────────
#ifdef DRAW_TEXT
void updateDisplay() {
  char buffer[16];
  // Format: "123.45"  (integer part + 2 decimal places)
  sprintf(buffer, "%d.%02d",
          (uint16_t)fuel_avg1_MLmin,
          (uint16_t)(fuel_avg1_MLmin * 100.0f) % 100);

#ifdef DRAW_TEXT_SHADOW
  // BUG 5 FIX: shadow must use the same formatted 'buffer', not the raw float
  display.setTextColor(SSD1306_BLACK);

  const int8_t offsets[4][2] = {{1,-1},{1,1},{-1,1},{-1,-1}};
  for (uint8_t o = 0; o < 4; o++) {
    display.setCursor(TEXT_LPKM_X    + offsets[o][0],
                      TEXT_LPKM_Y   + offsets[o][1]);
    display.print(buffer);

    display.setCursor(TEXT_TIMEBASE_X + offsets[o][0],
                      TEXT_TIMEBASE_Y + offsets[o][1]);
    display.print(F("T:"));
    display.print(timeBase);
    display.print(F("ms"));
  }
#endif // DRAW_TEXT_SHADOW

#ifdef DRAW_TEXT_RECTANGLE
  int16_t  x1, y1;
  uint16_t w, h;
  display.getTextBounds(buffer, TEXT_LPKM_X, TEXT_LPKM_Y, &x1, &y1, &w, &h);
  // BUG 4 FIX: use SSD1306_BLACK, not bare BLACK
  display.fillRect(x1, y1 - 1, w, h + 2, SSD1306_BLACK);
#endif

  display.setTextColor(SSD1306_WHITE);
  display.setCursor(TEXT_LPKM_X, TEXT_LPKM_Y);
  display.print(buffer);

#ifdef DRAW_TEXT_TIMEBASE
#ifdef DRAW_TEXT_RECTANGLE
  display.fillRect(TEXT_TIMEBASE_X, TEXT_TIMEBASE_Y - 1, 50, 9, SSD1306_BLACK);
#endif
  display.setCursor(TEXT_TIMEBASE_X, TEXT_TIMEBASE_Y);
  display.print(F("T:"));
  display.print(timeBase);
  display.print(F("ms"));
#endif // DRAW_TEXT_TIMEBASE
}
#endif // DRAW_TEXT

// ── updateGraph() ─────────────────────────────────────────────────────────────
void updateGraph() {
  // Shift left, append new sample
  for (uint8_t i = 0; i < graphW - 1; i++) {
    graphData[i] = graphData[i + 1];
  }
  graphData[graphW - 1] = (uint32_t)fuel_avg1_MLmin;

  // BUG 2 FIX: graphMax was initialised to the magic constant 1 instead of
  // graphData[0], breaking autoscale whenever all samples are below 1.
  graphMax = graphData[0];
  graphMin = graphData[0];

  // BUG 7 NOTE: OPTIMIZED_MAX_SEARCH was defined but never implemented.
  // The clean fix is to implement it properly rather than silently skip it.
#ifdef OPTIMIZED_MAX_SEARCH
  // Exclude the most-recent column (index graphW-1) from the search so the
  // scale settles on stable history rather than chasing the live edge.
  uint8_t searchEnd = graphW - 1;
#else
  uint8_t searchEnd = graphW;
#endif

  for (uint8_t i = 1; i < searchEnd; i++) {
    if (graphData[i] > graphMax) graphMax = graphData[i];
    if (graphData[i] < graphMin) graphMin = graphData[i];
  }
  // Avoid zero range
  if (graphMax <= graphMin) graphMax = graphMin + 1;
}

// ── drawGraph() ───────────────────────────────────────────────────────────────
void drawGraph() {
  for (uint8_t i = 0; i < graphW; i++) {
    uint16_t barHeight = (uint16_t)(
      ((float)(graphData[i] - graphMin) / (float)(graphMax - graphMin)) * graphH
    );
    if (barHeight > graphH) barHeight = graphH;
    display.drawFastVLine(graphX + i, graphY + (graphH - barHeight),
                          barHeight, SSD1306_WHITE);
  }
}

// ── loop() ────────────────────────────────────────────────────────────────────
void loop() {
  uint32_t currentMillis = millis();

  static uint32_t last_interval_micros = 0;

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    uint32_t current_micros   = micros();
    uint32_t interval_duration = current_micros - last_interval_micros;
    last_interval_micros      = current_micros;

    if (interval_duration == 0) return;   // extremely unlikely; guard division

    // Atomic snapshot of ISR accumulator
    noInterrupts();
    pulseWidthSnapshot = totalPulseWidth;
    totalPulseWidth    = 0;
    interrupts();

    // duty_cycle = pulseWidthSnapshot / interval_duration
    float duty           = (float)pulseWidthSnapshot / (float)interval_duration;
    fuelConsumptionMLsec = duty * (injectorFlowRateMLmin / 60.0f);
    float current_ml_min = duty * injectorFlowRateMLmin;

    // Exponential moving average (α = 0.1)
    fuel_avg1_MLmin = (fuel_avg1_MLmin * 9.0f + current_ml_min) / 10.0f;

    // BUG 1 FIX: timeBase must be uint16_t (holds 10–1000).
    // BUG 1 FIX: assign map() result directly to graph_interval, not via
    //            an already-overflowed uint8_t intermediate.
    knobValue      = analogRead(KNOB_PIN);
    timeBase       = (uint16_t)map(knobValue, 0, 1023, 10, 1000);
    graph_interval = (uint32_t)timeBase;

    if (currentMillis - previousMillis_graph >= graph_interval) {
      previousMillis_graph = currentMillis;

      // Reset EMA to zero when injector has been idle long enough
      if (current_ml_min == 0.0f && fuel_avg1_MLmin < 0.01f) {
        fuel_avg1_MLmin = 0.0f;
      }

      display.clearDisplay();
      updateGraph();
      drawGraph();

      Serial.print(F("Fuel: "));
      Serial.print(fuelConsumptionMLsec, 3);
      Serial.print(F(" ml/s  Avg: "));
      Serial.print(fuel_avg1_MLmin, 2);
      Serial.println(F(" ml/min"));

#ifdef DRAW_TEXT
      updateDisplay();
#endif
      display.display();
    }
  }
}
