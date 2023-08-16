
// Include the libraries for the display and the ESP8266
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>

// Define the pins for the display and the injector signal
//#define OLED_RESET 0  // GPIO0
#define INJECTOR_PIN D1 // GPIO5

// Create an object for the display
//Adafruit_SSD1306 display(OLED_RESET);
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Define text parameters
#define DRAW_TEXT // if draw text at all
#define DRAW_TEXT_TIMEBASE // if draw timebase
//#define DRAW_TEXT_SHADOW // if cast shadow 
#define DRAW_TEXT_RECTANGLE // if clear space for graph by drawing rectangle

#define TEXT_LPKM_X 0 // x position of CPM text
#define TEXT_LPKM_Y SCREEN_HEIGHT-8 // y position of CPM text

#define TEXT_TIMEBASE_X 60 // x position of timebase text
#define TEXT_TIMEBASE_Y SCREEN_HEIGHT-8 // y position of timebase text


// Declaration for SSD1306 display connected using software SPI (default case):
/*
#define OLED_MOSI   D7 
#define OLED_CLK    D5
#define OLED_DC     D2
#define OLED_CS     D8
#define OLED_RESET  D3
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
  OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);
*/
///*
// Comment out above, uncomment this block to use hardware SPI
#define OLED_DC     D2
#define OLED_CS     D8
#define OLED_RESET  D3
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
  &SPI, OLED_DC, OLED_RESET, OLED_CS);
//*/

// Define the pin for the knob
#define KNOB_PIN A0 // Analog pin for the knob


// Declare some variables for the fuel consumption calculation
volatile unsigned long pulseStart = 0; // The start time of the current pulse in microseconds
volatile unsigned long pulseWidth = 0; // The width of the current pulse in microseconds
volatile uint32_t totalPulseWidth = 0; // The total width of all pulses in one second in microseconds
float fuelConsumption = 0; // The fuel consumption in milliliters per second
float injectorFlowRate = 10; // The injector flow rate fudge factor

volatile uint32_t counts = 0; // accumulated pulses
volatile uint32_t counts_copy = 0; // accumulated pulses, outside ISR copy
volatile float fuel_avg1    = 0; // average fuel consumption


uint32_t previousMillis = 0; // Previous time in milliseconds
uint32_t previousMillis_serial = 0; // Previous time in milliseconds for serial out routine
uint32_t previousMillis_graph = 0; // Previous time in milliseconds for graph updat

// Declare some variables for the rolling graph display
//uint8_t graphX = 0; // The x coordinate of the graph
//uint8_t graphY = 0; // The y coordinate of the graph
//uint8_t graphWidth = 128; // The width of the graph
//uint8_t graphHeight = 32; // The height of the graph
//float graphScale = 10; // The scale factor of the graph

// Create some variables for the rolling graph
uint8_t graphX = 0;             // X position of the graph
uint8_t graphY = 0;             // Y position of the graph
uint8_t graphW = SCREEN_WIDTH;  // Width of the graph
uint8_t graphH = SCREEN_HEIGHT-1; // Height of the graph
uint32_t graphMin = 0 ;          // minimum value of the graph
uint32_t graphMax = 100;         // Maximum value of the graph
#define OPTIMIZED_MAX_SEARCH    // faster for bigger displays
 //but does not include most recent value in the search 

// Create an array to store the graph data
uint32_t graphData[SCREEN_WIDTH] ;

uint32_t interval = 100;     // Interval to update fuel consumption 
uint32_t graph_interval = 1000 ; // Interval to update graph , adjustable by a knob 
uint8_t  display_brightness ; // global variable to adjust display brightness

// Create some variables for the knob
uint16_t knobValue = 0; // Value read from the knob
uint8_t timeBase = 1; // Time base for the rolling graph in seconds

// Declare a flag variable to indicate when to update the display
volatile bool updateDisplay_flag = false;

