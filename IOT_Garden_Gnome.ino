#include <ArduinoJson.h>

#include <OneWire.h>
#include <DallasTemperature.h>

#include <Arduino.h>
#include <Stream.h>
#include <SPI.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

extern "C" {
#include "user_interface.h"
}
//fill in your own keys and wifi
//  --------- Config ---------- //
#include "./creds.h"

//# of connections
long connection = 0;

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
const int capacity = JSON_OBJECT_SIZE(3);
DynamicJsonDocument gnomeHoseDoc(capacity);
//send a message to a firebase
String postsensormessage (String aT, String sT, String sM, String sL, String w ) {
  //
  //String values = "{\"state\":{\"reported\":{\"Air_temp\":" + aT + ", \"Soil_temp\":" + sT + ", \"Sunlight\": " + sL + ", \"soil_moisture\": " + sM + ",\"Watered\": " + w + "}}}";
  return "{\"user\": \""+King_Kyrie_Key+"\",\"gnome\": \"gnome1\", \"location\": {\"lat\": 45, \"lng\": 73}, \"light\": "+sL+", \"temperature\": "+aT+", \"soil_humidity\": "+sM +"}";
}

String postgnomehoseoff(){
  return "{\"user\": \""+King_Kyrie_Key+"\",\"gnome\": \"gnome1\",\"hose\": false, \"water_time\": 1}";
}
String getgnomehose(){
  return "{\"user\": \""+King_Kyrie_Key+"\",\"gnome\": \"gnome1\"}";
}
const char* postmessage () {
  //Post a message
  return "{\"user\": \"6HyXNaKq1uWWHqOz1LooNRpL9eK2\",\"gnome\": \"gnome1\", \"location\": {\"lat\": 45, \"lng\": 73}, \"light\": 100, \"temperature\": -6, \"soil_humidity\": 50}";
}






void error(char *str){
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
  Serial.begin (115200);
  delay (2000);
  Serial.setDebugOutput(1);


  //fill with ssid and wifi password
  WiFi.begin(wifi_ssid, wifi_password);
  Serial.println ("connecting to wifi");
  while (WiFi.status() != WL_CONNECTED) {  //Wait for the WiFI connection completion
 
    delay(500);
    Serial.println("Waiting for connection");
 
  }
  Serial.println ("\nconnected to network " + String(wifi_ssid) + "\n");


  


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
//  if (((soilMoisture < wateringThreshold)||watering)) {
//    //water the garden
//    digitalWrite(solenoidPin, HIGH);
//    delay(wateringTime);
//    digitalWrite(solenoidPin, LOW);
//
//    //Serial.print("TRUE");
//
//    wateredToday = true;
//  }  else {
//    //Serial.print("FALSE");
//  }

if(gnomeHoseDoc["hose"]){
  digitalWrite(solenoidPin, HIGH);
  delay(gnomeHoseDoc["water_time"].as<int>()*60000);
  digitalWrite(solenoidPin, LOW);
}


  //Serial.println("////////////////////////////////////////////////////////////");

  if(WiFi.status()== WL_CONNECTED){   //Check WiFi connection status
 
    WiFiClient client;
    HTTPClient http;    //Declare object of class HTTPClient
    
    http.begin("http://limitless-forest-10226.herokuapp.com");      //Specify request destination
    http.addHeader("Content-Type", "application/json");  //Specify content-type header
    
    int httpCode = http.POST(postsensormessage(String(airTemp), String(soilTemp), String(soilMoisture), String(sunlight), String(wateredToday)));   //Send the request
    String payload = http.getString();                  //Get the response payload
    
    Serial.println("http Code : " + String(httpCode));   //Print HTTP return code
    Serial.println("payload : " + payload);    //Print request response payload

    http.end();  //Close connection

    
    if(gnomeHoseDoc["hose"]){
      Serial.print("gnome hose " );
      Serial.println(gnomeHoseDoc["hose"].as<char*>());
      http.begin("http://limitless-forest-10226.herokuapp.com/hose");      //Specify request destination
      http.addHeader("Content-Type", "application/json");  //Specify content-type header
      
      int httpCode = http.POST(postgnomehoseoff());   //Send the request
      String payload = http.getString();                  //Get the response payload
      
      Serial.println("http Code : " + String(httpCode));   //Print HTTP return code
      Serial.println("payload : " + payload);    //Print request response payload
  
      http.end();  //Close connection
    }

    
    http.begin("http://limitless-forest-10226.herokuapp.com/hose?user="+King_Kyrie_Key+"&gnome=gnome1");      //Specify request destination
    http.addHeader("Content-Type", "application/json");  //Specify content-type header
    
    httpCode = http.GET();   //Send the request
    payload = http.getString();                  //Get the response payload
    
    Serial.println("http Code : " + String(httpCode));   //Print HTTP return code
    Serial.println("payload : " + payload);    //Print request response payload
    
    http.end();  //Close connection
    DeserializationError err = deserializeJson(gnomeHoseDoc, payload);
    if (err) {
      Serial.print(F("deserializeJson() failed with code "));
      Serial.println(err.c_str());
    }


  }else{
  
    Serial.println("Error in WiFi connection");   
  
  }


  delay(10000);  //Send a request every 10 seconds
 
    
    
    
  //watering = doc["state"]["Watered"];
  //String output;
  //serializeJson(doc, output);
  Serial.println(watering);
  digitalWrite(D3, watering);

  Serial.println("//////////////////////////////////////////////////////////");
}
