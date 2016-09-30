
/*******************************************************************************
 * Copyright (c) 2016 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution. 
 *
 * The Eclipse Public License is available at 
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at 
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Ansgar Schmidt - Hardware Design aka Kaeschtle
 *    Ansgar Schmidt - Firmware 
 *******************************************************************************/

#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          // https://github.com/tzapu/WiFiManager
#include <Ticker.h>
#include <DHT.h>
#include <PubSubClient.h>         // For MQTT

#define SECOND                       1000
#define DHTPIN                          4 // DHT sensor data pin see pinout https://pradeepsinghblog.files.wordpress.com/2016/04/nodemcu_pins.png?w=616
#define DHTTYPE                     DHT22 // which DHT model we use 22
#define WAIT_UNTIL_NEW_PACKAGE 1 * SECOND // How long we wait to send a new data package 
#define STATUS_LED                      2 // OnBoard LED, we use it to indicate network activity
#define LIGHT_LED                       5 // LED to illuminate the engravings
#define URL                            "quickstart.messaging.internetofthings.ibmcloud.com"
#define CLIENT_ID                      "d:quickstart:nodemcu:Giveaway-13"
#define FIRMWARE                     0.23

WiFiClient      wifiClient;           // Wireless network
PubSubClient    client(wifiClient);   // MQTT Client
DHT             dht(DHTPIN, DHTTYPE); // DHT Temperature and Humidity Sensor
char            msg[100];             // Publish message buffer
uint8_t         messagecounter = 0;   // Counter for message scheduler
Ticker          breathticker;         // Ticker Object for illuminate LED

/**
 * Subroutine called by ticker to indicate wireless setup
 * Blinks both Status and Light led in inverted.
 */
void tick(){
  bool state = digitalRead(STATUS_LED); // get the current state of GPIO1 pin
  digitalWrite(STATUS_LED, !state);     // set pin to the opposite state
  digitalWrite(LIGHT_LED, !state);
}

/**
 * Subroutine called by ticker for breathing the illuminate LED
 */
void breath(){
  float val = ( ( exp( sin( millis() / 8000.0 * PI ) ) - 0.36787944 ) * 150.0 ) + 10; // Inspired by http://sean.voisen.org/blog/2011/10/breathing-led-with-arduino/
  analogWrite(LIGHT_LED, val);
}

/**
 * Setup routine only called after system start or reset.
 */
void setup() {

  ///// Set pin modes for LEDs /////
  pinMode(STATUS_LED, OUTPUT);
  pinMode(LIGHT_LED, OUTPUT);

  ///// Initiate a ticker object for wireless setup indication, we use it locale because we only need it in the setup routine /////
  Ticker ticker;
  ticker.attach(0.6, tick);

  ///// Wifi Manager for configure the wireless network /////
  WiFiManager wifiManager;
  //wifiManager.resetSettings();              // for debugging, this resets the configuration and forces the wifimanager to ask for the wifi network
  wifiManager.setConfigPortalTimeout(60 * 5); // 5 minutes timeout in case of a power failure without direct wireless access
  WiFiManagerParameter custom_text("<center><p>Installed version is 0.23<br>The <b><i>IBM IoT Givaway</i></b> is an open source project.<br>Both hardware and software can be found at github:<br><b>https://github.com/AnsgarSchmidt/IBM-IoT-Giveaway</b></p></center>");
  wifiManager.addParameter(&custom_text);
    
  if (!wifiManager.autoConnect("IBM-IoT-Giveaway")) {
    ESP.reset(); // In case there is no answer within the timeout periode reset and try again to connect to the already known network
    delay(1000);
  }

  ticker.detach();                    // Stop wireless connection blinking
  
  digitalWrite(BUILTIN_LED, HIGH);    // Power of both LEDs
  digitalWrite(LIGHT_LED, LOW);       // because we are now connected
  
  dht.begin();                        // Start the DHT sensor library and query data
  
  client.setServer(URL, 1883);        // configure the mqtt client
  
  breathticker.attach(0.01, breath);  // Start the illumination breath ticker
}

/**
 * Main loop runs until reset / power off / end of the world. Whatever comes first
 */
void loop() {

  ///// Check if we are connected to the MQTT Server otherwise connect.
  if (!client.connected()) {
   while (!client.connected()) {
    if (!client.connect(CLIENT_ID)) {
      delay(5000);  // Wait some time and try again
    }
   }
  }

  ///// Read the current temperature from the DHT sensor.
  float t = dht.readTemperature();
  int  ta = floor(t); 
  int  tb = (t-ta) * pow(10,2);
  yield(); // For the network stack to perform tasks

  ///// Read the current humidity from the DHT sensor.
  float h = dht.readHumidity();
  int  ha = floor(h); 
  int  hb = (h-ha) * pow(10,2);
  yield(); // For the network stack to perform tasks

  digitalWrite(STATUS_LED, LOW); // Set the network indicator LED
  messagecounter++; 

  ///// We need to split the data into sub json messages for the IoTF. Temperature and Humidity will always be part of the message
  if (messagecounter == 10){
    snprintf (msg, sizeof(msg), "{\"d\":{\"temperature\": %d.%d, \"humidity\": %d.%d,  \"name\": \"IBM-IoT-Giveaway\" }}", ta, tb, ha, hb);
    client.publish("iot-2/evt/Giveaway/fmt/json", msg);
    client.loop();
  }else if (messagecounter == 20){
    snprintf (msg, sizeof(msg), "{\"d\":{\"temperature\": %d.%d, \"humidity\": %d.%d,  \"firmware\": 0.23 }}", ta, tb, ha, hb);
    client.publish("iot-2/evt/Giveaway/fmt/json", msg);
    client.loop();
  }else if (messagecounter == 30){
    snprintf (msg, sizeof(msg), "{\"d\":{\"temperature\": %d.%d, \"humidity\": %d.%d,  \"tinkeredBy\": \"@ansi\" }}", ta, tb, ha, hb);
    client.publish("iot-2/evt/Giveaway/fmt/json", msg);
    client.loop();
    messagecounter = 0;
  }else{
    snprintf (msg, sizeof(msg), "{\"d\":{\"temperature\": %d.%d, \"humidity\": %d.%d,  \"uptime\": %d}}", ta, tb, ha, hb, millis());
    client.publish("iot-2/evt/Giveaway/fmt/json", msg);
    client.loop();
  }
  digitalWrite(STATUS_LED, HIGH); 
  
  delay(WAIT_UNTIL_NEW_PACKAGE);

}
