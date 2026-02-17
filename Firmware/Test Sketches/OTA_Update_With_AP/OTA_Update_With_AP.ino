/*
  1) Enable hotspot on your phone - set the credentials below to match your hot spot
  2) Set "Core Debug Level" to Debug
  3) Load this code onto any old ESP32 (classic, not S3, C6, etc)
  4) Press 'u' to access firmware update over the internet <- Should work
  5) Now connect to 'RTK Test' WiFi network from your phone
  6) Press 'u' to access firmware update, fails with the following output

  [ 18127][D][NetworkManager.cpp:127] hostByName(): DNS found IPv4 185.199.108.133
  [ 23136][I][ssl_client.cpp:133] start_ssl_client(): select returned due to timeout 5000 ms for fd 48

  Fails using Arduino core v3.0.1, 3.0.7, 3.2.1 <- You pick the one you want. We use 3.0.7 a lot.

  Why does connecting to the ESP32's AP cause the ESP32 to be unable to do HTTP Gets?

  We have seen this work before on a large codebase using the ESP32 Arduino v3.0.1 core but I can no longer
  replicate that success locally compiling the source. The compiled binary works, the locally compiled binary fails.
*/

#include <Arduino.h>
#include <ESP32OTAPull.h>

#define R4A_MILLISECONDS_IN_A_SECOND    1000

#include "Secrets.h"
#include "Telnet.h"

#define JSON_URL   "https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/main/RTK-Everywhere-Firmware.json"
#define VERSION    "1.0.0"

bool RTK_CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC =
    false; // Flag used by the special build of libmbedtls (libmbedcrypto) to select external memory

ESP32OTAPull ota;

R4A_TELNET_SERVER telnet;

void setup()
{
  Serial.begin(115200);
  delay(200);

  // Enable both AP and Station modes
  WiFi.mode(WIFI_AP_STA);

  // Start the Soft AP
  WiFi.softAP("RTK Test");
  printChannel(&Serial);
  Serial.print("Soft AP Created with IP Gateway ");
  Serial.println(WiFi.softAPIP());

  // Connect to the phone's hot spot
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(250);
  }
  Serial.println();

  // Display the WiFi information
  wifiDisplayInfo(&Serial);

  printMenu(&Serial);
}

void loop()
{
  r4aSerialMenu();
  telnet.update(true);
}

void processMenu(const char * command, Print * display)
{
  if (command[0] == 'r')
  {
    ESP.restart();
  }
  else if (command[0] == 'c')
  {
    printChannel(display);
  }
  else if (command[0] == 'u')
  {
    WiFi.STA.setDefault();
    display->printf("Check for update, but don't download it.\r\n");
    int ret = ota.CheckForOTAUpdate(JSON_URL, VERSION, ESP32OTAPull::DONT_DO_UPDATE);
    display->printf("CheckForOTAUpdate returned %d (%s)\r\n\n", ret, errtext(ret));

    String otaVersion = ota.GetVersion();
    display->printf("OTA Version Available: %s\r\n", otaVersion.c_str());
  }
  else if (command[0] == 'w')
  {
      wifiDisplayInfo(display);
  }
  printMenu(display);
}

void printMenu(Print * display)
{
  display->printf("r - ESP32 reboot\r\n");
  display->printf("c - Display the channels\r\n");
  display->printf("u - Check for firmware version\r\n");
  display->printf("w - Display WiFi information\r\n");
}

void printSoftApChannel(Print * display)
{
  NetworkInterface * interface;

  interface = Network.getDefaultInterface();
  WiFi.AP.setDefault();
  display->printf("WiFi AP Channel: %d\r\n", WiFi.channel());
  if (interface)
    Network.setDefaultInterface(*interface);
}

void printStationChannel(Print * display)
{
  NetworkInterface * interface;

  interface = Network.getDefaultInterface();
  WiFi.STA.setDefault();
  display->printf("WiFi STA Channel: %d\r\n", WiFi.channel());
  if (interface)
    Network.setDefaultInterface(*interface);
}

void printChannel(Print * display)
{
  printSoftApChannel(display);
  printStationChannel(display);
}

void wifiDisplayInfo(Print * display)
{
  printSoftApChannel(display);
  display->printf("Soft AP IP: %s\r\n", WiFi.AP.localIP().toString().c_str());
  display->printf("SSID: %s\r\n", ssid);
  display->printf("Password: %s\r\n", password);
  printStationChannel(display);
  display->printf("Station IP: %s\r\n", WiFi.STA.localIP().toString().c_str());
  display->println();
}

const char *errtext(int code)
{
  switch (code)
  {
    case ESP32OTAPull::UPDATE_AVAILABLE:
      return "An update is available but wasn't installed";
    case ESP32OTAPull::NO_UPDATE_PROFILE_FOUND:
      return "No profile matches";
    case ESP32OTAPull::NO_UPDATE_AVAILABLE:
      return "Profile matched, but update not applicable";
    case ESP32OTAPull::UPDATE_OK:
      return "An update was done, but no reboot";
    case ESP32OTAPull::HTTP_FAILED:
      return "HTTP GET failure";
    case ESP32OTAPull::WRITE_ERROR:
      return "Write error";
    case ESP32OTAPull::JSON_PROBLEM:
      return "Invalid JSON";
    case ESP32OTAPull::OTA_UPDATE_FAIL:
      return "Update fail (no OTA partition?)";
    default:
      if (code > 0)
        return "Unexpected HTTP response code";
      break;
  }
  return "Unknown error";
}
