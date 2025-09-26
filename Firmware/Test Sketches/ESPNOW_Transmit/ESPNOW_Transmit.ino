/*
  ESP-NOW tranmit to a specific peer
  By: Nathan Seidle
  SparkFun Electronics
  Date: September 25th, 2025
  License: Public domain / don't care.

  In this example we transmit 'hello world' to a specified peer.

  Run ESPNOW_Transmit and ESPNOW_Recieve on two ESP32s. This example uses Broadcast ESP-NOW
  so no MAC addresses should be needed. ESP-NOW communication should flow correctly between the two units.

  If you assign a remote MAC, once data is flowing, hold the remote in reset to see the delivery failures.

*/

#include <esp_now.h>
#include <esp_mac.h> //Needed for esp_read_mac()
#include <WiFi.h> //Needed because ESP-NOW requires WiFi.mode()

uint8_t broadcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t remoteMac[] = {0xB8, 0xD6, 0x1A, 0x0C, 0xA3, 0xDC}; //Modify this with the MAC address of the remote unit you want to transmit to
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
  delay(250);
  Serial.println("Remote to Central - This is Remote Transmitter");

  uint8_t unitMACAddress[6];
  esp_read_mac(unitMACAddress, ESP_MAC_WIFI_STA);
  Serial.printf("Hi! My MAC address is: {0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X}\r\n",
                unitMACAddress[0], unitMACAddress[1], unitMACAddress[2], unitMACAddress[3], unitMACAddress[4], unitMACAddress[5]);

  //ESP-NOW must have WiFi initialized
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_send_cb(onDataSent);

  //espnowAddPeer(remoteMac); // Register a remote address to deliver data to
  espnowAddPeer(mockMac);
  espnowAddPeer(broadcastMac); //Remote this line to remove broadcast transmissions
}

void loop()
{
  if (millis() - lastSend > 1000)
  {
    lastSend = millis();

    char espnowData[100];
    sprintf(espnowData, "This is test #: %d", packetCounter++);

    esp_err_t result = ESP_OK;

    //result = esp_now_send(0, (uint8_t *)&espnowData, strlen(espnowData)); // Send packet to all peers on the list, excluding broadcast peer.
    //result = esp_now_send(broadcastMac, (uint8_t *)&espnowData, strlen(espnowData)); // Send packet over broadcast
    result = esp_now_send(remoteMac, (uint8_t *)&espnowData, strlen(espnowData)); // Send packet to a specific peer

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
  }
}

esp_err_t espnowAddPeer(uint8_t *peerMac)
{
  esp_now_peer_info_t peerInfo;

  memcpy(peerInfo.peer_addr, peerMac, 6);
  peerInfo.channel = 0;
  peerInfo.ifidx = WIFI_IF_STA;
  peerInfo.encrypt = false;

  esp_err_t result = esp_now_add_peer(&peerInfo);
  if (result != ESP_OK)
  {
    Serial.printf("Failed to add peer: 0x%02X%02X%02X%02X%02X%02X\r\n", peerMac[0], peerMac[1], peerMac[2], peerMac[3], peerMac[4], peerMac[5]);
  }
  return (result);
}