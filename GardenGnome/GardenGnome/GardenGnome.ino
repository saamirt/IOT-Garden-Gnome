#include <OneWire.h>
#include <DallasTemperature.h>

#include <Arduino.h>
#include <Stream.h>


#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>


//AWS
#include "sha256.h"
#include "Utils.h"


//WEBSockets
#include <Hash.h>
#include <WebSocketsClient.h>


//MQTT PUBSUBCLIENT LIB
#include <PubSubClient.h>


//AWS MQTT Websocket
#include "Client.h"
#include "AWSWebSocketClient.h"
#include "CircularByteBuffer.h"




extern "C" {
#include "user_interface.h"
}
//fill in your own keys and wifi
//  --------- Config ---------- //
//AWS IOT config, change these:
char wifi_ssid[]       = "your-ssid";
char wifi_password[]   = "your-password";
char aws_endpoint[]    = "your-endpoint.iot.eu-west-1.amazonaws.com";
char aws_key[]         = "your-iam-key";
char aws_secret[]      = "your-iam-secret-key";
char aws_region[]      = "eu-west-1";

int port = 443;


//MQTT config
const int maxMQTTpackageSize = 1024;
const int maxMQTTMessageHandlers = 1;


ESP8266WiFiMulti WiFiMulti;


AWSWebSocketClient awsWSclient(1000);


PubSubClient client(awsWSclient);


//# of connections
long connection = 0;


//generate random mqtt clientID
char* generateClientID () {
  char* cID = new char[23]();
  for (int i = 0; i < 22; i += 1)
    cID[i] = (char)random(1, 256);
  return cID;
}


//count messages arrived
int arrivedcount = 0;


//callback to handle mqtt messages
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}


//connects to websocket layer and mqtt layer
bool connect () {
  if (client.connected()) {
    client.disconnect ();
  }
  //delay is not necessary... it just help us to get a "trustful" heap space value
  delay (1000);
  Serial.print (millis ());
  Serial.print (" - conn: ");
  Serial.print (++connection);
  Serial.print (" - (");
  Serial.print (ESP.getFreeHeap ());
  Serial.println (")");

  //creating random client id
  char* clientID = generateClientID ();

  client.setServer(aws_endpoint, port);
  if (client.connect(clientID)) {
    Serial.println("connected");
    return true;
  } else {
    Serial.print("failed, rc=");
    Serial.print(client.state());
    return false;
  }
  Serial.println("-------------------------");
}


//subscribe to a mqtt topic
void subscribe () {
  client.setCallback(callback);
  client.subscribe(aws_topic);
  //client.subscribe(aws_topic2);
  //subscript to a topic
  Serial.println("MQTT subscribed");
}


//send a message to a mqtt topic
void sendsensormessage (String aT, String sT, String sM, String sL, String w ) {
  //
  String values = "{\"state\":{\"reported\":{\"Air_temp\":" + aT + ", \"Soil_temp\":" + sT + ", \"Sunlight\": " + sL + ", \"soil_moisture\": " + sT + ",\"Watered\": " + w + "}}}";
  const char *publish_message = values.c_str();
  //send a message
  char buf[200];
  strcpy(buf, publish_message);
  int rc = client.publish(aws_topic, buf);
  if (rc) {
    Serial.println("Publish succeeded");
  } else {
    Serial.println("Publish failed");
    Serial.println(values);
  }
}


void sendmessage () {
  //send a message
  char buf[100];
  strcpy(buf, "{\"state\":{\"reported\":{\"temp\": 12.3, \"light\": 456, \"soil_moisture\": 789}}}");
  int rc = client.publish(aws_topic, buf);
  //int rc2 = client.publish(aws_topic2, buf);
}

// Data wire is plugged into pin 4 on the Arduino
#define ONE_WIRE_BUS D4
/********************************************************************/
// Setup a oneWire instance to communicate with any OneWire devices
// (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
/********************************************************************/
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);
/********************************************************************/

////////We need some real time clock to keep system up to date///////////


const int soilMoisturePin = D5;
const int sunlightPin = D7;
const int LEDPinGreen = D1;
const int LEDPinRed = D0;
const int solenoidPin = D3;
const int wateringTime = 600000; //Set the watering time (10 min for a start)
const float wateringThreshold = 15; //Value below which the garden gets watered

float soilTemp = 0; //Scaled value of soil temp (degrees F)
float soilMoistureRaw = 0; //Raw analog input of soil moisture sensor (volts)
float soilMoisture = 0; //Scaled value of volumetric water content in soil (percent)
float airTemp = 0; //Air temp (degrees C)
float heatIndex = 0; //Heat index (degrees C)
float sunlight = 0; //Sunlight illumination in lux
float analogValue = 0;
bool watering = false;
bool wateredToday = false;
/*
  Soil Moisture Reference
  Air = 0%
  Really dry soil = 10%
  Probably as low as you'd want = 20%
  Well watered = 50%
  Cup of water = 100%
*/



