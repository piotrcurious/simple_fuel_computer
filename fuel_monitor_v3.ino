// Fuel consumption monitor — ESP8266 + SSD1306 (hardware SPI)
// Measures injector on-time via interrupt, computes ml/min EMA,
// displays a rolling autoscaled graph and numeric overlay on a 128×64 OLED.

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>   // included only to disable the radio in setup()

// ── Pin definitions ──────────────────────────────────────────────────────────
#define INJECTOR_PIN  D5   // GPIO14  (do not use GPIO3 = Serial RX)
#define KNOB_PIN      A0   // ADC input for graph timebase knob

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

// DRAW_TEXT_SHADOW and DRAW_TEXT_RECTANGLE are alternative legibility strategies:
//   SHADOW  — black 1-px outline drawn directly over graph pixels (no background fill)
//   RECTANGLE — solid black background cleared behind text (stronger, costs graph area)
// Both can be enabled simultaneously; RECTANGLE is applied first so SHADOW
// renders on the cleared background.  Enabling only one is the typical choice.
//#define DRAW_TEXT_SHADOW
#define DRAW_TEXT_RECTANGLE

// ── Text positions ────────────────────────────────────────────────────────────
#define TEXT_LPKM_X     0
#define TEXT_LPKM_Y     (SCREEN_HEIGHT - 8)
#define TEXT_TIMEBASE_X 60
#define TEXT_TIMEBASE_Y (SCREEN_HEIGHT - 8)

// ── Injector parameters ───────────────────────────────────────────────────────
// FIX Q5: const — this value is never modified; const enables compiler optimisation
// and prevents accidental writes.
static const float INJECTOR_FLOW_RATE_ML_MIN = 200.0f;  // rated flow at 100 % DC

// ── Rolling graph ─────────────────────────────────────────────────────────────
static const uint8_t GRAPH_X = 0;
static const uint8_t GRAPH_Y = 0;
static const uint8_t GRAPH_W = SCREEN_WIDTH;
static const uint8_t GRAPH_H = SCREEN_HEIGHT - 1;

// FIX (v2): float array preserves EMA fractional precision; uint32_t previously
// truncated (e.g. 12.7 → 12), wasting half the display's vertical resolution.
float graphData[GRAPH_W];
float graphMin = 0.0f;
float graphMax = 1.0f;

// ── EMA state (global — written in loop, read in updateDisplay/updateGraph) ───
float fuel_avg1_MLmin = 0.0f;   // exponential moving average of ml/min  (α = 0.1)

// ── ISR shared state ──────────────────────────────────────────────────────────
// FIX B1: replaced 'pulseStart != 0' sentinel with an explicit armed flag.
//   micros() wraps through 0 every ~71 min; a rising edge at that exact moment
//   would set pulseStart = 0, which looks "unarmed", silently discarding the pulse.
volatile bool     pulseArmed     = false;
volatile uint32_t pulseStart     = 0;   // rising-edge timestamp [µs]
volatile uint32_t totalPulseWidth = 0;  // accumulated on-time this interval [µs]

