/*

 Teensy + Feather adapter pinout:
  5 -> CLK
  6 -> DOUT
  3V -> VCC
  AREF -> VDD
  GND -> GND
 
 The HX711 board can be powered from 2.7V to 5V
 
*/

#include "HX711.h"

#define calibration_factor 10500.0  // Make sure to let it start up with base weight already hanging
#define CHECK_INTERVAL 2000         // Delay in ms between sensor checks
#define STAY_OPEN 5000              // Keep the entrance open for 5 seconds after receiving a reading
#define TRIGGER WEIGHT 50           // Min of 50 lbs to trigger the entrance to open

#define DOUT  6
#define CLK  5

HX711 scale(DOUT, CLK);
int stayOpenCounter = 0;            // Keep the entrance open until we hit the "stay open" time

void setup() {
  Serial.begin(9600);
  Serial.println("Starting up our load cell");

  scale.set_scale(calibration_factor); // This value is obtained by using the SparkFun_HX711_Calibration sketch
  scale.tare();	// Assuming there is no weight on the scale at start up, reset the scale to 0

  Serial.println("Readings:");
}

void loop() {
  Serial.print("Reading: ");
  Serial.print(scale.get_units(), 1); // scale.get_units() returns a float
  Serial.print(" lbs"); // You can change this to kg but you'll need to refactor the calibration_factor
  Serial.println();

  if (stayOpenCounter < STAY_OPEN) {
    // keep sending an OSC message to LX to keep the entrance open
    stayOpenCounter += CHECK_INTERVAL;
  } else {
    // send an OSC message to close the entrance
    stayOpenCounter = 0;
  }
  
  delay(CHECK_INTERVAL);  // Check to see if anyone is on the ladder every 2 seconds
}