// This function is called when the injector signal changes state
void ICACHE_RAM_ATTR injectorISR() {
  // Check if the signal is high or low
  if (digitalRead(INJECTOR_PIN) == HIGH) {
    // If high, record the start time of the pulse
    pulseStart = micros();
  } else {
    // If low, calculate the pulse width and add it to the total
    pulseWidth = micros() - pulseStart;
    totalPulseWidth += pulseWidth;    // add up the pulses to calculate momentary consumption 
    counts += pulseWidth;             // add up the pulses to calculate average   consumption
  }
}

// This function is called every second by a timer interrupt
void ICACHE_RAM_ATTR timerISR() {
  // Calculate the fuel consumption based on the total pulse width and the injector flow rate
//  fuelConsumption = (totalPulseWidth / 1000000.0) * (injectorFlowRate / 60.0);
  fuelConsumption = totalPulseWidth  * injectorFlowRate ;
  
  // Reset the total pulse width for the next second
  totalPulseWidth = 0;
  
  // Display the fuel consumption on the serial monitor for debugging
  //Serial.print("Fuel consumption: ");
  //Serial.print(fuelConsumption);
  //Serial.println(" ml/s");
  
  // Set the flag variable to true to indicate that the display needs to be updated
  updateDisplay_flag = true;
}

// This function initializes the display and sets up the interrupts
void setup() {
  
  // Initialize serial communication at 9600 baud rate
  Serial.begin(9600);
  
  // Initialize the display with I2C address 0x3C and reset pin OLED_RESET
  //display.begin(SSD1306_SWITCHCAPVCC);

  if(!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  display.display();
  delay(2000); // Pause for 2 seconds
  
  // Clear the display buffer and set text size and color
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  
  // Display a message on the screen while setting up
  display.setCursor(0,0);
  display.println("Setting up...");
  display.display();
  
   // Set up a timer interrupt to call timerISR every second
   timer1_attachInterrupt(timerISR);
   timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);
   timer1_write(500000); // Set timer interval to 0.1 sec (80MHz/16/500000)
   timer1_write(1000000); // Set timer interval to 0.1 sec (160MHz/16/500000)
                          
   // Set up a pin interrupt to call injectorISR on every change of state of INJECTOR_PIN 
//   pinMode(INJECTOR_PIN, INPUT_PULLUP);
   pinMode(INJECTOR_PIN, INPUT);

   //attachInterrupt(digitalPinToInterrupt(INJECTOR_PIN), injectorISR, CHANGE);
   attachInterrupt(INJECTOR_PIN, injectorISR, CHANGE);
   
   // Display a message on the screen when done setting up
   display.setCursor(0,16);
   display.println("Done!");
   display.display();
}

