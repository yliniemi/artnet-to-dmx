#define SETUPWIFI_CPP
#include "setupWifi.h"

#define TRY_RECONNECTING 5      // when losing wifi try WiFi.reconnect() this many time
#define TRY_DISCONNECTING 25    // if reconnecting doesn't work try first disconnecting and then doing WiFi.begin()

struct
{
  char ssid[64];
  char psk[64];
  char tempSsid[64];
  char tempPsk[64];
  bool usingStaticIp = false;
  IPAddress ip;
  IPAddress gateway;
  IPAddress mask;
} persistent;

String WifiReconnectedAt = "";

TaskHandle_t task;

void reconnectToWifiIfNecessary(void* parameter)
{
  for (;;)
  {
    delay(10000);
    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.println();
      Serial.println("WiFi is disconnected");
      WifiReconnectedAt += WiFi.status();

      for (int i = 0; i < TRY_RECONNECTING && WiFi.status() != WL_CONNECTED; i++)
      {
        WiFi.reconnect();
        Serial.println();
        Serial.println("Trying to reconnect");
        delay(10000);
        WifiReconnectedAt += WiFi.status();
      }
      
      for (int i = 0; i < TRY_DISCONNECTING && WiFi.status() != WL_CONNECTED; i++)
      {
        WiFi.disconnect();
        delay(1000);
        if (persistent.usingStaticIp) WiFi.config(persistent.ip, persistent.gateway, persistent.mask);
        WiFi.mode(WIFI_STA);
        WiFi.begin(persistent.ssid, persistent.psk);
        Serial.println();
        Serial.print("Trying to connect to ");
        Serial.println(persistent.ssid);
        delay(9000);
        WifiReconnectedAt += WiFi.status();
      }
      
      if (WiFi.status() != WL_CONNECTED)
      {
        Serial.println();
        Serial.println("WiFi is gone for good. Giving up. Nothing matters. I hope I can do better in my next life. Rebooting....");
        printReconnectHistory();
        delay(5000);
        ESP.restart();
      }
      int time = millis();
      Serial.println(String("Connection to ") + persistent.ssid + " succeeded!");
      Serial.print(String("IP: "));
      Serial.println(WiFi.localIP());
      WifiReconnectedAt += String(" WiFi reconnected at: ") + time / (1000 * 60 * 60) + ":" + time / (1000 * 60) % 60 + ":" + time / 1000 % 60 + "\r\n";
      printReconnectHistory();
    }
  }
}

void printReconnectHistory()
{
  Serial.println();
  Serial.println(WifiReconnectedAt);
  #ifdef USING_SERIALOTA
  SerialOTA.println();
  SerialOTA.println(WifiReconnectedAt);
  #endif
}

bool setupWifi(const char* primarySsid, const char* primaryPsk)
{
  WiFi.persistent(false);          // we don't want to save the credentials on the internal filessytem of the esp32
  
  for (int i = 0; i < 63; i++)
  {
    persistent.ssid[i] = primarySsid[i];
    persistent.psk[i] = primaryPsk[i];
  }
  persistent.ssid[63] = 0;
  persistent.ssid[63] = 0;
  
  if (persistent.usingStaticIp) WiFi.config(persistent.ip, persistent.gateway, persistent.mask);
  WiFi.mode(WIFI_STA);
  WiFi.begin(persistent.ssid, persistent.psk);
  Serial.println();
  Serial.print("Trying to connect to ");
  Serial.print(persistent.ssid);
  Serial.print(" ");
  for (int i = 0; i < 20; i++)
  {
    delay(500);
    Serial.print(".");
    if (WiFi.waitForConnectResult() == WL_CONNECTED) break;
  }    // we wait 10 seconds but break if connection succeeds
  
  Serial.println();
  if (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.println();
    Serial.println("Connection Failed! Returning false...");
    return false;
  }

  Serial.println(String("Connection to ") + persistent.ssid + " succeeded!");
  // WiFi.setAutoReconnect(true);  // this didn't work well enough. i had to do this another way
  // WiFi.onEvent(WiFiStationDisconnected, SYSTEM_EVENT_STA_DISCONNECTED);   // this one was problematic too because you shouldn't do much right after trying to reconnect. perhaps fastled disabling interrupts does something nefarious

  xTaskCreatePinnedToCore(
      reconnectToWifiIfNecessary, // Function to implement the task
      "maintenance", // Name of the task
      10000,  // Stack size in words
      NULL,  // Task input parameter
      0,  // Priority of the task
      &task,  // Task handle.
      0); // not core 1
  
  return true;
}

// turned out I didn't need these funtions but I leave them hare in case I need them in the future
bool setupWifi(const char* primarySsid, const char* primaryPsk, IPAddress ip, IPAddress gateway, IPAddress mask)
{
  persistent.usingStaticIp = true;
  persistent.ip = ip;
  persistent.ip = gateway;
  persistent.ip = mask;
  return setupWifi(primarySsid, primaryPsk);
}
