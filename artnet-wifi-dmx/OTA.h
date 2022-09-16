#ifndef OTA_H
#define OTA_H

#include <ArduinoOTA.h>

void setupOTA(char* hostname, char* password, int OTArounds);

void setupOTA(char* hostname, char* password);

#endif
