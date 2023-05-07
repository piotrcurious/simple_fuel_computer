
// Include the Adafruit library for the display
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Define the display size and the pins
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET    -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define INJECTOR_PIN 2 // Pin connected to the injector signal

// Create an object for the display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Define some constants for the fuel consumption calculation
#define INJECTOR_CC 200 // Injector capacity in cc/min
#define FUEL_DENSITY 0.75 // Fuel density in g/cc
#define SECONDS_PER_HOUR 3600 // Number of seconds in an hour

// Define some variables for the rolling graph
int graph_x = 0; // The x position of the graph
int graph_y = 0; // The y position of the graph
int graph_width = SCREEN_WIDTH; // The width of the graph
int graph_height = SCREEN_HEIGHT; // The height of the graph
int graph_max = 20; // The maximum value of the graph in g/s
int graph_data[SCREEN_WIDTH]; // The array to store the graph data

// Define some variables for the injector pulse measurement
unsigned long pulse_start = 0; // The start time of the pulse in microseconds
unsigned long pulse_end = 0; // The end time of the pulse in microseconds
unsigned long pulse_width = 0; // The width of the pulse in microseconds
unsigned long last_second = 0; // The last time a second has passed in milliseconds
unsigned long current_second = 0; // The current time in milliseconds
float fuel_consumed = 0; // The fuel consumed in grams

void setup() {
  // Initialize the display and clear it
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C); 
  display.clearDisplay();

  // Set the text size and color
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Draw a rectangle around the graph area
  display.drawRect(graph_x, graph_y, graph_width, graph_height, SSDD1306_WHITE);

  // Display the initial screen
  display.display();

  // Set the injector pin as input and attach an interrupt to it
  pinMode(INJECTOR_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(INJECTOR_PIN), injectorISR, CHANGE);

}

void loop() {
  
  // Get the current time in milliseconds
  current_second = millis();

  // Check if a second has passed since the last update
  if (current_second - last_second >= 1000) {

    // Update the last second time
    last_second = current_second;

    // Calculate the fuel consumption in grams per second
    float fuel_consumption = fuel_consumed / SECONDS_PER_HOUR;

    // Add the fuel consumption to the graph data array and shift it to the left
    for (int i = 0; i < graph_width - 1; i++) {
      graph_data[i] = graph_data[i + 1];
    }
    graph_data[graph_width - 1] = fuel_consumption;

    // Clear the previous graph area
    display.fillRect(graph_x + 1, graph_y + 1, graph_width -2 , graph_height -2 , SSD1306_BLACK);

    // Draw the new graph data as vertical lines
    for (int i = 0; i < graph_width; i++) {
      int line_height = map(graph_data[i], 0, graph_max, 0, graph_height);
      display.drawFastVLine(graph_x + i, graph_y + graph_height - line_height, line_height, SSD1306_WHITE);
    }

    // Display the updated screen
    display.display();

    // Reset the fuel consumed variable for the next second
    fuel_consumed = 0;
    
  
}

// Define a function to handle the injector signal interrupt
void injectorISR() {

  // Check if the injector pin is high or low
  if (digitalRead(INJECTOR_PIN) == HIGH) {

    // If high, record the start time of the pulse
    pulse_start = micros();
    
  
} else {

    // If low, record the end time of the pulse and calculate its width
    pulse_end = micros();
    pulse_width = pulse_end - pulse_start;

    // Calculate the fuel injected in grams based on the pulse width and injector capacity and density
    float fuel_injected = (pulse_width / SECONDS_PER_HOUR) * (INJECTOR_CC / FUEL_DENSITY);

    // Add the fuel injected to the total fuel consumed variable 
    fuel_consumed += fuel_injected;
    
  
}
