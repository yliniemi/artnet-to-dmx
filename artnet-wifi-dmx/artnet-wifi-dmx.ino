/*
ArtNet to DMX using wifi
Copyright (C) 2022 Antti Yliniemi
https://github.com/yliniemi/artnet-to-dmx

This program is free software: you can redistribute it and/or modify it under the terms of the GNU Affero General Public License version 3 as published by the Free Software Foundation.
This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more details. 
https://www.gnu.org/licenses/agpl-3.0.txt
*/

// TODO for this wifi version
// use my own setup wifi routines

#define NUM_PORTS 3
#define DMX_PORT_TRANSMIT 1
#define DMX_PORT_RECEIVE 2
#define DMX_PORT_DISABLED 0

#include <myCredentials.h>        // oh yeah. there is myCredentials.zip on the root of this repository. include it as a library and then edit the file with your onw ips and stuff
#include "settings.h"

#include <Arduino.h>
#include <esp_dmx.h>

#include <ArtnetWifi.h>

#include "setupWifi.h"
#include "OTA.h"

#ifdef USING_SERIALOTA
#include "SerialOTA.h"
#endif

struct DmxAttributes
{
  dmx_port_t port;
  dmx_config_t config;
  int bufferDepth;
  float averageBufferDepth;
  int maxChannels;
  uint64_t lastPackageTime;
  int artnetUniverseNumber;
  int portPin;
  int portMode;
  uint8_t buffer[2][DMX_MAX_PACKET_SIZE];
  unsigned int bufferIndex;
  int lag;      // longest time sending the actual dmx frame took
  int maxLag;   // longest time between sending frames
  uint64_t lastPrint;
};

struct
{
  int ledPin = -1;                 // the default is 2 for esp32-dev1 and wemos d1 mini esp32, 33 for esp32-cam
  DmxAttributes dmx[NUM_PORTS];
} global;

//Wifi settings
// const char* ssid;
// const char* psk;

WiFiUDP UdpSend;
ArtnetWifi artnet;

/* Make sure to double-check that these pins are compatible with your ESP32!
  Some ESP32s, such as the ESP32-WROVER series, do not allow you to read or
  write data on pins 16 or 17, so it's always good to read the manuals. */

/* Next, lets decide which DMX port to use. The ESP32 has either 2 or 3 ports.
  Port 0 is typically used to transmit serial data back to your Serial Monitor,
  so we shouldn't use that port. Lets use port 1! */


/* Now we want somewhere to store our DMX data. Since a single packet of DMX
  data can be up to 513 bytes long, we want our array to be at least that long.
  This library knows that the max DMX packet size is 513, so we can fill in the
  array size with `DMX_MAX_PACKET_SIZE`. */


/* The last few main variables that we need allow us to log data to the Serial
  Monitor. In this sketch, we want to log some information about the DMX we are
  transmitting once every 44 packets. We'll declare a packet counter variable
  that will keep track of the amount of packets that have been sent since we
  last logged a serial message. We'll also want to increment the value of each
  byte we send so we'll declare a variable to track what value we should
  increment to. */

#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>


#include <AsyncWebConfig.h>

String params = "["
  "{"
  "'name':'hostname',"
  "'label':'hostname',"
  "'type':"+String(INPUTTEXT)+","
  "'default':''"
  "},"
  "{"
  "'name':'ssid',"
  "'label':'ssid',"
  "'type':"+String(INPUTTEXT)+","
  "'default':''"
  "},"
  "{"
  "'name':'psk',"
  "'label':'psk',"
  "'type':"+String(INPUTPASSWORD)+","
  "'default':''"
  "},"
  "{"
  "'name':'waitForArtnetPackages',"
  "'label':'waitForArtnetPackages',"
  "'type':"+String(INPUTCHECKBOX)+","
  "'default':'0'"
  "},"
  "{"
  "'name':'maxDmxChannels',"
  "'label':'maxDmxChannels',"
  "'type':"+String(INPUTNUMBER)+","
  "'min':1,'max':512,"
  "'default':'512'"
  "},"
  "{"
  "'name':'dmx 1 mode',"
  "'label':'dmx 1 mode',"
  "'type':"+String(INPUTRADIO)+","
  "'options':["
  "{'v':'t','l':'transmit'},"
  "{'v':'r','l':'receive'},"
  "{'v':'d','l':'disabled'}],"
  "'default':'t'"
  "},"
  "{"
  "'name':'dmx 1 artnet universe',"
  "'label':'dmx 1 artnet universe',"
  "'type':"+String(INPUTNUMBER)+","
  "'min':-1,'max':32768,"
  "'default':'0'"
  "},"
  "{"
  "'name':'dmx 1 pin',"
  "'label':'dmx 1 pin',"
  "'type':"+String(INPUTNUMBER)+","
  "'min':-1,'max':99,"
  "'default':'-1'"
  "},"
  "{"
  "'name':'built in led pin',"
  "'label':'built in led pin',"
  "'type':"+String(INPUTNUMBER)+","
  "'min':-1,'max':99,"
  "'default':'-1'"
  "},"
  "{"
  "'name':'ota password',"
  "'label':'ota password',"
  "'type':"+String(INPUTPASSWORD)+","
  "'default':''"
  "},"
  "{"
  "'name':'ota rounds',"
  "'label':'ota rounds',"
  "'type':"+String(INPUTNUMBER)+","
  "'min':0,'max':999,"
  "'default':'10'"
  "}"
  "]";

