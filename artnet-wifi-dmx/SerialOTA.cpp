#include "SerialOTA.h"


WiFiServer telnetServer(23);
WiFiClient SerialOTA;

char* OTAhostname;

void SerialOTAhandle()
{
  static bool haveClient = false; // this is so that when we already have a client we won't disconnect them
  if (!haveClient)
  {
    // Check for new client connections.
    SerialOTA = telnetServer.available();
    if (SerialOTA)
    {
      haveClient = true;
      SerialOTA.print("Welcome to ");
      SerialOTA.println(OTAhostname);
    }
  }
  else if (!SerialOTA.connected())
  {
    // The current client has been disconnected.
    SerialOTA.stop();
    SerialOTA = WiFiClient();
    haveClient = false;
  }
}

void setupSerialOTA(char* setHostname)
{
  telnetServer.begin();
  telnetServer.setNoDelay(true);
  OTAhostname = setHostname;
}
