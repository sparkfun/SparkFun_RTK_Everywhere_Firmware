/*
  ESP-NOW transmit to a specific peer
  By: Nathan Seidle
  SparkFun Electronics
  Date: September 25th, 2025
  License: Public domain / don't care.

  In this example we transmit 'This is test #1' to a specified peer or broadcast.

  Run ESPNOW_Transmit and ESPNOW_Recieve on two ESP32s. 
  
  The remote will print its MAC address. Reload this transmit example with that remote's MAC address.
  
  The transmit unit must have the remote MAC or the broadcast MAC added to the peer list.
  
  If the broadcast MAC is added to the peer list, then any device will receive data sent to the broadcast MAC.

  Interestingly, the remote devices don't have to have peers added. A remote will 'receive' packets that either 
  have its MAC address or the broadcast MAC address in the packet.
*/

#include <esp_now.h>
#include <esp_mac.h> //Needed for esp_read_mac()
#include <WiFi.h> //Needed because ESP-NOW requires WiFi.mode()

uint8_t broadcastMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t remoteMac[] = {0x64, 0xB7, 0x08, 0x86, 0xA4, 0xD0}; //Modify this with the MAC address of the remote unit you want to transmit to

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
  Serial.println("ESP-NOW Example - This device is the transmitter");

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

  esp_now_register_send_cb((esp_now_send_cb_t)onDataSent);

  Serial.println();
  Serial.println("a) Add remote MAC to the peer list");
  Serial.println("b) Add broadcast MAC to the peer list");
  Serial.println("c) Clear the peer list");
  Serial.println("1) Send data to remote MAC");
  Serial.println("2) Send data to broadcast MAC");
  Serial.println("r) Reset");
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
    else if (incoming == 'a')
    {
      Serial.println("Adding peer to list");
      espnowAddPeer(remoteMac); // Register a remote address to deliver data to
    }
    else if (incoming == 'b')
    {
      Serial.println("Adding broadcast to list");
      espnowAddPeer(broadcastMac); // Register a remote address to deliver data to
    }
    else if (incoming == 'c')
    {
      Serial.println("Clearing broadcast list");
      espnowClearPeerList();
    }
    else if (incoming == '1')
    {
      Serial.println("Send to peer list");
      espnowSendMessage(0); // Send packet to all peers on the list, excluding broadcast peer.
    }
    else if (incoming == '2')
    {
      Serial.println("Send via broadcast");
      espnowSendMessage(broadcastMac);// Send packet over broadcast
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

// Send data packet to a given MAC
void espnowSendMessage(const uint8_t *sendToMac)
{
  char espnowData[100];
  sprintf(espnowData, "This is test #: %d", packetCounter++);

  esp_err_t result = esp_now_send(sendToMac, (uint8_t *)&espnowData, sizeof(espnowData)); // Send packet to a specific peer

  if (result == ESP_OK)
    Serial.println("Sent with success"); // We will always get a success with broadcastMac packets, presumably because they do not have delivery confirmation.
  else
    Serial.println("Error sending the data");
}

void espnowClearPeerList()
{
//  if (esp_now_is_peer_exist(broadcastMac))
//  {
//    status = espNowRemovePeer(broadcastMac);
//    if (status != ESP_OK)
//      Serial.printf("ERROR: Failed to delete broadcast peer, status: %d\r\n", status);
//    else
//      Serial.printf("ESP-NOW removed broadcast peer\r\n");
//  }

  // Remove all peers
  while (1)
  {
    esp_now_peer_info_t peerInfo;

    // Get the next peer
    esp_err_t status = esp_now_fetch_peer(true, &peerInfo);
    if (status != ESP_OK)
    {
      Serial.println("All done!");
      break;
    }

    // Remove the unicast peer
    status = esp_now_del_peer(peerInfo.peer_addr);
    if (status != ESP_OK)
    {
      Serial.printf("ERROR: Failed to delete peer %02X:%02X:%02X:%02X:%02X:%02X, status: %d\r\n",
                    peerInfo.peer_addr[0], peerInfo.peer_addr[1], peerInfo.peer_addr[2], peerInfo.peer_addr[3],
                    peerInfo.peer_addr[4], peerInfo.peer_addr[5], status);
      break;
    }

    Serial.printf("ESP-NOW removed peer %02X:%02X:%02X:%02X:%02X:%02X\r\n", peerInfo.peer_addr[0],
                  peerInfo.peer_addr[1], peerInfo.peer_addr[2], peerInfo.peer_addr[3], peerInfo.peer_addr[4],
                  peerInfo.peer_addr[5]);
  }
}