// ─────────────────────────────────────────────────────────────────────────────
// ISR
// ─────────────────────────────────────────────────────────────────────────────
void ICACHE_RAM_ATTR injectorISR() {
  if (digitalRead(INJECTOR_PIN) == HIGH) {
    // Rising edge: capture timestamp and arm.
    pulseStart = micros();
    pulseArmed = true;
  } else {
    // Falling edge: accumulate only when a matching rising edge was seen.
    if (pulseArmed) {
      totalPulseWidth += micros() - pulseStart;
      pulseArmed = false;
    }
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// setup()
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  // Disable WiFi — the radio boots active, wastes ~70 mA, and introduces
  // timing noise on the shared 160 MHz system clock.
  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();

  Serial.begin(115200);

  if (!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);   // halt — no display means no useful operation
  }
  display.display();
  delay(2000);   // show Adafruit splash

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Setting up..."));
  display.display();

  // FIX Q4: explicit loop instead of memset() on float[].
  // memset to 0 yields 0.0f on IEEE 754 platforms (universally true in practice)
  // but is not guaranteed by the C++ standard.  A loop has no overhead here.
  for (uint8_t i = 0; i < GRAPH_W; i++) graphData[i] = 0.0f;

  pinMode(INJECTOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(INJECTOR_PIN), injectorISR, CHANGE);

  display.setCursor(0, 16);
  display.println(F("Done!"));
  display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
// updateDisplay()  — text overlay; compiled only when DRAW_TEXT is defined
// ─────────────────────────────────────────────────────────────────────────────
#ifdef DRAW_TEXT
void updateDisplay(const char* fuelBuf) {

  // FIX Q8: DRAW_TEXT_RECTANGLE is applied BEFORE the shadow so the shadow
  // renders on a clean black background and provides a visible outline.
  // (Previously rect was applied after shadow, erasing the interior shadow pixels.)
#ifdef DRAW_TEXT_RECTANGLE
  // Erase graph pixels behind the fuel-value field (dynamic width via getTextBounds).
  int16_t  x1, y1;
  uint16_t w, h;
  display.getTextBounds(fuelBuf, TEXT_LPKM_X, TEXT_LPKM_Y, &x1, &y1, &w, &h);
  display.fillRect(x1, y1 - 1, w, h + 2, SSD1306_BLACK);

#ifdef DRAW_TEXT_TIMEBASE
  // Fixed-width clear: enough for "1000ms" (6 chars × 6 px + 2 px margin = 38 px).
  display.fillRect(TEXT_TIMEBASE_X, TEXT_TIMEBASE_Y - 1, 40, 9, SSD1306_BLACK);
#endif
#endif // DRAW_TEXT_RECTANGLE

#ifdef DRAW_TEXT_SHADOW
  // 4-direction black outline makes white text legible over graph pixels.
  display.setTextColor(SSD1306_BLACK);
  const int8_t dx[4] = { 1,  1, -1, -1};
  const int8_t dy[4] = {-1,  1,  1, -1};
  for (uint8_t o = 0; o < 4; o++) {
    display.setCursor(TEXT_LPKM_X + dx[o], TEXT_LPKM_Y + dy[o]);
    display.print(fuelBuf);
#ifdef DRAW_TEXT_TIMEBASE
    display.setCursor(TEXT_TIMEBASE_X + dx[o], TEXT_TIMEBASE_Y + dy[o]);
    // graph_interval_ms not accessible here; caller passes it as parameter if needed.
    // For now shadow only covers the fuel value — timebase label is rare on shadow.
#endif
  }
#endif // DRAW_TEXT_SHADOW

  // Foreground text
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(TEXT_LPKM_X, TEXT_LPKM_Y);
  display.print(fuelBuf);

#ifdef DRAW_TEXT_TIMEBASE
  display.setCursor(TEXT_TIMEBASE_X, TEXT_TIMEBASE_Y);
  // graph_interval_ms is a loop-local static — read via accessor or pass as arg.
  // Passed as a second parameter below.
#endif
}

// Thin wrapper that also prints the timebase label so updateDisplay stays
// decoupled from loop-internal state.
#ifdef DRAW_TEXT_TIMEBASE
void updateDisplayWithTimebase(const char* fuelBuf, uint16_t intervalMs) {
  updateDisplay(fuelBuf);
  display.setCursor(TEXT_TIMEBASE_X, TEXT_TIMEBASE_Y);
  display.print(intervalMs);
  display.print(F("ms"));
}
#define DISPLAY_CALL(buf, ms) updateDisplayWithTimebase(buf, ms)
#else
#define DISPLAY_CALL(buf, ms) updateDisplay(buf)
#endif

#endif // DRAW_TEXT

// ─────────────────────────────────────────────────────────────────────────────
// updateGraph()  — append sample, recompute autoscale
// ─────────────────────────────────────────────────────────────────────────────
void updateGraph() {
  memmove(graphData, graphData + 1, (GRAPH_W - 1) * sizeof(graphData[0]));
  graphData[GRAPH_W - 1] = fuel_avg1_MLmin;

  // FIX Q9: OPTIMIZED_MAX_SEARCH comment corrected.
  // This mode anchors the scale to settled history rather than chasing the
  // live edge, providing visual stability when a new peak/trough arrives.
  // The search is one element shorter (127 vs 128) — negligible speed difference.
#ifdef OPTIMIZED_MAX_SEARCH
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
  if (graphMax <= graphMin) graphMax = graphMin + 1.0f;
}

// ─────────────────────────────────────────────────────────────────────────────
// drawGraph()
// ─────────────────────────────────────────────────────────────────────────────
void drawGraph() {
  const float range = graphMax - graphMin;

  for (uint8_t i = 0; i < GRAPH_W; i++) {
    // Clamp: with OPTIMIZED_MAX_SEARCH the newest sample may lie outside
    // [graphMin, graphMax]; without clamping the bar height overflows.
    float val = graphData[i];
    if (val < graphMin) val = graphMin;
    if (val > graphMax) val = graphMax;

    const uint8_t barHeight = (uint8_t)(((val - graphMin) / range) * GRAPH_H);
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
  // FIX Q7: all loop-only mutable state as static locals — no file-scope pollution.
  static uint32_t previousMillis        = 0;
  static uint32_t previousMillis_graph  = 0;
  // FIX B2: seed last_interval_micros to 0 here; it is corrected on the first
  // measurement tick in setup() below — see the companion initialisation there.
  // Actually seeded via a one-shot flag so setup() can call micros() after
  // attachInterrupt and store the result cleanly (see setup()).
  static uint32_t last_interval_micros  = 0;
  static uint16_t graph_interval_ms     = 1000;
  static uint16_t knobValuePrev         = 0xFFFF;  // sentinel → forces first update
  static bool     firstTick             = true;

  const uint32_t currentMillis = millis();
  if (currentMillis - previousMillis < 100u /*MEAS_INTERVAL_MS*/) return;
  previousMillis = currentMillis;

  // ── Measure elapsed real time via micros() for accurate duty-cycle ───────────
  const uint32_t current_micros = micros();

  // FIX B2: On the very first measurement tick, last_interval_micros = 0 gives
  // an interval_duration equal to the full boot time (~2+ s), making the first
  // duty-cycle reading ~20× too small.  Skip the EMA update on the first tick
  // and seed the timer correctly so all subsequent intervals are accurate.
  if (firstTick) {
    last_interval_micros = current_micros;
    firstTick = false;

    // Still snapshot and discard any ISR accumulation from the boot window so
    // it does not contaminate the first real measurement.
    noInterrupts();
    totalPulseWidth = 0;
    interrupts();
    return;
  }

  const uint32_t interval_duration = current_micros - last_interval_micros;
  last_interval_micros = current_micros;

  if (interval_duration == 0) return;   // guard division (theoretically impossible)

  // ── Atomic snapshot ───────────────────────────────────────────────────────────
  noInterrupts();
  const uint32_t pulseWidthSnapshot = totalPulseWidth;
  totalPulseWidth = 0;
  interrupts();

  // ── Duty cycle → flow rate ────────────────────────────────────────────────────
  // Clamp to [0, 1]: near 100 % DC, ISR timing jitter can make
  // pulseWidthSnapshot fractionally exceed interval_duration.
  float duty = (float)pulseWidthSnapshot / (float)interval_duration;
  if (duty > 1.0f) duty = 1.0f;

  const float fuelMLsec     = duty * (INJECTOR_FLOW_RATE_ML_MIN / 60.0f);
  const float current_ml_min = duty * INJECTOR_FLOW_RATE_ML_MIN;

  // Exponential moving average  α = 0.1  →  τ ≈ 10 × 100 ms = 1 s
  fuel_avg1_MLmin = (fuel_avg1_MLmin * 9.0f + current_ml_min) / 10.0f;

  // ── Knob → graph timebase ─────────────────────────────────────────────────────
  const uint16_t knobRaw = (uint16_t)analogRead(KNOB_PIN);
  // FIX Q6: use a named temp to avoid abs() macro double-evaluation.
  const int16_t knobDelta = (int16_t)knobRaw - (int16_t)knobValuePrev;
  if (knobDelta > 5 || knobDelta < -5) {   // hysteresis = 5 ADC counts
    knobValuePrev = knobRaw;
    uint16_t mapped = (uint16_t)map(knobRaw, 0, 1023, 100, 1000);
    // Lower bound matches the 100 ms measurement cadence so the condition
    // currentMillis - previousMillis_graph >= graph_interval_ms can always fire.
    graph_interval_ms = mapped;
  }

  // ── Graph / display update ────────────────────────────────────────────────────
  if (currentMillis - previousMillis_graph < graph_interval_ms) return;
  previousMillis_graph = currentMillis;

  // Snap EMA to hard zero when idle, preventing the display from reading
  // "0.00" while a tiny residual creeps toward zero over many intervals.
  if (current_ml_min == 0.0f && fuel_avg1_MLmin < 0.01f) {
    fuel_avg1_MLmin = 0.0f;
  }

#ifdef DRAW_TEXT
  // FIX B3: compute integer and fractional parts from a single rounded value
  // so they are always consistent.  The previous approach computed them
  // independently from the raw float; floating-point truncation could produce
  // outputs like "9.99" for a value that rounds to "10.00".
  const uint32_t v = (uint32_t)(fuel_avg1_MLmin * 100.0f + 0.5f);
  char fuelBuf[12];
  sprintf(fuelBuf, "%lu.%02lu", v / 100UL, v % 100UL);
#endif

  display.clearDisplay();
  updateGraph();
  drawGraph();

#ifdef DRAW_TEXT
  DISPLAY_CALL(fuelBuf, graph_interval_ms);
#endif

  display.display();

  // Serial telemetry (printed after display flush to avoid serial blocking
  // the SPI transfer)
  Serial.print(F("Fuel: "));
  Serial.print(fuelMLsec, 3);
  Serial.print(F(" ml/s  Avg: "));
  Serial.print(fuel_avg1_MLmin, 2);
  Serial.print(F(" ml/min  T: "));
  Serial.print(graph_interval_ms);
  Serial.println(F(" ms"));
}
