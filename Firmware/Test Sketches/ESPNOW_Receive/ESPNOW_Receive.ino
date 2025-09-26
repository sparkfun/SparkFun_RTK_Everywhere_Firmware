/*
  Receive dummy data over ESP-NOW

  Receive over broadcast packets.

  A receiver does not need to have the broadcastMac added to its peer list. It will receive a broadcast
  no matter what.

  If desired, onDataRecieve should check the received MAC against the list of friendly/paired MACs
  in order to throw out broadcasted packets that may not be valid data.

  Add peers
  Add callback to deal with allowed incoming data
  Update test sketches
*/

#include <esp_now.h>
#include <esp_mac.h> //Needed for esp_read_mac()
#include <WiFi.h> //Needed because ESP-NOW requires WiFi.mode()

void onDataReceive(const esp_now_recv_info *mac, const uint8_t *incomingData, int len)
{
  //    typedef struct esp_now_recv_info {
  //        uint8_t * src_addr;             // Source address of ESPNOW packet
  //        uint8_t * des_addr;             // Destination address of ESPNOW packet
  //        wifi_pkt_rx_ctrl_t * rx_ctrl;   // Rx control info of ESPNOW packet
  //    } esp_now_recv_info_t;

  // Display the packet info
  Serial.printf(
    "RX from MAC %02X:%02X:%02X:%02X:%02X:%02X, %d bytes - ",
    mac->des_addr[0], mac->des_addr[1], mac->des_addr[2], mac->des_addr[3], mac->des_addr[4], mac->des_addr[5],
    len);

  //Print what was received - it might be binary gobbly-de-gook
  char toPrint[len + 1];
  snprintf(toPrint, sizeof(toPrint), "%s", incomingData);
  Serial.println(toPrint);
}

void setup()
{
  Serial.begin(115200);
  delay(250);
  Serial.println("Remote to Central - This is Central");

  uint8_t unitMACAddress[6];
  esp_read_mac(unitMACAddress, ESP_MAC_WIFI_STA);
  Serial.printf("Hi! My MAC address is: {0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X}\r\n",
                unitMACAddress[0], unitMACAddress[1], unitMACAddress[2], unitMACAddress[3], unitMACAddress[4], unitMACAddress[5]);

  //ESP-NOW must have WiFi initialized
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK)
    Serial.println("Error initializing ESP-NOW");
  else
    Serial.println("ESP-NOW started");

  esp_now_register_recv_cb(onDataReceive);
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
  esp_now_peer_info_t peerInfo;

  memcpy(peerInfo.peer_addr, peerMac, 6);
  peerInfo.channel = 0;
  peerInfo.ifidx = WIFI_IF_STA;
  peerInfo.encrypt = false;

  esp_err_t result = esp_now_add_peer(&peerInfo);
  if (result != ESP_OK)
    Serial.printf("Failed to add peer: 0x%02X%02X%02X%02X%02X%02X\r\n", peerMac[0], peerMac[1], peerMac[2], peerMac[3], peerMac[4], peerMac[5]);
  else
    Serial.printf("Added peer: 0x%02X%02X%02X%02X%02X%02X\r\n", peerMac[0], peerMac[1], peerMac[2], peerMac[3], peerMac[4], peerMac[5]);

  return (result);
}
