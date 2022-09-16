#ifndef SETTINGS_H
#define SETTINGS_H

#define CONFIG_FILE_NAME "/config.jsn"

// #define DEBUG                     // uncomment this to see what gets read from the config. including psk and ota password
#define USING_SERIALOTA           // uncomment this if you are not using SerialOTA
#define USING_LED_BUFFER
#define FASTLED_ESP32_I2S
// #define BLINKY_OFF
// #define DMX_OFF

#define AP_PSK "temptemp"
#define AP_SSID_PREFIX "artnet_dmx_"
#define AP_TIMEOUT_MINUTES 2
#define ARTNET_TIMEOUT_MICROSECONDS 250000               // turn this into a variable changeable from the gui

char otaPassword[64] = OTA_PASSWORD;
int otaRounds = 10;             // this is how many seconds we waste waiting for the OTA during boot. sometimes people make mistakes in their code - not me - and the program freezes. this way you can still update your code over the air even if you have some dodgy code in your loop
bool serialEnabled = true;

char ssid[64] = "";
char psk[64] = "";
char hostname[64] = "wifi_dmx_serial_number";          // can be at most 32 characters

// const int maxFileSize = 10240;

bool staticIpEnabled = false;
int ip[] = {192, 168, 69, 213};
int gateway[] = {192, 168, 69, 1};               // this is also the default dns server
int mask [] = {255, 255, 255, 0};
#endif
