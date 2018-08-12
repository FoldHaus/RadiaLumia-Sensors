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

// ------------------ Configuration for the load cells ------------------

//#define DEBUG                       // uncomment when you want to output info to the console

#define calibration_factor 10500.0  // Make sure to let it start up with base weight already hanging
#define CHECK_INTERVAL 500          // Delay in ms between sensor checks
#define STAY_OPEN 10000             // Keep the entrance open for 10 seconds after receiving a reading
#define TRIGGER_WEIGHT 5.0          // Min of 25 lbs to trigger the entrance to open

#define OPEN 1
#define CLOSED 0

#define DOUT1  6                    // load cell 1 data input
#define DOUT2  9                    // load cell 2 data input
#define DOUT3  4                    // load cell 3 data input
#define CLK    5

HX711 scale1(DOUT1, CLK);
HX711 scale2(DOUT2, CLK);
HX711 scale3(DOUT3, CLK);

int stayOpenCounter = 0;            // Keep the entrance open until we hit the "stay open" time
boolean entranceOpen = 0;           // 0 for closed; 1 for open

// ------------------ Configuration for network messaging ------------------

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

#define OSC_PATH "/sensor/ladder/weighted"

int packetNum = 0;      // packet number for testing only
float paramValue = 0;

IPAddress lxServer(192,168,1,10);
int lxPort = 7878;

// Set the static IP address to use if the DHCP fails to assign
IPAddress ip(192,168,1,101);

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
  // ------------------ Setup for load cells ------------------

#ifdef DEBUG
  Serial.begin(9600); 
  delay(1000); 
  Serial.println("Starting up our three load cells");
#endif

  scale1.set_scale(calibration_factor); // assign calibration factor
  scale1.tare();  // Assuming there is no weight on the scale at start up, reset the scale to 0

  scale2.set_scale(calibration_factor);
  scale2.tare();

  scale3.set_scale(calibration_factor);
  scale3.tare();

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

  // start the ethernet module and give it time to boot up
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

  oscMessage(CLOSED);  // zero the ladder sensor with LX when we start up
#ifdef DEBUG
  Serial.println("Sent 0 to LX to get it started");
#endif
}

// LOOP STARTS HERE

void loop() {
#ifdef DEBUG
  Serial.print("\nReading: ");
  Serial.print(scale1.get_units(), 1); // scale.get_units() returns a float
  Serial.print(" ");
  Serial.print(scale2.get_units(), 1);
  Serial.print(" ");
  Serial.print(scale3.get_units(), 1);
  Serial.print(" lbs"); // You can change this to kg but you'll need to refactor the calibration_factor
  Serial.println();
#endif

  // trigger the sensor team if at least one of the load cells is over weight
  int triggered = 0;
  triggered = (scale1.get_units() > TRIGGER_WEIGHT) || 
              (scale2.get_units() > TRIGGER_WEIGHT) || 
              (scale3.get_units() > TRIGGER_WEIGHT);
  
  // open the entrance whenever the load cells are triggered
  if (triggered) {
    setNewState(OPEN);
    stayOpenCounter = 0;
#ifdef DEBUG
    Serial.println("triggered, so open");
#endif
  }

  // if it's already open and it's been longer than the delay, close the entrance
  if (entranceOpen && (stayOpenCounter >= STAY_OPEN)) {
    setNewState(CLOSED);
    stayOpenCounter = 0;
#ifdef DEBUG
    Serial.println("reset to closed");
#endif
  }
  // otherwise stay open and increment the counter
  else if (entranceOpen && (stayOpenCounter < STAY_OPEN)) {
    setNewState(OPEN);
    stayOpenCounter += CHECK_INTERVAL;
#ifdef DEBUG
    Serial.println("stay open and increment counter");
#endif
  } else {
    setNewState(CLOSED);
  }
  delay(CHECK_INTERVAL);  // Check to see if anyone is on the ladder every 2 seconds
}

// FUNCTIONS START HERE

//
// Function: setNewState(int):
//    Only send an updated value to LX when the state has changed,
//    minimizing the network traffic generated by these sensors  
//
void setNewState(int newState) {
  if (entranceOpen != newState) {
    entranceOpen = newState;
#ifdef DEBUG
    Serial.print("setting entranceOpen to ");
    Serial.println(entranceOpen);
#endif
    // oscMessage(newState);      // used when we were only sending a message at state change
  }
  oscMessage(newState);           // send a message with the current state on every loop
}

//
// Function: oscMessage(float):
//    Sends the value of the parameter as an OSC message
//    to the address, port, and path specified above  
//
void oscMessage(float paramValue) {

    // configure the address and content of the OSC message
    OSCMessage msg(OSC_PATH);
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
    Serial.print(" with value ");
    Serial.println(paramValue);
#endif
    
    msg.empty(); //free space occupied by message
}