AsyncWebServer server(80);
AsyncWebConfig conf;

TaskHandle_t otas, rebootIfNoClients;

bool initWiFi() {
    bool connected = false;
    if (!setupWifi(conf.getString("ssid").c_str(), conf.getString("psk").c_str()))
    {
      WiFi.mode(WIFI_AP);
      String apSsid = String(AP_SSID_PREFIX);
      apSsid.concat(WiFi.macAddress());
      apSsid.replace(":", "");
      WiFi.softAP(apSsid.c_str(), AP_PSK, 1);
      Serial.print("IP-Adresse = ");
      Serial.println(WiFi.localIP());
      Serial.print("Access Point = ");
      Serial.println(apSsid);
      return false;
    }
    else return true;
}

void handleRoot(AsyncWebServerRequest *request) {
  conf.handleFormRequest(request);
  if (request->hasParam("SAVE")) {
    uint8_t cnt = conf.getCount();
    Serial.println("*********** Configuration ************");
    for (uint8_t i = 0; i<cnt; i++) {
      Serial.print(conf.getName(i));
      Serial.print(" = ");
      Serial.println(conf.values[i]);
    }
  }
}

// TURN THIS FUNTION INTO TWO FUNCTIONS. The for loop makes variable access cumbersome
void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data)
{
  static int counter[NUM_PORTS];
  static int dropped[NUM_PORTS];
  static bool blinky = true;
  for (int i = 0; i < NUM_PORTS; i++)
  {
    if (universe == global.dmx[i].artnetUniverseNumber)
    {
      if (counter[i] == 255)
      {
        Serial.printf("Uni: %5d, length: %5d, seq: %3d, dropped: %3d, buf depth: %f, lag %d, maxLag %d\n",
            universe, length, sequence, dropped[i], global.dmx[i].averageBufferDepth, global.dmx[i].lag, global.dmx[i].maxLag);
        SerialOTA.printf("Uni: %5d, length: %5d, seq: %3d, dropped: %3d, buf depth: %f, lag %d, maxLag %d\n",
            universe, length, sequence, dropped[i], global.dmx[i].averageBufferDepth, global.dmx[i].lag, global.dmx[i].maxLag);
        counter[i] = 0;
        dropped[i] = 0;
        global.dmx[i].lag = 0;
        global.dmx[i].maxLag = 0;
        global.dmx[i].lastPrint = esp_timer_get_time();
      }
      counter[i] = counter[i] + 1;
      if (global.dmx[i].bufferDepth == 2) dropped[i]++;
      // Serial.printf("Uni: %5d, length: %5d, seq: %3d\n", universe, length, sequence);
      global.dmx[i].bufferIndex = (global.dmx[i].bufferIndex + 1) % 2;
      memcpy(&global.dmx[i].buffer[global.dmx[i].bufferIndex][1], &data[0], min((int)length, DMX_MAX_PACKET_SIZE - 1));
      global.dmx[i].bufferDepth = min(global.dmx[i].bufferDepth + 1, 2);
      
    }
    
    #ifndef BLINKY_OFF
    if (blinky)
    {
      gpio_set_level((gpio_num_t)global.ledPin, 0);
      // digitalWrite(ledPin, LOW);
      blinky = false;
    }
    else
    {
      gpio_set_level((gpio_num_t)global.ledPin, 1);
      // digitalWrite(ledPin, HIGH);
      blinky = true;
    }
    #endif
  }
  
}

