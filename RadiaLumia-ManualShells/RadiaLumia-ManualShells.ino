/*
 Teensy + Feather adapter pinout:
  5 -> CLK
  6 -> DOUT1
  9 -> DOUT2
  4 -> DOUT3
  3V -> VCC
  AREF -> VDD
  GND -> GND
 
 The HX711 board can be powered from 2.7V to 5V
*/

#include "HX711.h"
#include <SPI.h>
#include <Ethernet2.h>
#include <EthernetUdp2.h>
#include <OSCMessage.h>

// ------------------ Configuration for the load cell ------------------

//#define DEBUG                       // uncomment when you want to output info to the console

#define NUM_MSHELLS 3               // total number of manual shells

#define MAX_FORCE 10                // max force we expect in lbs (top of the range) 
#define CALIB_FACTOR 10500.0        // Make sure to let it start up with base weight already hanging

#define IDLE_INTERVAL 500           // Delay in ms between standard sensor checks
#define ACTIVE_INTERVAL 100         // Delay in ms between checks during a pull event
#define STAY_ACTIVE 2000            // Delay after last activity before going back to standard checks
#define VARIANCE 0.3               // Difference in values needed to go into active mode

#define PULLED 1
#define NOT_PULLED 0

#define DOUT1  6                    // load cell 1 data input
#define DOUT2  9                    // load cell 2 data input
#define DOUT3  4                    // load cell 3 data input
#define CLK    5

bool activeMode = false;            // Start in idle - true when reading pulls, false otherwise
int activeCounter = 0;              // Counter to know when we've waited long enough to go back to idle
int currentDelay = IDLE_INTERVAL;   // Time in ms between sensor reads

// ------------------ Configuration for network messaging ------------------

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

const char* oscPath[] = {"/sensor/mshell/1/pull", "/sensor/mshell/2/pull", "/sensor/mshell/3/pull"};
float currentState[] = {0, 0, 0};    // hold the current state of load cell values here
HX711 scale[] = { {DOUT1, CLK}, {DOUT2, CLK}, {DOUT3, CLK} };

int packetNum = 0;                    // packet number for testing only
float paramValue = 0;

IPAddress lxServer(192,168,1,10);
int lxPort = 7878;

// Set the static IP address to use if the DHCP fails to assign
IPAddress ip(192, 168, 1, 102);

// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 80 is default for HTTP):
EthernetClient client;

// An EthernetUDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

//#define WIZ_RESET 9

#if defined(ESP8266)
  // default for ESPressif
  #define WIZ_CS 15
#elif defined(ESP32)
  #define WIZ_CS 33
#elif defined(ARDUINO_STM32_FEATHER)
  // default for WICED
  #define WIZ_CS PB4
#elif defined(TEENSYDUINO)
  #define WIZ_CS 10
#elif defined(ARDUINO_FEATHER52)
  #define WIZ_CS 11
#else   // default for 328p, 32u4 and m0
  #define WIZ_CS 10
#endif

// SETUP STARTS HERE

