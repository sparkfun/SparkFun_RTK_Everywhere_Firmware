/*
  Receive dummy data over ESP-NOW

  Receive over broadcast packets.

  A receiver does not need to have the broadcastMac added to its peer list. It will receive a broadcast
  no matter what.

  If desired, onDataRecieve should check the received MAC against the list of friendly/paired MACs 
  in order to throw out broadcasted packets that may not be valid data.
*/

#include <esp_now.h>
#include <WiFi.h>

void onDataRecieve(const uint8_t *mac_addr, const uint8_t *incomingData, int len)
{
  Serial.printf("Bytes received: %d", len);
  Serial.printf(" from peer: 0x%02X%02X%02X%02X%02X%02X\r\n", mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  Serial.println("Point to Point Receiver - No WiFi");

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
