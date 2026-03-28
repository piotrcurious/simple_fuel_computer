
// include the library code:
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// define the display size
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// initialize the display with pins A4 and A5
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// define some constants
#define CAM_PIN 2 // cam position sensor pin
#define INJ_PIN 3 // fuel injector signal pin
#define PULSE_PER_REV 36 // number of pulses per camshaft revolution
#define INJ_FLOW_RATE 200.0 // fuel injector flow rate in ml/min
#define FUEL_DENSITY 0.75 // fuel density in g/ml
#define GRAPH_HEIGHT 16 // height of the graph area in pixels
#define GRAPH_WIDTH 128 // width of the graph area in pixels

// define some variables
volatile unsigned long cam_pulse_count = 0; // count of cam pulses
volatile unsigned long inj_pulse_count = 0; // count of injector pulses
volatile unsigned long inj_pulse_width = 0; // width of injector pulse in microseconds
volatile unsigned long last_cam_time = 0; // last time a cam pulse was detected in microseconds
volatile unsigned long last_inj_time = 0; // last time an injector pulse was detected in microseconds
float rpm = 0; // engine speed in revolutions per minute
float inj_duty_cycle = 0; // injector duty cycle in percentage
float fuel_consumption = 0; // fuel consumption in g/s
float graph_data[GRAPH_WIDTH]; // array to store the graph data

unsigned long last_rev_time = 0;
// interrupt service routine for cam pulse detection
void camISR() {
  unsigned long current_time = micros(); // get the current time in microseconds
  cam_pulse_count++; // increment the cam pulse count
  if (cam_pulse_count == PULSE_PER_REV) { // if one revolution is completed
    unsigned long rev_duration = current_time - last_rev_time;
    if (rev_duration > 0) {
      rpm = 60000000.0 / rev_duration; // calculate the rpm
    }
    cam_pulse_count = 0; // reset the cam pulse count
    last_rev_time = current_time;
  }
}

// interrupt service routine for injector pulse detection
void injISR() {
  unsigned long current_time = micros(); // get the current time in microseconds
  if (digitalRead(INJ_PIN) == HIGH) { // if injector signal is high
    last_inj_time = current_time; // update the last inj time
    inj_pulse_count++; // increment the inj pulse count
  } else { // if injector signal is low
    inj_pulse_width += current_time - last_inj_time; // add the width of the inj pulse to the total width
  }
}

unsigned long last_calc_time = 0;
// function to calculate the fuel consumption based on injector pulse width and rpm
void calculateFuelConsumption() {
  unsigned long current_time = micros();
  unsigned long duration = current_time - last_calc_time;
  if (duration == 0) return;

  // Set RPM to 0 if no pulses for more than 2 seconds
  if (current_time - last_rev_time > 2000000) {
    rpm = 0;
  }

  if (rpm > 0) { // if engine is running
    inj_duty_cycle = (inj_pulse_width * 100.0) / duration; // calculate the inj duty cycle in percentage
    // ml/sec = (inj_pulse_width / duration) * (INJ_FLOW_RATE / 60.0)
    // g/sec = ml/sec * FUEL_DENSITY
    fuel_consumption = (inj_pulse_width / (float)duration) * (INJ_FLOW_RATE / 60.0) * FUEL_DENSITY;
  } else { // if engine is not running
    inj_duty_cycle = 0; // set inj duty cycle to zero
    fuel_consumption = 0; // set fuel consumption to zero
  }
  inj_pulse_width = 0;
  last_calc_time = current_time;
}

// function to update the graph data array with the latest fuel consumption value and shift the previous values left by one pixel 
void updateGraphData() {
  for (int i = 0; i < GRAPH_WIDTH - 1; i++) { // for each pixel except the last one 
    graph_data[i] = graph_data[i + 1]; // shift the value left by one pixel 
  }
  graph_data[GRAPH_WIDTH - 1] = fuel_consumption; // set the last pixel value to the latest fuel consumption value 
}

// function to draw the graph on the display 
void drawGraph() {
  float max_value = graph_data[0]; // initialize max value with first value 
  for (int i = 1; i < GRAPH_WIDTH; i++) { // for each pixel except the first one 
    if (graph_data[i] > max_value) { // if value is greater than max value 
      max_value = graph_data[i]; // update max value 
    }
  }
  
  for (int i = GRAPH_WIDTH -1 ; i >=0 ; i--) { 
    int x = i;
    int y = map(graph_data[i],0,max_value,SCREEN_HEIGHT-1,SCREEN_HEIGHT-GRAPH_HEIGHT); 
    display.drawPixel(x,y,SSD1306_WHITE); 
    display.drawLine(x,y,x,SCREEN_HEIGHT-1,SSD1306_WHITE); 
   }
}

void setup() {
  
   Serial.begin(9600);
   
   pinMode(CAM_PIN, INPUT); 
   pinMode(INJ_PIN, INPUT); 
  
   attachInterrupt(digitalPinToInterrupt(CAM_PIN), camISR, RISING); 
   attachInterrupt(digitalPinToInterrupt(INJ_PIN), injISR, CHANGE); 
  
   if(!display.begin(SSD1306_SWITCHCAPVCC,0x3C)) { 
     Serial.println(F("SSD1306 allocation failed"));
     for(;;);
   }
   
   display.clearDisplay(); 
   display.setTextSize(1); 
   display.setTextColor(SSD1306_WHITE); 

}

unsigned long last_loop_time = 0;
void loop() {
   if (millis() - last_loop_time < 1000) return;
   last_loop_time = millis();

   noInterrupts(); 
   calculateFuelConsumption(); 
   updateGraphData(); 
   interrupts(); 
  
   Serial.print("RPM: "); Serial.print(rpm);
   Serial.print(" INJ: "); Serial.print(inj_duty_cycle);
   Serial.print("% Fuel: "); Serial.println(fuel_consumption);

   display.clearDisplay(); 
  
   display.setCursor(0,0); 
   display.print("RPM: "); 
   display.print(rpm); 
  
   display.setCursor(64,0); 
   display.print("INJ: "); 
   display.print(inj_duty_cycle); 
   display.print("%"); 
  
   drawGraph(); 
  
   display.display(); 
  
}


//Source: Conversation with Bing, 5/7/2023
//(1) fuel consumption - How does a real-time mpg display work? - Motor .... https://mechanics.stackexchange.com/questions/36285/how-does-a-real-time-mpg-display-work.
//(2) arduino based fully DIY fuel injection ECU.. https://forum.arduino.cc/t/arduino-based-fully-diy-fuel-injection-ecu/208115.
//(3) Vehicle Fuel Consumption Prediction Method Based on Driving ... - Hindawi. https://www.hindawi.com/journals/jat/2020/9263605/.
