// Fuel consumption monitor — ESP8266 + SSD1306 (hardware SPI)
// Measures injector on-time via interrupt, computes ml/min EMA,
// displays a rolling graph and numeric overlay on a 128×64 OLED.

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>   // included only to disable the radio in setup()

// ── Pin definitions ──────────────────────────────────────────────────────────
#define INJECTOR_PIN  D5   // GPIO14  (GPIO3 = Serial RX — never use that)
#define KNOB_PIN      A0   // ADC input for timebase knob

// ── Display ───────────────────────────────────────────────────────────────────
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64

#define OLED_DC     D2
#define OLED_CS     D8
#define OLED_RESET  D3
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
                          &SPI, OLED_DC, OLED_RESET, OLED_CS);

// ── Feature flags ─────────────────────────────────────────────────────────────
#define DRAW_TEXT
#define DRAW_TEXT_TIMEBASE
//#define DRAW_TEXT_SHADOW       // 4-direction outline shadow (costs ~4× text draws)
#define DRAW_TEXT_RECTANGLE      // erase graph pixels behind text before drawing

// ── Text positions ────────────────────────────────────────────────────────────
#define TEXT_LPKM_X     0
#define TEXT_LPKM_Y     (SCREEN_HEIGHT - 8)
#define TEXT_TIMEBASE_X 60
#define TEXT_TIMEBASE_Y (SCREEN_HEIGHT - 8)

// ── ISR shared state ──────────────────────────────────────────────────────────
// FIX 4: was 'volatile unsigned long' — must match micros() type explicitly.
volatile uint32_t pulseStart      = 0;   // rising-edge timestamp [µs]; 0 = unarmed
volatile uint32_t totalPulseWidth = 0;   // accumulated on-time this interval [µs]

// ── Measurement ───────────────────────────────────────────────────────────────
float injectorFlowRateMLmin = 200.0f;    // rated flow at 100 % duty [ml/min]
float fuel_avg1_MLmin       = 0.0f;     // EMA of instantaneous ml/min (α = 0.1)

// ── Timing ────────────────────────────────────────────────────────────────────
uint32_t previousMillis       = 0;
uint32_t previousMillis_graph = 0;
static uint32_t last_interval_micros = 0;

static const uint32_t MEAS_INTERVAL_MS = 100;  // fuel-calc cadence [ms]

// ── Knob / graph timebase ─────────────────────────────────────────────────────
// FIX 10: removed redundant 'timeBase' + 'graph_interval' pair — one variable.
uint16_t graph_interval_ms = 1000;       // graph update period [ms], set by knob

static const uint16_t KNOB_HYSTERESIS = 5;  // ADC counts; prevents jitter (FIX 6)
static uint16_t knobValuePrev = 0xFFFF;      // sentinel → force first update

// ── Rolling graph ─────────────────────────────────────────────────────────────
static const uint8_t GRAPH_X = 0;
static const uint8_t GRAPH_Y = 0;
static const uint8_t GRAPH_W = SCREEN_WIDTH;
static const uint8_t GRAPH_H = SCREEN_HEIGHT - 1;

// FIX 8: float array preserves EMA fractional precision in the graph buffer.
// (uint32_t previously truncated e.g. 12.7 → 12, losing sub-unit resolution.)
float graphData[GRAPH_W];
float graphMin = 0.0f;
float graphMax = 1.0f;

