/*

 Teensy + Feather adapter pinout:
  A6 -> sensorPin
  3 -> switchPin
  4 -> ledPin
 
*/

#include <SPI.h>
#include <Ethernet2.h>
#include <EthernetUdp2.h>
#include <OSCMessage.h>

// ------------------ Set up variables for anemometer ------------------

const int sensorPin = A6;   // Input from anemometer
const int switchPin = 3;    // Override switch
const int ledPin = 4;       // LED for visual feedback

int sensorDelay = 500;     // Delay between sensor readings in ms

int sensorValue = 0;        // Direct value received from anemometer
float sensorVoltage = 0;    // Calculated voltage based on analog reading from anemometer
float windSpeed = 0;        // Wind speed in meters per second (m/s)

// This constant maps the value provided from the analog read function, 
// which ranges from 0 to 1023, to actual voltage, which ranges from 0V to 5V

// float voltageConversionConstant = .004882814; // for arduino uno @ 5v
float voltageConversionConstant = .003225806; // for teensy @ 3.3v

float voltageMin = 0.42;    // Mininum output voltage from anemometer in mV.
float windSpeedMin = 0;     // Wind speed in meters/sec corresponding to minimum voltage

float voltageMax = 2.0;     // Maximum output voltage from anemometer in mV.
float windSpeedMax = 32;    // Wind speed in meters/sec corresponding to maximum voltage

int switchState = 0;        // variable for reading the switch status


// ------------------ Configuration for network messaging ------------------

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

#define OSC_PATH "/sensor/wind"

int packetNum = 0;      // packet number for testing only
float paramValue = 0;

IPAddress lxServer(192,168,1,10);
//IPAddress lxServer(10,1,1,10);  // for testing on a switch
int lxPort = 7878;

// Set the static IP address to use if the DHCP fails to assign
IPAddress ip(192, 168, 1, 11);
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
  Serial.begin(9600);

  pinMode(ledPin, OUTPUT);
  pinMode(switchPin, INPUT);
  pinMode(sensorPin, INPUT_PULLDOWN);

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
  sensorValue = analogRead(sensorPin);                      // value is 0-1023
  sensorVoltage = sensorValue * voltageConversionConstant;  // Convert sensor value to actual voltage
  
  // Convert voltage value to wind speed using range of max 
  // and min voltages and wind speed for the anemometer

  // Check if voltage is below minimum value. If so, set wind speed to zero.
  if (sensorVoltage <= voltageMin){
    windSpeed = 0;
  } else {
    // For voltages above minimum value, use the linear relationship to calculate wind speed.
    windSpeed = (sensorVoltage - voltageMin) * windSpeedMax / (voltageMax - voltageMin); 
  }
   
   // Print voltage and windspeed to serial
    Serial.print("Voltage: ");
    Serial.print(sensorVoltage);
    Serial.print("\t"); 
    Serial.print("Wind speed: ");
    Serial.print(windSpeed); 
    Serial.print( " m/s");
    
    int windSpeedMPH = windSpeed * 2.23693629;
    Serial.print("\t");
    Serial.print(windSpeedMPH);
    Serial.println(" mph");
  
    // read the state of the switch value
    switchState = digitalRead(switchPin);
  
    if (windSpeedMPH > 5 || switchState == HIGH) {
      Serial.println("Wind protection mode.");
      digitalWrite(ledPin, HIGH);
      oscMessage(1);
    }
    else {
      digitalWrite(ledPin, LOW);
      oscMessage(0);
    }
   
   delay(sensorDelay);
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