void error(char *str)
{
  Serial.print("error: ");
  Serial.println(str);

  // red LED indicates error
  digitalWrite(LEDPinRed, HIGH);

  while (1);
}
float analogSensorRead(int PIN) {
  digitalWrite(PIN, HIGH);
  delay(10);
  analogValue = analogRead(A0);
  digitalWrite(PIN, LOW);
  return analogValue;
}
void print2digits(int number) {
  if (number < 10) {
    Serial.print("0"); // print a 0 before if the number is < than 10
  }
  Serial.print(number);
}

void setup() {
  wifi_set_sleep_type(NONE_SLEEP_T);
  Serial.begin (115200);
  delay (2000);
  Serial.setDebugOutput(1);


  //fill with ssid and wifi password
  WiFiMulti.addAP(wifi_ssid, wifi_password);
  Serial.println ("connecting to wifi");
  while (WiFiMulti.run() != WL_CONNECTED) {
    delay(100);
    Serial.print (".");
  }
  Serial.println ("\nconnected to network " + String(wifi_ssid) + "\n");


  //fill AWS parameters
  awsWSclient.setAWSRegion(aws_region);
  awsWSclient.setAWSDomain(aws_endpoint);
  awsWSclient.setAWSKeyID(aws_key);
  awsWSclient.setAWSSecretKey(aws_secret);
  awsWSclient.setUseSSL(true);

  if (connect ()) {
    subscribe ();
    sendmessage ();
  }


  pinMode(D8, OUTPUT); //LED green pint
  pinMode(LEDPinRed, OUTPUT); //LED red pin
  pinMode(soilMoisturePin, OUTPUT); //LED green pint
  pinMode(sunlightPin, OUTPUT); //LED red pin
  pinMode(solenoidPin, OUTPUT); //solenoid pin
  digitalWrite(solenoidPin, LOW); //Make sure the valve is off
  pinMode(A0, INPUT);




  sensors.begin();
  Serial.println("--------------------");
  Serial.println("////////////////////////");
  Serial.println("--------------------");

}

void loop() {
  //Three blinks means start of new cycle
  for (int i = 0; i < 3; i++) {
    digitalWrite(D8, HIGH);
    delay(200);
    digitalWrite(D8, LOW);
    delay(200);
  }
  //Reset wateredToday variable if it's a new day
  //  if (!(now.day() == rtc.now().day())) {
  //    wateredToday = false;
  //  }


  //Collect Variables
  sensors.requestTemperatures(); // Send the command to get temperature readings
  soilTemp = sensors.getTempCByIndex(0);
  airTemp = sensors.getTempCByIndex(1);
  delay(20);


  //This is a rough conversion that I tried to calibrate using a flashlight of a "known" brightness
  sunlight = pow(((((150 * 3.3) / (analogSensorRead(sunlightPin) * (3.3 / 1024))) - 150) / 70000), -1.25);
  delay(20);

  soilMoistureRaw = analogSensorRead(soilMoisturePin) * (3.3 / 1024);

  //Volumetric Water Content is a piecewise function of the voltage from the sensor
  if (soilMoistureRaw < 1.1) {
    soilMoisture = (10 * soilMoistureRaw) - 1;
  }
  else if (soilMoistureRaw < 1.3) {
    soilMoisture = (25 * soilMoistureRaw) - 17.5;
  }
  else if (soilMoistureRaw < 1.82) {
    soilMoisture = (48.08 * soilMoistureRaw) - 47.5;
  }
  else if (soilMoistureRaw < 2.2) {
    soilMoisture = (26.32 * soilMoistureRaw) - 7.89;
  }
  else {
    soilMoisture = (62.5 * soilMoistureRaw) - 87.5;
  }
  ///////////For Testing Purpose/////////////////////////////
//  Serial.print("soilTemp: ");
//  Serial.print(soilTemp);
//  Serial.print(", airtemp: ");
//  Serial.print(airTemp);
//  Serial.print(", soilMoisture: ");
//  Serial.print(soilMoisture);
//  Serial.print(", sunlight: ");
//  Serial.print(sunlight);
//  Serial.print(", Watered: ");
//  Serial.println(wateredToday);
//  Serial.println();


  //Water the plants//////////////////////////
  if ((soilMoisture < wateringThreshold) && (wateredToday = false)) {
    //water the garden
    digitalWrite(solenoidPin, HIGH);
    delay(wateringTime);
    digitalWrite(solenoidPin, LOW);

    //Serial.print("TRUE");

    wateredToday = true;
  }  else {
    //Serial.print("FALSE");
  }
  delay(5000);


  //Serial.println("////////////////////////////////////////////////////////////");

  //Update Shadow////////////////////////
  //keep the mqtt up and running
  if (awsWSclient.connected ()) {
    client.loop();
    subscribe ();
    //publish
    //sendmessage();
    sendsensormessage(String(airTemp), String(soilTemp), String(soilMoisture), String(sunlight), String(wateredToday) );
    delay(5000);
  } else {
    //handle reconnection
    connect ();
  }

  Serial.println("//////////////////////////////////////////////////////////");
}