// ─────────────────────────────────────────────────────────────────────────────
// ISR
// ─────────────────────────────────────────────────────────────────────────────
void ICACHE_RAM_ATTR injectorISR() {
  if (digitalRead(INJECTOR_PIN) == HIGH) {
    // Rising edge: arm the timer.
    pulseStart = micros();
  } else {
    // Falling edge: accumulate only when a matching rising edge was seen.
    if (pulseStart != 0) {
      totalPulseWidth += micros() - pulseStart;
      pulseStart = 0;   // disarm until next rising edge
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// setup()
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  // Disable WiFi — radio boots active, wastes ~70 mA and adds timing noise.
  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();

  Serial.begin(115200);

  if (!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);   // halt — no point continuing without a display
  }
  display.display();
  delay(2000);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Setting up..."));
  display.display();

  memset(graphData, 0, sizeof(graphData));

  pinMode(INJECTOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(INJECTOR_PIN), injectorISR, CHANGE);

  display.setCursor(0, 16);
  display.println(F("Done!"));
  display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// updateDisplay()  — text overlay (compiled only when DRAW_TEXT is defined)
// ─────────────────────────────────────────────────────────────────────────────
#ifdef DRAW_TEXT
void updateDisplay(const char* buffer) {

#ifdef DRAW_TEXT_SHADOW
  // FIX (prev. round): shadow now uses the same pre-formatted 'buffer'.
  // Draw 4-direction outline for legibility over the graph.
  display.setTextColor(SSD1306_BLACK);
  const int8_t offsets[4][2] = {{1,-1},{1,1},{-1,1},{-1,-1}};
  for (uint8_t o = 0; o < 4; o++) {
    display.setCursor(TEXT_LPKM_X    + offsets[o][0],
                      TEXT_LPKM_Y   + offsets[o][1]);
    display.print(buffer);

#ifdef DRAW_TEXT_TIMEBASE
    display.setCursor(TEXT_TIMEBASE_X + offsets[o][0],
                      TEXT_TIMEBASE_Y + offsets[o][1]);
    display.print(graph_interval_ms);
    display.print(F("ms"));
#endif
  }
#endif // DRAW_TEXT_SHADOW

  // Erase the graph pixels behind each text field before writing.
#ifdef DRAW_TEXT_RECTANGLE
  int16_t  x1, y1;
  uint16_t w, h;
  display.getTextBounds(buffer, TEXT_LPKM_X, TEXT_LPKM_Y, &x1, &y1, &w, &h);
  display.fillRect(x1, y1 - 1, w, h + 2, SSD1306_BLACK);

#ifdef DRAW_TEXT_TIMEBASE
  // Fixed-width clear: wide enough for "1000ms" (6 chars × 6px + margin)
  display.fillRect(TEXT_TIMEBASE_X, TEXT_TIMEBASE_Y - 1, 44, 9, SSD1306_BLACK);
#endif
#endif // DRAW_TEXT_RECTANGLE

  display.setTextColor(SSD1306_WHITE);
  display.setCursor(TEXT_LPKM_X, TEXT_LPKM_Y);
  display.print(buffer);

#ifdef DRAW_TEXT_TIMEBASE
  display.setCursor(TEXT_TIMEBASE_X, TEXT_TIMEBASE_Y);
  display.print(graph_interval_ms);
  display.print(F("ms"));
#endif
}
#endif // DRAW_TEXT

// ─────────────────────────────────────────────────────────────────────────────
// updateGraph()  — append new sample, recompute autoscale range
// ─────────────────────────────────────────────────────────────────────────────
void updateGraph() {
  // FIX 5: memmove is faster than a byte-by-byte for-loop on ESP8266.
  memmove(graphData, graphData + 1, (GRAPH_W - 1) * sizeof(graphData[0]));
  graphData[GRAPH_W - 1] = fuel_avg1_MLmin;   // float — no truncation

  // Autoscale: find min and max across the visible history.
#ifdef OPTIMIZED_MAX_SEARCH
  // Exclude the newest sample so the scale follows stable history, not the
  // live edge.  drawGraph() handles the out-of-range new sample safely (FIX 1).
  const uint8_t searchEnd = GRAPH_W - 1;
#else
  const uint8_t searchEnd = GRAPH_W;
#endif

  graphMin = graphData[0];
  graphMax = graphData[0];
  for (uint8_t i = 1; i < searchEnd; i++) {
    if (graphData[i] > graphMax) graphMax = graphData[i];
    if (graphData[i] < graphMin) graphMin = graphData[i];
  }
  if (graphMax <= graphMin) graphMax = graphMin + 1.0f;   // guarantee non-zero range
}

// ─────────────────────────────────────────────────────────────────────────────
// drawGraph()
// ─────────────────────────────────────────────────────────────────────────────
void drawGraph() {
  const float range = graphMax - graphMin;

  for (uint8_t i = 0; i < GRAPH_W; i++) {
    // FIX 1: clamp before scaling — when OPTIMIZED_MAX_SEARCH excludes the
    // newest sample, graphData[GRAPH_W-1] may be outside [graphMin, graphMax].
    // Without clamping, the unsigned subtraction wraps and produces a full-
    // height spike on the right edge.
    float val = graphData[i];
    if (val < graphMin) val = graphMin;
    if (val > graphMax) val = graphMax;

    uint8_t barHeight = (uint8_t)(((val - graphMin) / range) * GRAPH_H);
    display.drawFastVLine(GRAPH_X + i,
                          GRAPH_Y + (GRAPH_H - barHeight),
                          barHeight,
                          SSD1306_WHITE);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// loop()
// ─────────────────────────────────────────────────────────────────────────────
void loop() {
  const uint32_t currentMillis = millis();

  if (currentMillis - previousMillis < MEAS_INTERVAL_MS) return;
  previousMillis = currentMillis;

  // ── Measure elapsed real time using micros() for duty-cycle accuracy ────────
  const uint32_t current_micros    = micros();
  const uint32_t interval_duration = current_micros - last_interval_micros;
  last_interval_micros = current_micros;

  if (interval_duration == 0) return;   // guard division (extremely unlikely)

  // ── Atomic snapshot ──────────────────────────────────────────────────────────
  noInterrupts();
  const uint32_t pulseWidthSnapshot = totalPulseWidth;
  totalPulseWidth = 0;
  interrupts();

  // ── Duty cycle → flow rate ───────────────────────────────────────────────────
  // FIX 3: clamp duty to [0, 1] — near 100 % DC, ISR timing jitter can make
  // pulseWidthSnapshot slightly exceed interval_duration.
  float duty = (float)pulseWidthSnapshot / (float)interval_duration;
  if (duty > 1.0f) duty = 1.0f;

  // FIX 9: fuelConsumptionMLsec was a global used only here — now local.
  const float fuelConsumptionMLsec = duty * (injectorFlowRateMLmin / 60.0f);
  const float current_ml_min       = duty * injectorFlowRateMLmin;

  // Exponential moving average  α = 0.1  (time constant ≈ 10 × MEAS_INTERVAL)
  fuel_avg1_MLmin = (fuel_avg1_MLmin * 9.0f + current_ml_min) / 10.0f;

  // ── Knob → graph timebase ────────────────────────────────────────────────────
  const uint16_t knobRaw = (uint16_t)analogRead(KNOB_PIN);
  // FIX 6: only update when knob moves more than the hysteresis band, preventing
  // ADC noise from continuously jittering graph_interval_ms.
  if (abs((int16_t)knobRaw - (int16_t)knobValuePrev) > KNOB_HYSTERESIS) {
    knobValuePrev = knobRaw;
    uint16_t mapped = (uint16_t)map(knobRaw, 0, 1023, 10, 1000);
    // FIX 7: clamp so graph_interval_ms is never below the measurement cadence.
    // Values < MEAS_INTERVAL_MS would silently degrade to MEAS_INTERVAL_MS.
    if (mapped < MEAS_INTERVAL_MS) mapped = (uint16_t)MEAS_INTERVAL_MS;
    graph_interval_ms = mapped;
  }

  // ── Graph / display update (rate-limited by knob) ─────────────────────────
  if (currentMillis - previousMillis_graph < graph_interval_ms) return;
  previousMillis_graph = currentMillis;

  // Snap EMA to zero when injector has been idle for this entire graph window.
  if (current_ml_min == 0.0f && fuel_avg1_MLmin < 0.01f) {
    fuel_avg1_MLmin = 0.0f;
  }

  // Build the numeric label once; reuse for shadow + foreground to guarantee
  // identical geometry (prior round fixed shadow rendering the raw float).
#ifdef DRAW_TEXT
  char buffer[12];
  sprintf(buffer, "%u.%02u",
          (unsigned)fuel_avg1_MLmin,
          (unsigned)(fuel_avg1_MLmin * 100.0f) % 100u);
#endif

  display.clearDisplay();
  updateGraph();
  drawGraph();

#ifdef DRAW_TEXT
  updateDisplay(buffer);
#endif

  display.display();

  // Serial telemetry
  Serial.print(F("Fuel: "));
  Serial.print(fuelConsumptionMLsec, 3);
  Serial.print(F(" ml/s  Avg: "));
  Serial.print(fuel_avg1_MLmin, 2);
  Serial.print(F(" ml/min  T: "));
  Serial.print(graph_interval_ms);
  Serial.println(F(" ms"));
}
