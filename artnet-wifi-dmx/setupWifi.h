#ifndef SETUPWIFI_H
#define SETUPWIFI_H

#include <myCredentials.h>

#include <WiFi.h>

#ifdef USING_SERIALOTA
#include "SerialOTA.h"
#endif

void reconnectToWifiIfNecessary();
void printReconnectHistory();
bool setupWifi(const char* primarySsid, const char* primaryPsk);
bool setupWifi(const char* primarySsid, const char* primaryPsk, IPAddress ip, IPAddress gateway, IPAddress mask);

#endif
