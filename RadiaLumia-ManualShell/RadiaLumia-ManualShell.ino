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
#include <SPI.h>
#include <Ethernet2.h>
#include <EthernetUdp2.h>
#include <OSCMessage.h>

// ------------------ Configuration for the load cell ------------------

#define calibration_factor 10500.0  // Make sure to let it start up with base weight already hanging
#define CHECK_INTERVAL 2000         // Delay in ms between sensor checks

#define DOUT  6
#define CLK  5

HX711 scale(DOUT, CLK);

// ------------------ Configuration for network messaging ------------------

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

// TODO -- Update this path to the right one for the manual shells
#define OSC_PATH "/lx/master/effect/2/wind"

int packetNum = 0;      // packet number for testing only
float paramValue = 0;

IPAddress lxServer(192,168,42,65);
//IPAddress lxServer(10,1,1,10);  // for testing on a switch
int lxPort = 3030;

// Set the static IP address to use if the DHCP fails to assign
IPAddress ip(192, 168, 42, 200);
//IPAddress ip(10, 1, 1, 20);    // for testing on a switch

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
  
  Serial.begin(9600);
  Serial.println("Starting up our load cell");

  scale.set_scale(calibration_factor); // This value is obtained by using the SparkFun_HX711_Calibration sketch
  scale.tare();	// Assuming there is no weight on the scale at start up, reset the scale to 0

  Serial.println("Readings:");

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
  while (!Serial); // wait for serial port to connect.
#endif

  // Open serial communications and wait for port to open:
  Serial.begin(115200);
  delay(1000);
  Serial.println("\nHello! I am the Ethernet FeatherWing");

  Ethernet.init(WIZ_CS);
  
  // give the ethernet module time to boot up:
  delay(1000);

  // start the Ethernet connection:
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // no point in carrying on, so do nothing forevermore:
    // try to congifure using IP address instead of DHCP:
    Ethernet.begin(mac, ip);
  }
  Udp.begin(8888);
  
  // print the Ethernet board/shield's IP address:
  Serial.print("My IP address: ");
  Serial.println(Ethernet.localIP());

  Serial.print("LX IP address: ");
  Serial.println(lxServer);
  Serial.print("LX port: ");
  Serial.println(lxPort);
}

// LOOP STARTS HERE

void loop() {
  Serial.print("Reading: ");
  Serial.print(scale.get_units(), 1); // scale.get_units() returns a float
  Serial.print(" lbs"); // You can change this to kg but you'll need to refactor the calibration_factor
  Serial.println();

//  float paramValue = map((float)random(50), 0.0, 50.0, 0.0, 1.0); // use 0-50 lbs for testing
  float paramValue = map(scale.get_units(), 0, 50, 0, 1); // map the force in lbs to the range 0-1
  oscMessage(paramValue);

  delay(CHECK_INTERVAL);  // Check to see if anyone is on the ladder every 2 seconds
}

// FUNCTIONS START HERE

void oscMessage(float paramValue) {

    // configure the address and content of the OSC message
    OSCMessage msg(OSC_PATH);
    msg.add(paramValue);

    // create and send the UDP packet
    Udp.beginPacket(lxServer, lxPort);    
    msg.send(Udp);   
    Udp.endPacket();

    // debugging content printed to the Serial
    packetNum++;
    Serial.print("sent packet ");
    Serial.print(packetNum);
    Serial.print(" with value ");
    Serial.println(paramValue);
    
    msg.empty(); //free space occupied by message
}

