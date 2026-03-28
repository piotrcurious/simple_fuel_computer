
// Include the libraries for ESP32 and Bluedisplay
#include <Arduino.h>
#include <BlueDisplay.h>

// Define the pin for the fuel injector signal
#define INJECTOR_PIN 4

// Define the constants for the display
#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 240
#define GRAPH_X 10
#define GRAPH_Y 10
#define GRAPH_WIDTH (DISPLAY_WIDTH - 20)
#define GRAPH_HEIGHT (DISPLAY_HEIGHT - 20)
#define GRAPH_COLOR RGB(0, 255, 0)

// Define the variables for the fuel consumption calculation
volatile unsigned long pulseStart = 0; // The start time of the current pulse in microseconds
volatile unsigned long totalPulseWidth = 0; // The total width of all pulses in the accumulation interval
float fuelConsumptionMLsec = 0; // The fuel consumption in milliliters per second
float fuelSmoothedMLsec = 0; // Smoothed fuel consumption
float injectorFlowRateMLmin = 200.0; // The injector flow rate in milliliters per minute
unsigned long lastSecondTime; // time of the last second in milliseconds

// Define the array for storing the graph data
float graphData[GRAPH_WIDTH];

// Define the index for the graph data
int graphIndex;

// Initialize the display object
BlueDisplay myDisplay;

// Setup function
void setup() {
  // Initialize serial communication
  Serial.begin(115200);

  // Initialize the display connection
  myDisplay.connectToDisplay();

  // Set the display orientation to landscape
  myDisplay.setOrientation(1);

  // Clear the display with black color
  myDisplay.clearDisplay(BLACK);

  // Draw a rectangle around the graph area
  myDisplay.drawRect(GRAPH_X - 1, GRAPH_Y - 1, GRAPH_X + GRAPH_WIDTH + 1, GRAPH_Y + GRAPH_HEIGHT + 1, WHITE);

  // Initialize the injector pin as input with pullup resistor
  pinMode(INJECTOR_PIN, INPUT_PULLUP);

  // Attach an interrupt to the injector pin on state change
  attachInterrupt(digitalPinToInterrupt(INJECTOR_PIN), injectorInterrupt, CHANGE);

  // Initialize the variables
  fuelConsumptionMLsec = 0;
  totalPulseWidth = 0;
  lastSecondTime = millis();
}

// Loop function
void loop() {
  // Check if one second has passed since the last update
  if (millis() - lastSecondTime >= 1000) {
    noInterrupts();
    unsigned long pulseWidthSnapshot = totalPulseWidth;
    totalPulseWidth = 0;
    interrupts();

    // Calculate the average fuel consumption in the last second
    // totalPulseWidth is in microseconds.
    // injectorFlowRateMLmin is in ml/min. ml/sec = injectorFlowRateMLmin / 60.
    fuelConsumptionMLsec = (pulseWidthSnapshot / 1000000.0) * (injectorFlowRateMLmin / 60.0);

    // Smooth the value
    fuelSmoothedMLsec = (fuelSmoothedMLsec * 0.7) + (fuelConsumptionMLsec * 0.3);
    if (fuelConsumptionMLsec == 0) fuelSmoothedMLsec *= 0.5;
    if (fuelSmoothedMLsec < 0.0001) fuelSmoothedMLsec = 0;

    // Print the fuel consumption to serial monitor for debugging
    Serial.print("Fuel consumption: ");
    Serial.print(fuelSmoothedMLsec);
    Serial.println(" ml/s");

    // Shift the graph data array to the left by one position
    for (int i = 0; i < GRAPH_WIDTH - 1; i++) {
      graphData[i] = graphData[i + 1];
    }
    // Update the graph data with the new value at the end
    graphData[GRAPH_WIDTH - 1] = fuelSmoothedMLsec;

    // Draw the graph on the display
    drawGraph();

    // Reset the variables for the next second
    lastSecondTime = millis();
  }
}

// Interrupt function for the injector signal
void injectorInterrupt() {
  if (digitalRead(INJECTOR_PIN) == HIGH) {
    pulseStart = micros();
  } else {
    totalPulseWidth += (micros() - pulseStart);
  }
}

// Function to draw the graph on the display
void drawGraph() {
  // Clear the graph area with black color
  myDisplay.fillRect(GRAPH_X, GRAPH_Y, GRAPH_X + GRAPH_WIDTH, GRAPH_Y + GRAPH_HEIGHT, BLACK);

  // Find max value for scaling
  float maxVal = 0.1; // Minimum scale
  for (int i = 0; i < GRAPH_WIDTH; i++) {
    if (graphData[i] > maxVal) maxVal = graphData[i];
  }

  // Loop through the graph data array
  for (int i = 0; i < GRAPH_WIDTH; i++) {
    // Calculate the x coordinate of the current point
    int x = GRAPH_X + i;

    // Calculate the y coordinate of the current point
    int y = GRAPH_Y + GRAPH_HEIGHT - (int)((graphData[i] / maxVal) * GRAPH_HEIGHT);

    // Draw a vertical line from the bottom to the current point with green color
    myDisplay.drawLine(x, GRAPH_Y + GRAPH_HEIGHT, x, y, GRAPH_COLOR);
  }
}
