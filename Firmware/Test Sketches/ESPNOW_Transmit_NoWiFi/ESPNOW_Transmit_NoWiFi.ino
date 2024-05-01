/*
  ESP-NOW coexisting with WiFi
  By: Nathan Seidle
  SparkFun Electronics
  Date: April 9th, 2024
  License: MIT. Please see LICENSE.md for more information.

  In this example, we demonstrate how a transmitter must be on the same channel
  as a receiver and how ESP-NOW can coexist with WiFi.

  Run ESPNOW_Transmit_NoWiFi and ESPNOW_Recieve_WithWiFi on two ESP32s. These example use Broadcast ESP-NOW
  so no MAC addresses should be needed. ESP-NOW communication should flow correctly between the two units.

  On the Receiver, connect to WiFi (press 'w'). Communication will break.

  On the Receiver, press 'g' to get the channel of the connected WiFi.

  On the Transmitter unit, press 'a' until the ESP-NOW Channel increases to match the channel of the Receiver unit.
  Communication should flow correctly again.
*/

#include <esp_now.h>
#include <WiFi.h>
#include "esp_wifi.h" //Needed for esp_wifi_set_protocol()

const char* ssid     = "mySSID";
const char* password = "myPassword";

uint8_t broadcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t roverMac[] = {0x64, 0xB7, 0x08, 0x3D, 0xFD, 0xAC};
uint8_t mockMac[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}; //Dummy MAC for testing

esp_now_peer_info_t peerInfo;

unsigned long lastSend = 0;

int channelNumber = 0;
uint8_t packetCounter = 0; // Intentionally 8-bit so it rolls over

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  Serial.print("Last Packet Send Status: ");
  if (status == ESP_NOW_SEND_SUCCESS)
    Serial.println("Delivery Success");
  else
    Serial.printf("Failed delivery to peer: 0x%02X%02X%02X%02X%02X%02X\r\n", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  Serial.println("Point to Point - Base Transmitter");

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_send_cb(onDataSent);

  //espnowAddPeer(roverMac); // Register a remote address if we want to deliver data there
  espnowAddPeer(mockMac);
  espnowAddPeer(broadcastMac);

  esp_wifi_set_promiscuous(true);
  esp_err_t response = esp_wifi_set_channel(channelNumber, WIFI_SECOND_CHAN_NONE);
  if (response != ESP_OK)
  {
    char responseString[100];
    esp_err_to_name_r(response, responseString, sizeof(responseString));
    Serial.printf("setChannel failed: %s\r\n", responseString);
  }
  esp_wifi_set_promiscuous(false);

  Serial.println("r) Reset");
  Serial.println("a) Increase ESP-NOW channel");
  Serial.println("g) Get channel of WiFi SSID");
}

void loop()
{
  if (millis() - lastSend > 500)
  {
    lastSend = millis();

    char espnowData[100] = "This is the test string.";
    sprintf(espnowData, "This is test #: %d", packetCounter++);

    esp_err_t result = ESP_OK;

    //result = esp_now_send(0, (uint8_t *)&espnowData, strlen(espnowData)); // Send packet to all peers on the list, excluding broadcast peer.
    result = esp_now_send(broadcastMac, (uint8_t *)&espnowData, strlen(espnowData)); // Send packet over broadcast
    //result = esp_now_send(roverMac, (uint8_t *)&espnowData, strlen(espnowData)); // Send packet to a specific peer

    if (result == ESP_OK)
      Serial.println("Sent with success"); // We will always get a success with broadcastMac packets, presumably because they do not have delivery confirmation.
    else
      Serial.println("Error sending the data");
  }

  if (Serial.available())
  {
    byte incoming = Serial.read();
    if (incoming == 'r')
    {
      Serial.println("Reset");
      ESP.restart();
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

// Add a given MAC address to the peer list
esp_err_t espnowAddPeer(uint8_t *peerMac)
{
  return (espnowAddPeer(peerMac, true)); // Encrypt by default
}

esp_err_t espnowAddPeer(uint8_t *peerMac, bool encrypt)
{
  esp_now_peer_info_t peerInfo;

  memcpy(peerInfo.peer_addr, peerMac, 6);
  peerInfo.channel = 0;
  peerInfo.ifidx = WIFI_IF_STA;
  // memcpy(peerInfo.lmk, "RTKProductsLMK56", 16);
  // peerInfo.encrypt = encrypt;
  peerInfo.encrypt = false;

  esp_err_t result = esp_now_add_peer(&peerInfo);
  if (result != ESP_OK)
  {
    Serial.printf("Failed to add peer: 0x%02X%02X%02X%02X%02X%02X\r\n", peerMac[0], peerMac[1], peerMac[2],
                  peerMac[3], peerMac[4], peerMac[5]);
  }
  return (result);
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