#ifdef DRAW_TEXT
// Update OLED display with new data
void updateDisplay() {

#ifdef DRAW_TEXT_SHADOW
//cast +1 -1 shadow first
  display.setTextColor(SSD1306_BLACK);
  // Set the cursor position and print CPM value
  display.setCursor(TEXT_LPKM_X+1, TEXT_LPKM_Y-1);
  //display.print("CPM: ");
  display.print(fuel_avg1);
  
  display.setCursor(TEXT_TIMEBASE_X+1, TEXT_TIMEBASE_Y-1);
  display.print("T: ");
  display.print(timeBase);
  display.print("s");
  
//cast +1 +1 shadow 
  display.setTextColor(SSD1306_BLACK);
  // Set the cursor position and print CPM value
  display.setCursor(TEXT_LPKM_X+1, TEXT_LPKM_Y+1);
  //display.print("CPM: ");
  display.print(fuel_avg1);
  
  // Set the cursor position and print time base value
  display.setCursor(TEXT_TIMEBASE_X+1, TEXT_TIMEBASE_Y+1);
  display.print("T: ");
  display.print(timeBase);
  display.print("s");

//cast -1 +1 shadow first
  display.setTextColor(SSD1306_BLACK);
  // Set the cursor position and print CPM value
  display.setCursor(TEXT_LPKM_X-1, TEXT_LPKM_Y+1);
  //display.print("CPM: ");
  display.print(fuel_avg1);
  
  // Set the cursor position and print time base value
  display.setCursor(TEXT_TIMEBASE_X-1, TEXT_TIMEBASE_Y+1);
  display.print("T: ");
  display.print(timeBase);
  display.print("s");
#endif //DRAW_TEXT_SHADOW

#ifdef DRAW_TEXT_RECTANGLE
int16_t  x1, y1;
uint16_t w, h;

char buffer[40];
  sprintf(buffer, "%d.%02d",(uint16_t)fuel_avg1, (uint16_t)(fuel_avg1*100)%100);
display.getTextBounds(buffer, TEXT_LPKM_X, TEXT_LPKM_Y, &x1, &y1, &w, &h);
display.fillRect(x1,y1-1,w,h+1,BLACK); // Clear the area below CPM value
#endif // DRAW_TEXT_RECTANGLE

//paint text
  display.setTextColor(SSD1306_WHITE);
  // Set the cursor position and print CPM value
  display.setCursor(TEXT_LPKM_X, TEXT_LPKM_Y);
  //display.print("CPM: ");
  display.print(buffer);

#ifdef DRAW_TEXT_TIMEBASE  
#ifdef DRAW_TEXT_RECTANGLE
display.fillRect(TEXT_TIMEBASE_X,TEXT_TIMEBASE_Y-1,40,9,BLACK); // Clear the area below timebase value
#endif // DRAW_TEXT_RECTANGLE

  // Set the cursor position and print time base value
  display.setCursor(TEXT_TIMEBASE_X, TEXT_TIMEBASE_Y);
  display.print("T: ");
  display.print(timeBase);
  display.print("ms");
#endif //DRAW_TEXT_TIMEBASE
}
#endif DRAW_TEXT

// Update rolling graph with new data
void updateGraph() {
#ifdef OPTIMIZED_MAX_SEARCH
// Shift the graph data to the left by one pixel and find the maximum value
  graphMin = graphMax; // set graphMin to last graphMax value
  graphMax = 0;
  for (int i = 0; i < graphW - 1; i++) {
    graphData[i] = graphData[i + 1];
    if (graphData[i] > graphMax) {
      graphMax = graphData[i];
      }
    if (graphData[i] < graphMin) {
      graphMin = graphData[i];
    }
    // graphMax = max(graphMax, graphData[i]); 
    // or use that instead 
  }
#endif OPTIMIZED_MAX_SEARCH

#ifndef OPTIMIZED_MAX_SEARCH
  // Shift the graph data to the left by one pixel
  for (int i = 0; i < graphW - 1; i++) {
    graphData[i] = graphData[i + 1];
  }
#endif OPTIMIZED_MAX_SEARCH
  
  // Add the new data to the rightmost pixel
//  graphData[graphW - 1] = counts_copy;
  graphData[graphW - 1] = fuel_avg1;

#ifndef OPTIMIZED_MAX_SEARCH
  // Find the maximum value in the graph data
  graphMax = 0;
  graphMin = 0; 
  for (int i = 0; i < graphW; i++) {
    if (graphData[i] > graphMax) {
      graphMax = graphData[i];
    }
  }
#endif OPTIMIZED_MAX_SEARCH

}