void setup() {
  // ------------------ Setup for load cell ------------------
 
#ifdef DEBUG
  Serial.begin(9600);
  delay(1000);
  Serial.println("Starting up our load cell");
#endif

  for (int i = 0; i < NUM_MSHELLS; i++) {
      scale[i].set_scale(CALIB_FACTOR); // assign calibration factor
      scale[i].tare();  // Assuming there is no weight on the scale at start up, reset the scale to 0
  }
  
#ifdef DEBUG
  Serial.println("Readings:");
#endif

  // ------------------ Setup for networking ------------------
  
#if defined(WIZ_RESET)
  pinMode(WIZ_RESET, OUTPUT);
  digitalWrite(WIZ_RESET, HIGH);
  delay(100);
  digitalWrite(WIZ_RESET, LOW);
  delay(100);
  digitalWrite(WIZ_RESET, HIGH);
#endif

#if !defined(ESP8266) 
//  while (!Serial); // wait for serial port to connect.
#endif

#ifdef DEBUG
  Serial.println("\nHello! I am the Ethernet FeatherWing");
#endif
  
  // start up the ethernet module and pause for it to boot up
  Ethernet.init(WIZ_CS);
  delay(1000);

  // start the Ethernet connection:
  if (Ethernet.begin(mac) == 0) {
#ifdef DEBUG
    Serial.println("Failed to configure Ethernet using DHCP");
#endif
    // no point in carrying on, so do nothing forevermore:
    // try to congifure using IP address instead of DHCP:
    Ethernet.begin(mac, ip);
  }
  Udp.begin(8888);
  
#ifdef DEBUG
  // print the Ethernet board/shield's IP address:
  Serial.print("My IP address: ");
  Serial.println(Ethernet.localIP());

  Serial.print("LX IP address: ");
  Serial.println(lxServer);
  Serial.print("LX port: ");
  Serial.println(lxPort);
  Serial.println();
#endif

  // zero out the sensor cache in LX when we start up
  for (int i = 0; i < NUM_MSHELLS; i++) {
    oscMessage(oscPath[i], NOT_PULLED);
#ifdef DEBUG
    Serial.print("Sent 0 to LX for ");
    Serial.println(oscPath[i]);
#endif
  }
}

// LOOP STARTS HERE

void loop() {
#ifdef DEBUG
  Serial.print("\nReading: ");
  Serial.print(scale[0].get_units(), 1); // scale.get_units() returns a float
  Serial.print(" ");
  Serial.print(scale[1].get_units(), 1);
  Serial.print(" ");
  Serial.print(scale[2].get_units(), 1);
  Serial.print(" lbs"); // You can change this to kg but you'll need to refactor the calibration_factor
  Serial.println();
#endif

  // iterate through sensors sending values to LX
  for (int i = 0; i < NUM_MSHELLS; i++) {     
      float paramValue = map(scale[i].get_units(), 0, MAX_FORCE, 0, 1); // map the force in lbs to the range 0-1

      // check if the values have changed more than VARIANCE or if values are effectively non-zero
      if ((abs(paramValue - currentState[i]) >= VARIANCE) || paramValue > VARIANCE) {
        currentDelay = ACTIVE_INTERVAL;     // shorten delay between reads as we're in a pull event
        activeMode = true;                  // we're now in active mode
        
#ifdef DEBUG
        Serial.print("Values varied more than VARIANCE. New value ");
        Serial.print(paramValue);
        Serial.print(", old value ");
        Serial.println(currentState[i]);
#endif
      } else {
        activeCounter += ACTIVE_INTERVAL;

        // if we haven't detected an active event and it's been longer than the delay, reset to idle
        if (activeCounter >= STAY_ACTIVE) {
          activeMode = false;
          currentDelay = IDLE_INTERVAL;
          activeCounter = 0;
        }
      }

      // send the OSC message
      currentState[i] = paramValue;
      oscMessage(oscPath[i], paramValue);
  }

#ifdef DEBUG
  Serial.print("Delaying for ");
  Serial.print(currentDelay);
  Serial.println(" ms");
#endif
  
  delay(currentDelay);
}

// FUNCTIONS START HERE

//
// Function: oscMessage(const char *, float):
//    Sends the value of the parameter as an OSC message
//    to the address, port, and path specified above  
//
void oscMessage(const char* path, float paramValue) {

    // configure the address and content of the OSC message
    OSCMessage msg(path);
    msg.add(paramValue);

    // create and send the UDP packet
    Udp.beginPacket(lxServer, lxPort);    
    msg.send(Udp);   
    Udp.endPacket();

#ifdef DEBUG
    // debugging content printed to the Serial
    packetNum++;
    Serial.print("sent packet ");
    Serial.print(packetNum);
    Serial.print(" to ");
    Serial.print(path);
    Serial.print(" with value ");
    Serial.println(paramValue);
#endif

    msg.empty(); //free space occupied by message
}


