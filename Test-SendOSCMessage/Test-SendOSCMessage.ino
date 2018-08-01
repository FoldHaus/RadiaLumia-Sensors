#include <SPI.h>
#include <Ethernet2.h>
#include <EthernetUdp2.h>
#include <OSCMessage.h>

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };

//#define OSC_PATH "/lx/master/effect/2/wind"
#define OSC_PATH "/sensor/ladder/weighted"

int packetNum = 0;      // packet number for testing only
float paramValue = 0;

//IPAddress lxServer(192,168,42,65);
IPAddress lxServer(192,168,1,10);  // for testing on a switch
int lxPort = 7878;

// Set the static IP address to use if the DHCP fails to assign
//IPAddress ip(192, 168, 42, 200);
IPAddress ip(192, 168, 1, 11);    // for testing on a switch

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

void setup() {
#if defined(WIZ_RESET)
  pinMode(WIZ_RESET, OUTPUT);
  digitalWrite(WIZ_RESET, HIGH);
  delay(100);
  digitalWrite(WIZ_RESET, LOW);
  delay(100);
  digitalWrite(WIZ_RESET, HIGH);
#endif

//#if !defined(ESP8266) 
//  while (!Serial); // wait for serial port to connect.
//#endif

  // Open serial communications and wait for port to open:
  Serial.begin(9600);
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

void loop() {
  oscMessage();     // send the OSC message
  delay(500);      // send one message every 5 secs
}

void oscMessage() {

    // configure the address and content of the OSC message
    OSCMessage msg(OSC_PATH);
    paramValue = (float)random(100) / 100;    // randomly generate a number between 0 and 1
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

