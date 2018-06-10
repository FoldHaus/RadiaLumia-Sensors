/*
Receives and visualizes OSCBundles sent over UDP

Use with /examples/UDPSendMessage

or with /examples/SerialSendMessage in conjunction 
with /Applicaitons/Processing/SLIPSerialToUDP
*/

import oscP5.*;
import netP5.*;
  
OscP5 oscP5;

void setup() {
  size(150,300);
  frameRate(30);
  //set this to the receiving port
  oscP5 = new OscP5(this,3030);
}


void draw() {
  background(0); 
  //draw the analog values
  float windHeight = map(windValue, 0, 1, 0, 200);
  fill(255);
  rect(50, 250, 50, -windHeight);
  //and the labels
  textSize(12);
  text("/lx/channel/4/pattern/2/wind", 50, 270);
}

float windValue = 0.3;

// incoming osc message are forwarded to the oscEvent method. 
void oscEvent(OscMessage theOscMessage) {
  //println(theOscMessage.addrPattern());
  if (theOscMessage.addrPattern().equals("/lx/channel/4/pattern/2/wind")){
    windValue = theOscMessage.get(0).floatValue();
    //println(windValue);
    text(windValue, 50, 300);
  } 
}