void handleOTAs(void* parameter)
{
  for (;;)
  {
    vTaskDelay(portTICK_PERIOD_MS * 1000);
    #ifdef USING_SERIALOTA
    SerialOTAhandle();
    #endif
    ArduinoOTA.handle();
  }
}

void waitForClients(void* parameter)
{
  static uint64_t lastMillisWithConnection = esp_timer_get_time();
  for (;;)
  {
    vTaskDelay(portTICK_PERIOD_MS * 10000);
    if (WiFi.softAPgetStationNum() != 0) lastMillisWithConnection = esp_timer_get_time();
    Serial.printf("WiFi.softAPgetStationNum() = %d\n", WiFi.softAPgetStationNum());
    
    if (esp_timer_get_time() > lastMillisWithConnection + AP_TIMEOUT_MINUTES * 60000000)
    {
      Serial.printf("Access point has had no connections for %d minutes. Rebooting\n", AP_TIMEOUT_MINUTES);
      delay(100);
      ESP.restart();
    }
    
  }
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  dmx_config_t dmxConfig[NUM_PORTS];
  for (int i = 0; i < NUM_PORTS; i++)
  {
    global.dmx[i].port = i;
    global.dmx[i].artnetUniverseNumber = -1;
    global.dmx[i].portPin = -1;
    global.dmx[i].portMode = DMX_PORT_DISABLED;
    global.dmx[i].config = DMX_DEFAULT_CONFIG;
  }
  
  Serial.println(params);
  conf.setDescription(params);
  conf.readConfig();
  
  conf.getString("hostname").toCharArray(hostname, 64);
  hostname[63] = 0;

  bool wifiSuccess = initWiFi();
  
  conf.getString("ota password").toCharArray(otaPassword, 64);
  otaPassword[63] = 0;
  
  setupOTA(hostname, otaPassword, otaRounds);
  
  #ifdef USING_SERIALOTA
  setupSerialOTA(hostname);
  #endif
  
  xTaskCreatePinnedToCore(
      handleOTAs, // Function to implement the task
      "handleOTAs", // Name of the task
      10000,  // Stack size in words
      NULL,  // Task input parameter
      0,  // Priority of the task
      &otas,  // Task handle.
      0); // Core where the task should run
  
  if (!wifiSuccess)
  {
    xTaskCreatePinnedToCore(
        waitForClients, // Function to implement the task
        "rebootIfNoClients", // Name of the task
        10000,  // Stack size in words
        NULL,  // Task input parameter
        0,  // Priority of the task
        &rebootIfNoClients,  // Task handle.
        0); // Core where the task should run
  }
  
  char dns[30];
  sprintf(dns,"%s.local",conf.getApName());
  if (MDNS.begin(dns)) {
    Serial.println("MDNS responder gestartet");
  }
  server.on("/",handleRoot);
  server.begin();
  
  conf.getString("ssid").toCharArray(ssid, 64);
  ssid[63] = 0;
  conf.getString("psk").toCharArray(psk, 64);
  psk[63] = 0;
  Serial.print("dmxPortPin[1] = ");
  Serial.println(conf.getString("dmx 1 mode"));
  if (conf.getString("dmx 1 mode").equals("t"))
  {
    global.dmx[1].portMode = DMX_PORT_TRANSMIT;
    Serial.println("dmx 1 port mode = transmit");
  }
  if (conf.getString("dmx 1 mode").equals("r"))
  {
    global.dmx[1].portMode = DMX_PORT_RECEIVE;
    Serial.println("dmx 1 port mode = receive");
  }
  global.dmx[1].artnetUniverseNumber = conf.getInt("dmx 1 artnet universe");
  Serial.print("global.dmx[1].artnetUniverseNumber = ");
  Serial.println(global.dmx[1].artnetUniverseNumber);
  global.dmx[1].portPin = conf.getInt("dmx 1 pin");
  Serial.print("global.dmx[1].portPin = ");
  Serial.println(global.dmx[1].portPin);
  global.ledPin = conf.getInt("built in led pin");
  for (int i = 0; i < NUM_PORTS; i++)
  {
    global.dmx[i].maxChannels = conf.getInt("maxDmxChannels");
  }
  
  #ifndef BLINKY_OFF
  gpio_set_direction((gpio_num_t)global.ledPin, GPIO_MODE_OUTPUT);
  gpio_set_level((gpio_num_t)global.ledPin, 1);
  #endif
  
  // this will be called for each packet received
  artnet.setArtDmxCallback(onDmxFrame);
  artnet.begin();

  /* Start the serial connection back to the computer so that we can log
     messages to the Serial Monitor. Lets set the baud rate to 115200. */
  

  int queueSize = 0;
  int interruptPriority = 1;
  /* Configure the DMX hardware to the default DMX settings and tell the DMX
      driver which hardware pins we are using. */
  for (int i = 0; i < NUM_PORTS; i++)
  {
    if (global.dmx[i].portMode == DMX_PORT_TRANSMIT)
    {
      Serial.print("Transmit i = ");
      Serial.println(i);
      dmx_param_config(global.dmx[i].port, &global.dmx[i].config);
      dmx_set_pin(global.dmx[i].port, global.dmx[i].portPin, -1, -1);
      dmx_driver_install(global.dmx[i].port, DMX_MAX_PACKET_SIZE, queueSize, NULL, interruptPriority);
      dmx_set_mode(global.dmx[i].port, DMX_MODE_WRITE);
    }
    if (global.dmx[i].portMode == DMX_PORT_RECEIVE)
    {
      dmx_param_config(global.dmx[i].port, &global.dmx[i].config);
      dmx_set_pin(global.dmx[i].port, -1, global.dmx[i].portPin, -1); // the last one is an enable pin. I'll pull it up directly from the 3.3v line.
      // I'll have to find out how receiving happens
    }
    // Serial.end();
  }
  Serial.printf("portTICK_PERIOD_MS = %d\n", portTICK_PERIOD_MS);
  Serial.println("Setup Done!");
  delay(100);
}


