
//Setup Variables

const int sensorPin = A0; //Defines the pin that the anemometer output is connected to
const int switchPin = 3;     // the number of the switch pin
const int ledPin = 13;      // number of the onboard LED pin

int sensorValue = 0; //Variable stores the value direct from the analog pin
float sensorVoltage = 0; //Variable that stores the voltage (in Volts) from the anemometer being sent to the analog pin
float windSpeed = 0; // Wind speed in meters per second (m/s)

// This constant maps the value provided from the analog read function, 
// which ranges from 0 to 1023, to actual voltage, which ranges from 0V to 5V
// float voltageConversionConstant = .004882814; // for arduino uno @ 5v
float voltageConversionConstant = .003225806; // for teensy @ 3.3v

int sensorDelay = 1000; //Delay between sensor readings, measured in milliseconds (ms)

//Anemometer Technical Variables
//The following variables correspond to the anemometer sold by Adafruit, but could be modified to fit other anemometers.

float voltageMin = 0.42; // Mininum output voltage from anemometer in mV.
float windSpeedMin = 0; // Wind speed in meters/sec corresponding to minimum voltage

float voltageMax = 2.0; // Maximum output voltage from anemometer in mV.
float windSpeedMax = 32; // Wind speed in meters/sec corresponding to maximum voltage

int switchState = 0;    // variable for reading the switch status

void setup() 
{ 
  pinMode(ledPin, OUTPUT);
  pinMode(switchPin, INPUT);             
  Serial.begin(9600);  //Start the serial connection
}


void loop() 
{
sensorValue = analogRead(sensorPin); //Get a value between 0 and 1023 from the analog pin connected to the anemometer
// Serial.println(sensorValue);

sensorVoltage = sensorValue * voltageConversionConstant; //Convert sensor value to actual voltage

//Convert voltage value to wind speed using range of max and min voltages and wind speed for the anemometer
if (sensorVoltage <= voltageMin){
  windSpeed = 0; //Check if voltage is below minimum value. If so, set wind speed to zero.
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

  // read the state of the pushbutton value:
  switchState = digitalRead(switchPin);

  if (windSpeedMPH > 10 || switchState == HIGH) {
    Serial.println("Wind protection mode.");
    digitalWrite(ledPin, HIGH);
  }
  else {
    digitalWrite(ledPin, LOW);
  }
 
 delay(sensorDelay);
}