// Draw rolling graph on OLED display
void drawGraph() {
  // Draw a horizontal line at the bottom of the graph
  //display.drawLine(graphX, graphY + graphH, graphX + graphW , graphY + graphH , SSD1306_WHITE);
  
  // Draw a vertical line at the left of the graph
  //display.drawLine(graphX, graphY, graphX, graphY + graphH - 1, SSD1306_WHITE);
  
  // Draw the graph data as vertical bars
  for (uint8_t i = 0; i < graphW; i++) {
    // Map the data value to the graph height
//       uint8_t barHeight = map(graphData[i], graphMin, graphMax, 0, graphH );
       uint16_t barHeight = map(graphData[i], graphMin, graphMax+1, 0, graphH );

//      uint8_t barHeight = graphH*graphMax graphData[i];

//    uint8_t barHeight= 10;
    // Draw a vertical bar from the bottom to the data value
//    display.drawLine(graphX + i, graphY + graphH , graphX + i, graphY + graphH - barHeight, SSD1306_WHITE);
    //display.drawFastVLine(graphX + i, graphY + graphH , barHeight, SSD1306_WHITE);
    display.drawFastVLine(graphX + i, graphY+(graphH-barHeight) , barHeight, SSD1306_WHITE);
    
  }
}


// This function updates the display with the rolling graph of fuel consumption

void loop() {
  uint32_t currentMillis = millis(); // Get current time in milliseconds
  
  // Update counts per second every interval
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis; // Save current time
    noInterrupts();  //disable interrupts while reading and updating counts
    counts_copy = counts; // take snapshot of counts
    counts = 0;       // Reset counts to zero
    interrupts();     // Enable interrupts again
    
//    fuel_avg1 = (fuel_avg1 * 59.0 + counts_copy * 60.0) / 60.0;  // Calculate fuel consumption per minute using exponential moving average    
    fuel_avg1 = (fuel_avg1 * 5.0 + counts_copy * 6.0) / 6.0;  // Calculate fuel consumption per minute using exponential moving average    
    
//    Serial.print("CPM: "); // Print counts per minute to serial monitor
//    Serial.println(cpm);
//    if (currentMillis - previousMillis_serial >= SERIAL_OUT_INTERVAL) { // Check if interval has passed
//      previousMillis_serial = currentMillis; // Update previous time for serial out
//      Serial.print(currentMillis); // Print timestamp in millis
//      Serial.print(","); // Print comma separator
//      Serial.print(cpm_avg1,3); // Print short term average
//      Serial.print(","); // Print comma separator
//      Serial.println(cpm_avg2,3); // Print long term average
//      }
    
    knobValue = analogRead(KNOB_PIN); // Read value from knob
    knobValue = 300; // hard code for tests
    timeBase = map(knobValue, 0, 920, 1, 200); // Map knob value to time base in miliseconds
    graph_interval = (uint32_t)timeBase  ; // Calculate graph interval based on time base 
    
  if (currentMillis - previousMillis_graph >= graph_interval) {
    previousMillis_graph = currentMillis; // Save current time
                       // Clear the display buffer
    display.clearDisplay();
    updateGraph();     // Update rolling graph with new data    
    drawGraph();       // Draw rolling graph on OLED display
    Serial.println(counts_copy);
  }
      
#ifdef DRAW_TEXT    
    updateDisplay();   // Update OLED display with text  
#endif //DRAW_TEXT
    display.display(); // Show display buffer on screen


 
  } //if 1 second interval
  
/*
    if (pulse_beep){
    tone(BUZZER_PIN, 4000, 2); // beep buzzer
    pulse_beep = false ; // reset pulse_beep 
    display_brightness = 255; 
     for (uint8_t i = 0; i < 8; i++){
      LowPower.idle(SLEEP_15MS, ADC_OFF, TIMER2_ON, TIMER1_OFF, TIMER0_ON, SPI_OFF, USART0_OFF, TWI_OFF);
     }
    }
    
    display_brightness-- ;        // fade the display
    if (display_brightness <=1) {
      display_brightness = 1; // min
    }
    
    display.ssd1306_command(SSD1306_SETCONTRAST);
    display.ssd1306_command(display_brightness);
  
    LowPower.idle(SLEEP_15MS, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_ON, SPI_OFF, USART0_OFF, TWI_OFF);
*/
    
} // loop()