// TURN THIS FUNTION INTO TWO FUNCTIONS. The for loop makes variable access cumbersome
void loop()
{
  static uint64_t startedSending[NUM_PORTS];
  static bool dmxSent[NUM_PORTS];
  vTaskDelay(portTICK_PERIOD_MS);
  artnet.read();
  for (int i = 0; i < NUM_PORTS; i++)
  {
    if (global.dmx[i].portMode == DMX_PORT_TRANSMIT)
    {
      if (is_dmx_send_done(global.dmx[i].port))
      {
        global.dmx[i].maxLag = max((int)(esp_timer_get_time() - startedSending[i]), global.dmx[i].maxLag);
        if (dmxSent[i])
        {
          global.dmx[i].lag = max((int)(esp_timer_get_time() - startedSending[i]), global.dmx[i].lag);
          dmxSent[i] = false;
        }
        
        if ((global.dmx[i].bufferDepth > 0) | esp_timer_get_time() > global.dmx[i].lastPackageTime + ARTNET_TIMEOUT_MICROSECONDS)
        {
          if (esp_timer_get_time() > global.dmx[i].lastPackageTime + ARTNET_TIMEOUT_MICROSECONDS && (esp_timer_get_time() > global.dmx[i].lastPrint + 5000000))
          {
            Serial.printf("Buf depth: %f, lag %d, maxLag %d\n",
                global.dmx[i].averageBufferDepth, global.dmx[i].lag, global.dmx[i].maxLag);
            SerialOTA.printf("Buf depth: %f, lag %d, maxLag %d\n",
                global.dmx[i].averageBufferDepth, global.dmx[i].lag, global.dmx[i].maxLag);
            global.dmx[i].lag = 0;            
            global.dmx[i].maxLag = 0;
            global.dmx[i].lastPrint = esp_timer_get_time();
          }
          global.dmx[i].averageBufferDepth = global.dmx[i].averageBufferDepth * 0.999 + 0.001 * global.dmx[i].bufferDepth;
          if (global.dmx[i].bufferDepth > 0)
          {
            global.dmx[i].lastPackageTime = esp_timer_get_time();
            if (global.dmx[i].bufferDepth == 1)
            {
              dmx_write_packet(global.dmx[i].port, global.dmx[i].buffer[global.dmx[i].bufferIndex], DMX_MAX_PACKET_SIZE);
            }
            if (global.dmx[i].bufferDepth == 2)
            {
              dmx_write_packet(global.dmx[i].port, global.dmx[i].buffer[(global.dmx[i].bufferIndex + 1) % 2], DMX_MAX_PACKET_SIZE);
            }
            global.dmx[i].bufferDepth--;
          }
          dmx_send_packet(global.dmx[i].port, min(DMX_MAX_PACKET_SIZE, global.dmx[i].maxChannels + 1));
          startedSending[i] = esp_timer_get_time();
          dmxSent[i] = true;
        }
      }
    }
    else global.dmx[i].bufferDepth = 2; 
  }
}
