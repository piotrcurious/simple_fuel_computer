Here is a possible code for your task. I have not tested it so it may not work as expected. Use it at your own risk.

```c
// Include the libraries for ESP32 and Bluedisplay
#include <Arduino.h>
#include <BlueDisplay.h>

// Define the pin for the fuel injector signal
#define INJECTOR_PIN 2

// Define the constants for the display
#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 240
#define GRAPH_X 10
#define GRAPH_Y 10
#define GRAPH_WIDTH (DISPLAY_WIDTH - 20)
#define GRAPH_HEIGHT (DISPLAY_HEIGHT - 20)
#define GRAPH_COLOR RGB(0, 255, 0)

// Define the variables for the fuel consumption calculation
float injectorPulseWidth; // in milliseconds
float fuelConsumption; // in milliliters per second
float fuelConsumptionSum; // in milliliters per second accumulated over one second
int pulseCount; // number of pulses in one second
unsigned long lastPulseTime; // time of the last pulse in microseconds
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

  // Attach an interrupt to the injector pin on falling edge
  attachInterrupt(digitalPinToInterrupt(INJECTOR_PIN), injectorInterrupt, FALLING);

  // Initialize the variables
  injectorPulseWidth = 0;
  fuelConsumption = 0;
  fuelConsumptionSum = 0;
  pulseCount = 0;
  lastPulseTime = micros();
  lastSecondTime = millis();
}

// Loop function
void loop() {
  // Check if one second has passed since the last update
  if (millis() - lastSecondTime >= 1000) {
    // Calculate the average fuel consumption in the last second
    fuelConsumption = fuelConsumptionSum / pulseCount;

    // Print the fuel consumption to serial monitor for debugging
    Serial.print("Fuel consumption: ");
    Serial.print(fuelConsumption);
    Serial.println(" ml/s");

    // Update the graph data with the new value
    graphData[graphIndex] = fuelConsumption;

    // Increment the graph index and wrap around if necessary
    graphIndex++;
    if (graphIndex > GRAPH_WIDTH-1) {
     graphIndex = GRAPH_WIDTH-1;


    // Shift the graph data array to the left by one position
     for (int i = 0; i < GRAPH_WIDTH - 1; i++) {
      graphData[i] = graphData[i + 1];
      }
    }

    // Draw the graph on the display
    drawGraph();

    // Reset the variables for the next second
    fuelConsumptionSum = 0;
    pulseCount = 0;
    lastSecondTime = millis();
  }
}

// Interrupt function for the injector signal
void injectorInterrupt() {
  // Get the current time in microseconds
  unsigned long currentTime = micros();

  // Calculate the pulse width in milliseconds
  injectorPulseWidth = (currentTime - lastPulseTime) / 1000.0;

  // Update the last pulse time with the current time
  lastPulseTime = currentTime;

  // Calculate the fuel consumption in milliliters per pulse using a constant factor of 0.01 (this may vary depending on your injector specifications)
  fuelConsumption = injectorPulseWidth * 0.01;

  // Add the fuel consumption to the sum for the current second
  fuelConsumptionSum += fuelConsumption;

  // Increment the pulse count for the current second
  pulseCount++;
}

// Function to draw the graph on the display
void drawGraph() {
  // Clear the graph area with black color
  myDisplay.fillRect(GRAPH_X, GRAPH_Y, GRAPH_X + GRAPH_WIDTH, GRAPH_Y + GRAPH_HEIGHT, BLACK);

  // Loop through the graph data array
  for (int i = 0; i < GRAPH_WIDTH; i++) {
    // Calculate the x coordinate of the current point
    int x = GRAPH_X + i;

    // Calculate the y coordinate of the current point using a scaling factor of 10 (this may vary depending on your fuel consumption range)
    int y = GRAPH_Y + GRAPH_HEIGHT - (graphData[i] * 10);

    // Draw a vertical line from the bottom to the current point with green color
    myDisplay.drawLine(x, GRAPH_Y + GRAPH_HEIGHT, x, y, GRAPH_COLOR);
  }
}
