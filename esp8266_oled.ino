
// Include the libraries for the display and the ESP8266
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>

// Define the pins for the display and the injector signal
#define OLED_RESET 0  // GPIO0
#define INJECTOR_PIN 3 // GPIO5

// Create an object for the display
Adafruit_SSD1306 display(OLED_RESET);

// Declare some variables for the fuel consumption calculation
volatile unsigned long pulseStart = 0; // The start time of the current pulse in microseconds
volatile unsigned long totalPulseWidth = 0; // The total width of all pulses in the accumulation interval
float fuelConsumptionMLsec = 0; // The fuel consumption in milliliters per second
float fuelSmoothedMLsec = 0; // Smoothed fuel consumption
float injectorFlowRateMLmin = 200.0; // The injector flow rate in milliliters per minute

// Declare some variables for the rolling graph display
int graphX = 0; // The x coordinate of the graph
int graphY = 0; // The y coordinate of the graph
int graphWidth = 128; // The width of the graph
int graphHeight = 32; // The height of the graph
int graphScale = 10; // The scale factor of the graph

// Declare a flag variable to indicate when to update the display
volatile bool updateDisplay = false;

// This function is called when the injector signal changes state
void injectorISR() {
  // Check if the signal is high or low
  if (digitalRead(INJECTOR_PIN) == HIGH) {
    // If high, record the start time of the pulse
    pulseStart = micros();
  } else {
    // If low, calculate the pulse width and add it to the total
    totalPulseWidth += (micros() - pulseStart);
  }
}

// This function is called periodically by a timer interrupt
void timerISR() {
  static uint32_t last_micros = 0;
  uint32_t current_micros = micros();
  uint32_t duration = current_micros - last_micros;
  if (duration == 0) return;
  last_micros = current_micros;

  // totalPulseWidth is in microseconds.
  // injectorFlowRateMLmin is in ml/min. ml/sec = injectorFlowRateMLmin / 60.
  // fuelConsumptionMLsec in ml/sec = (totalPulseWidth / (float)duration) * (injectorFlowRateMLmin / 60.0);
  float instant_fuel = (totalPulseWidth / (float)duration) * (injectorFlowRateMLmin / 60.0);
  if (totalPulseWidth == 0) instant_fuel = 0;
  fuelConsumptionMLsec = instant_fuel;
  
  // Reset the total pulse width for the next interval
  totalPulseWidth = 0;
  
  // Set the flag variable to true to indicate that the display needs to be updated
  updateDisplay = true;
}

// This function initializes the display and sets up the interrupts
void setup() {
  
  // Initialize serial communication at 9600 baud rate
  Serial.begin(9600);
  
  // Initialize the display with I2C address 0x3C and reset pin OLED_RESET
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  
  // Clear the display buffer and set text size and color
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  
  // Display a message on the screen while setting up
  display.setCursor(0,0);
  display.println("Setting up...");
  
   // Set up a timer interrupt to call timerISR every 0.1 second (80MHz/16/500000)
   timer1_attachInterrupt(timerISR);
   timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);
   timer1_write(500000);
   
   // Set up a pin interrupt to call injectorISR on every change of state of INJECTOR_PIN 
   pinMode(INJECTOR_PIN, INPUT_PULLUP);
   attachInterrupt(digitalPinToInterrupt(INJECTOR_PIN), injectorISR, CHANGE);
   
   // Display a message on the screen when done setting up
   display.setCursor(0,16);
   display.println("Done!");
   display.display();
}

// This function updates the display with the rolling graph of fuel consumption
void loop() {
  float fuel_copy;
  
  // Check if the flag variable is true
  if (updateDisplay) {
    noInterrupts();
    fuel_copy = fuelConsumptionMLsec;
    updateDisplay = false;
    interrupts();

    // Smooth the value
    fuelSmoothedMLsec = (fuelSmoothedMLsec * 0.7) + (fuel_copy * 0.3);
    if (fuel_copy == 0) fuelSmoothedMLsec *= 0.5;
    if (fuelSmoothedMLsec < 0.0001) fuelSmoothedMLsec = 0;

    Serial.print("Fuel: "); Serial.print(fuelSmoothedMLsec); Serial.println(" ml/sec");
    
    // Draw a line on the graph representing the fuel consumption
    display.drawLine(graphX, graphY + graphHeight - 1, graphX, graphY + graphHeight - (fuelSmoothedMLsec * graphScale), WHITE);
    
    // Increment the x coordinate of the graph and wrap around if necessary
    graphX++;
    if (graphX >= graphWidth) {
      graphX = 0;
      // Clear the display before drawing a new cycle of the graph
      display.clearDisplay();
      display.display();
    }
    
    // Update the display with the new line
    display.display();
    
    }
}
