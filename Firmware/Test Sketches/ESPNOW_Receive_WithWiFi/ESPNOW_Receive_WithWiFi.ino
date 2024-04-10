/*
  ESP-NOW coexisting with WiFi
  By: Nathan Seidle
  SparkFun Electronics
  Date: April 9th, 2024
  License: MIT. Please see LICENSE.md for more information.

  Run ESPNOW_Transmit_NoWiFi and ESPNOW_Recieve_WithWiFi on two ESP32s. These example use Broadcast ESP-NOW
  so no MAC addresses should be needed. ESP-NOW communication should flow correctly between the two units.

  On the Receiver, connect to WiFi (press 'w'). Communication will break.

  On the Receiver, press 'g' to get the channel of the connected WiFi.

  On the Transmitter unit, press 'a' until the ESP-NOW Channel increases to match the channel of the Receiver unit.
  Communication should flow correctly again.
  
  Gotchas:
    Calling esp_now_init causes the WiFi protocols to get overwritten. We have to reset protocols before WiFi.begin will work.
    Once connected to WiFi, the radio changes its channel to match the channel of the SSID we connected to. This channel is
    likely different from the default 0 channel of the ESP-NOW transmitter. To receive packets from the transmitter, we also
    need to change the transmitter's channel to match the channel of the SSID.
    Finally, when WiFi has been started, the 2.4GHz radio automatically enters a power save mode. This causes most ESP-NOW 
    packets to be lost. WiFi.setSleep(false) will keep it awake.
*/

#include <esp_now.h>
#include <WiFi.h>
#include "esp_wifi.h" //Needed for esp_wifi_set_protocol()

const char* ssid     = "mySSID";
const char* password = "myPassword";

int channelNumber = 10;

void onDataRecieve(const uint8_t *mac_addr, const uint8_t *incomingData, int len)
{
  Serial.printf("Peer: 0x%02X%02X%02X%02X%02X%02X ", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.printf("Data: %s\r\n", incomingData);
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  Serial.println("Point to Point Receiver - Add WiFi");

  uint8_t unitMACAddress[6]; //Use MAC address in BT broadcast and display
  esp_read_mac(unitMACAddress, ESP_MAC_WIFI_STA);
  Serial.print("WiFi MAC Address:");
  for (int x = 0 ; x < 6 ; x++)
  {
    Serial.print(" 0x");
    if (unitMACAddress[x] < 0x10) Serial.print("0");
    Serial.print(unitMACAddress[x], HEX);
  }
  Serial.println();

  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(onDataRecieve);

  Serial.println("r) Reset");
  Serial.println("w) Connect to WiFi");
  Serial.println("a) Increase ESP-NOW channel");
  Serial.println("g) Get channel of WiFi SSID");
}

void loop()
{
  if (Serial.available())
  {
    byte incoming = Serial.read();
    if (incoming == 'r')
    {
      Serial.println("Reset");
      ESP.restart();
    }
    else if (incoming == 'w')
    {
      Serial.printf("Connecting to %s... ", ssid);

      WiFi.mode(WIFI_STA); //Mode must be in STA before esp_wifi_set_protocol() can succeed

      // Re-enable WiFi protocols so that WiFi.begin() will succeed
      esp_err_t response = esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR); // Enable WiFi+ESP-NOW protocols.
      if (response != ESP_OK)
        Serial.printf("wifiConnect: Error setting WiFi protocols: %s\r\n", esp_err_to_name(response));

      WiFi.setSleep(false); //We must disable sleep so that ESP-NOW can readily receive packets

      // If ESP-NOW has been previously initialized, the _11B, _11G, and _11N protocols are disabled causing infinite wait during WiFi.begin(). 
      // Be sure to reset the protocols.
      WiFi.begin(ssid, password); 

      int wifiResponse = WiFi.waitForConnectResult();

      if (wifiResponse == WL_CONNECTED)
      {
        Serial.print("Connected with IP address: ");
        Serial.println(WiFi.localIP());
      }
      else
        Serial.printf("Failed to connect with wifiResponse %d\r\n", wifiResponse);
    }
    else if (incoming == 'a')
    {
      channelNumber++;
      Serial.printf("Going to channel number: %d\r\n", channelNumber);

      esp_wifi_set_promiscuous(true);
      esp_err_t response = esp_wifi_set_channel(channelNumber, WIFI_SECOND_CHAN_NONE);
      if (response != ESP_OK)
      {
        char responseString[100];
        esp_err_to_name_r(response, responseString, sizeof(responseString));
        Serial.printf("setChannel failed: %s\r\n", responseString);
      }
      esp_wifi_set_promiscuous(false);
    }
    else if (incoming == 'g')
    {
      int wifiChannel = getWiFiChannel(ssid);
      Serial.printf("Current wifi channel for %s: %d\r\n", ssid, wifiChannel);
    }
  }
}

int32_t getWiFiChannel(const char *ssid)
{
  if (int32_t n = WiFi.scanNetworks())
  {
    for (uint8_t i = 0; i < n; i++)
    {
      if (!strcmp(ssid, WiFi.SSID(i).c_str()))
        return WiFi.channel(i);
    }
  }
  return 0;
}
