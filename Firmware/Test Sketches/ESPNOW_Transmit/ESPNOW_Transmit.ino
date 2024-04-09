/*
  Transmit dummy data over ESP-NOW

  In this example, we don't have a paired MAC, this example simply broadcasts.

  To send a broadcast, the 0xFF broadcastMac has to be added to the peer list, *and*
  we have to address the message to the broadcastMac, *not* 0 to send to all peers on the list.

  A receiver does not need to have the broadcastMac added to its peer list. It will receive a broadcast
  no matter what.
*/

#include <esp_now.h>
#include <WiFi.h>

uint8_t broadcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t roverMac[] = {0x64, 0xB7, 0x08, 0x3D, 0xFD, 0xAC};
uint8_t mockMac[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}; //Dummy MAC for testing

esp_now_peer_info_t peerInfo;

unsigned long lastSend = 0;

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
}

void loop()
{
  if (millis() - lastSend > 500)
  {
    lastSend = millis();

    uint8_t espnowData[] = "This is the test string.";

    esp_err_t result = ESP_OK;

    result = esp_now_send(0, (uint8_t *)&espnowData, sizeof(espnowData)); // Send packet to all peers on the list, excluding broadcast peer.
    //result = esp_now_send(broadcastMac, (uint8_t *)&espnowData, sizeof(espnowData)); // Send packet over broadcast
    //result = esp_now_send(roverMac, (uint8_t *)&espnowData, sizeof(espnowData)); // Send packet to a specific peer

    if (result == ESP_OK)
      Serial.println("Sent with success");
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
