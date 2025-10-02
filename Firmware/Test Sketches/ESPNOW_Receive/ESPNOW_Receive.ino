/*
  Receive dummy data over ESP-NOW

  Receive over broadcast packets.

  A receiver does not need to have the broadcastMac added to its peer list. It will receive a broadcast
  no matter what.

  Interestingly, the receiver does not have peers added. It will 'receive' packets that either have 
  its MAC address or the broadcast MAC address in the packet.

  The transmitter needs to have either this remote's MAC address added as a peer, or the broadcast MAC added as a peer.
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
  Serial.println("ESP-NOW Example - This device is the receiver");

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

  esp_now_register_recv_cb((esp_now_recv_cb_t)onDataReceive);
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
