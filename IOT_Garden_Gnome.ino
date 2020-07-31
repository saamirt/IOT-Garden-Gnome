#include <FS.h>
#include <ArduinoJson.h>

#include <OneWire.h>
#include <DallasTemperature.h>

#include <Arduino.h>
#include <Stream.h>
#include <SPI.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         // https://github.com/tzapu/WiFiManager

extern "C" {
#include "user_interface.h"
}
//fill in your own keys and wifi
//  --------- Config ---------- //
#include "./creds.h"

// Set web server port number to 88
WiFiServer server(88);

/********************************************************************/
//Pin definitions
const int soilMoisturePin = D5;
const int sunlightPin = D7;
const int LEDPinGreen = D1;
const int LEDPinRed = D0;
const int solenoidPin = D3;
const int wateringTime = 600000; //Set the watering time (10 min for a start)
const float wateringThreshold = 15; //Value below which the garden gets watered
/********************************************************************/
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
/********************************************************************/
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
//Define Size of json documents
const int gnomeHoseDocCapacity = JSON_OBJECT_SIZE(10);
const int gnomeSensorDataDocCapacity = JSON_OBJECT_SIZE(14);
//Create json documents
DynamicJsonDocument gnomeHoseDoc(gnomeHoseDocCapacity);
DynamicJsonDocument gnomeSensorDataDoc(gnomeSensorDataDocCapacity);
/********************************************************************/
//flag for saving data
bool shouldSaveConfig = false;
/********************************************************************/


//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
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

String postRequest(HTTPClient &http, String endpoint, String msg ){

    http.begin(endpoint);      //Specify request destination
    http.addHeader("Content-Type", "application/json");  //Specify content-type header

    int httpCode = http.POST(msg);   //Send the request
    String payload = http.getString();                  //Get the response payload
    
    Serial.println("http Code : " + String(httpCode));   //Print HTTP return code
    Serial.println("payload : " + payload);    //Print request response payload

    http.end();  //Close connection
    return payload;
}
void setup() {
  Serial.begin (115200);
  delay (2000);
  Serial.setDebugOutput(1);

  String id = String(ESP.getChipId());
  // WiFiManager
  // Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  
  // Uncomment and run it once, if you want to erase all the stored information
  wifiManager.resetSettings(); //Just for development remove later
  
  WiFiManagerParameter gnome_credential("gnomeId", "gnome id", "", 30," type='hidden'"); 
  wifiManager.addParameter(&gnome_credential);
  WiFiManagerParameter user_credential("userId", "user id", "", 30," type='hidden'"); 
  wifiManager.addParameter(&user_credential);
  
  WiFiManagerParameter gnome_credential_script("<script type='text/javascript' defer>  window.addEventListener('message', event => { if (event.origin.startsWith('http://localhost:3000')) { console.log(event.data); document.getElementById('gnomeId').value = event.data.gnomeId; document.getElementById('userId').value = event.data.userId; }});</script>");
  wifiManager.addParameter(&gnome_credential_script);
  WiFiManagerParameter handle_submit_script("<script type='text/javascript' defer>  saveButton = document.getElementsByTagName('button'); saveButton.onclick = () => {window.parent.postMessage('Network Saved', 'http://localhost:3000');}</script>");
  wifiManager.addParameter(&handle_submit_script);
  // set custom ip for portal
  //wifiManager.setAPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  // fetches ssid and pass from eeprom and tries to connect
  // if it does not connect it starts an access point with the specified name
  // here  "AutoConnectAP"
  // and goes into a blocking loop awaiting configuration
  wifiManager.autoConnect("GnomeAutoConnectAP");
  // or use this for auto generated name ESP + ChipID
  //wifiManager.autoConnect();
  
  // if you get here you have connected to the WiFi
  Serial.println("Connected.");
  
  gnomeId = gnome_credential.getValue();
  King_Kyrie_Key = user_credential.getValue();

    //Save credentials into msg docs
    gnomeHoseDoc["user"] = King_Kyrie_Key;
    gnomeHoseDoc["gnome"] = gnomeId;
    gnomeSensorDataDoc["user"] = King_Kyrie_Key;
    gnomeSensorDataDoc["gnome"] = gnomeId;
    
  //fill with ssid and wifi password
//  WiFi.begin(wifi_ssid, wifi_password);
//  Serial.println ("connecting to wifi");
  if (WiFi.status() == WL_CONNECTED) {  //Wait for the WiFI connection completion
    WiFiClient client;
    HTTPClient http;
    String msg = "";
    String payload = "";
    serializeJson(gnomeHoseDoc, msg);
    do{
      payload = postRequest(http, "http://limitless-forest-10226.herokuapp.com/connect", msg );  
    }while(payload != "The Gnome "+gnomeId+" has been connected");
    msg = "";
  }
//  Serial.println ("\nconnected to network " + String(wifi_ssid) + "\n");


  


  pinMode(D8, OUTPUT); //LED green pin
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
    delay(100);
    digitalWrite(D8, LOW);
    delay(100);
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

  
  gnomeSensorDataDoc["light"] = sunlight;
  gnomeSensorDataDoc["temperature"] = airTemp;
  gnomeSensorDataDoc["soil_humidity"] = soilMoisture;
  gnomeSensorDataDoc["state"]["hose"]["is_active"] = gnomeHoseDoc["is_active"];
  
  
  if(gnomeHoseDoc["is_active"]){
    digitalWrite(solenoidPin, HIGH);
    delay(gnomeHoseDoc["duration"].as<int>()*60000);
    digitalWrite(solenoidPin, LOW);
  }


  //Serial.println("////////////////////////////////////////////////////////////");

  if(WiFi.status()== WL_CONNECTED){   //Check WiFi connection status
 
    WiFiClient client;
    HTTPClient http;    //Declare object of class HTTPClient
    String msg;

//    http.begin("http://limitless-forest-10226.herokuapp.com/data");      //Specify request destination
//    http.addHeader("Content-Type", "application/json");  //Specify content-type header
//
//    serializeJson(gnomeSensorDataDoc, msg);
//    //int httpCode = http.POST(postsensormessage(String(airTemp), String(soilTemp), String(soilMoisture), String(sunlight), String(wateredToday)));   //Send the request
//    int httpCode = http.POST(msg);   //Send the request
//    msg = "";
//    String payload = http.getString();                  //Get the response payload
//    
//    Serial.println("http Code : " + String(httpCode));   //Print HTTP return code
//    Serial.println("payload : " + payload);    //Print request response payload
//
//    http.end();  //Close connection

    serializeJson(gnomeSensorDataDoc, msg);
    String payload = postRequest(http, "http://limitless-forest-10226.herokuapp.com/data", msg );
    msg = "";

    
    if(gnomeHoseDoc["is_active"]){
//      http.begin("http://limitless-forest-10226.herokuapp.com/hose");      //Specify request destination
//      http.addHeader("Content-Type", "application/json");  //Specify content-type header

      gnomeHoseDoc["user"] = King_Kyrie_Key;
      gnomeHoseDoc["gnome"] = gnomeId;
      gnomeHoseDoc["is_active"] = false;
      serializeJson(gnomeHoseDoc, msg);
      //int httpCode = http.POST(postgnomehoseoff());   //Send the request
      String payload = postRequest(http, "http://limitless-forest-10226.herokuapp.com/hose", msg );
      //Serial.println(msg);
      msg = "";
//      int httpCode = http.POST(msg);
//      String payload = http.getString();                  //Get the response payload
//      
//      Serial.println("http Code : " + String(httpCode));   //Print HTTP return code
//      Serial.println("payload : " + payload);    //Print request response payload
//  
//      http.end();  //Close connection
    }

    
    http.begin("http://limitless-forest-10226.herokuapp.com/hose?user="+King_Kyrie_Key+"&gnome="+gnomeId);      //Specify request destination
    http.addHeader("Content-Type", "application/json");  //Specify content-type header
    
    int httpCode = http.GET();   //Send the request
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
 

  Serial.println("//////////////////////////////////////////////////////////");
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
