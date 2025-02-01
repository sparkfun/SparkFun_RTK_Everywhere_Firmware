/**********************************************************************
  ESPNOW.ino

  Handle the ESP-NOW events
  Use ESP NOW protocol to transmit RTCM between RTK Products via 2.4GHz

  How pairing works:
    1. Device enters pairing mode
    2. Device adds the broadcast MAC (all 0xFFs) as peer
    3. Device waits for incoming pairing packet from remote
    4. If valid pairing packet received, add peer, immediately transmit a pairing packet to that peer and exit.

    ESP NOW is bare metal, there is no guaranteed packet delivery. For RTCM byte transmissions using ESP NOW:
      We don't care about dropped packets or packets out of order. The ZED will check the integrity of the RTCM packet.
      We don't care if the ESP NOW packet is corrupt or not. RTCM has its own CRC. RTK needs valid RTCM once every
      few seconds so a single dropped frame is not critical.
**********************************************************************/

#ifdef COMPILE_ESPNOW

//****************************************
// Constants
//****************************************

const uint8_t espNowBroadcastAddr[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

//****************************************
// Types
//****************************************

// Create a struct for ESP NOW pairing
typedef struct _ESP_NOW_PAIR_MESSAGE
{
    uint8_t macAddress[6];
    bool encrypt;
    uint8_t channel;
    uint8_t crc; // Simple check - add MAC together and limit to 8 bit
} ESP_NOW_PAIR_MESSAGE;

//****************************************
// Locals
//****************************************

ESPNOWState espNowState;

//****************************************
// Forward routine declarations
//****************************************

esp_err_t espNowSendPairMessage(const uint8_t *sendToMac = espNowBroadcastAddr);

//*********************************************************************
// Add a peer to the ESP-NOW network
esp_err_t espNowAddPeer(const uint8_t * peerMac)
{
    esp_now_peer_info_t peerInfo;

    // Describe the peer
    memcpy(peerInfo.peer_addr, peerMac, 6);
    peerInfo.channel = 0;
    peerInfo.ifidx = WIFI_IF_STA;
    peerInfo.encrypt = false;

    // Add the peer
    if (settings.debugEspNow)
        systemPrintf("Calling esp_now_add_peer\r\n");
    esp_err_t result = esp_now_add_peer(&peerInfo);
    if (settings.debugEspNow == true)
    {
        if (result != ESP_OK)
            systemPrintf("ERROR: Failed to add ESP-NOW peer %02x:%02x:%02x:%02x:%02x:%02x, result: %s\r\n",
                         peerMac[0], peerMac[1],
                         peerMac[2], peerMac[3],
                         peerMac[4], peerMac[5],
                         esp_err_to_name(result));
        else
            systemPrintf("Added ESP-NOW %s peer %02x:%02x:%02x:%02x:%02x:%02x\r\n",
                         peerInfo.encrypt ? "encrypted" : "unencrypted",
                         peerMac[0], peerMac[1],
                         peerMac[2], peerMac[3],
                         peerMac[4], peerMac[5]);
    }
    return result;
}

//*********************************************************************
// Start ESP-Now if needed, put ESP-Now into broadcast state
void espNowBeginPairing()
{
    espnowStart();

    // To begin pairing, we must add the broadcast MAC to the peer list
    espNowAddPeer(espNowBroadcastAddr);

    espNowSetState(ESPNOW_PAIRING);
}

//*********************************************************************
// Get the current ESP-NOW state
ESPNOWState espnowGetState()
{
    return espNowState;
}

//*********************************************************************
// Callback when data is received
void espNowOnDataReceived(const esp_now_recv_info *mac,
                          const uint8_t *incomingData,
                          int len)
{
    if (espNowState == ESPNOW_PAIRING)
    {
        if (len == sizeof(ESP_NOW_PAIR_MESSAGE)) // First error check
        {
            ESP_NOW_PAIR_MESSAGE pairMessage;
            memcpy(&pairMessage, incomingData, sizeof(pairMessage));

            // Check CRC
            uint8_t tempCRC = 0;
            for (int x = 0; x < 6; x++)
                tempCRC += pairMessage.macAddress[x];

            if (tempCRC == pairMessage.crc) // 2nd error check
            {
                memcpy(&receivedMAC, pairMessage.macAddress, 6);
                espNowSetState(ESPNOW_MAC_RECEIVED);
            }
            // else Pair CRC failed
        }
    }
    else
    {
        espnowRSSI = packetRSSI; // Record this packet's RSSI as an ESP NOW packet

        // We've just received ESP-Now data. We assume this is RTCM and push it directly to the GNSS.
        // Determine if ESPNOW is the correction source
        if (correctionLastSeen(CORR_ESPNOW))
        {
            // Pass RTCM bytes (presumably) from ESP NOW out ESP32-UART to GNSS
            gnss->pushRawData((uint8_t *)incomingData, len);

            if ((settings.debugEspNow == true || settings.debugCorrections == true) && !inMainMenu)
                systemPrintf("ESPNOW received %d RTCM bytes, pushed to GNSS, RSSI: %d\r\n", len, espnowRSSI);
        }
        else
        {
            if ((settings.debugEspNow == true || settings.debugCorrections == true) && !inMainMenu)
                systemPrintf("ESPNOW received %d RTCM bytes, NOT pushed due to priority, RSSI: %d\r\n", len,
                             espnowRSSI);
        }

        espnowIncomingRTCM = true; // Display a download icon
        lastEspnowRssiUpdate = millis();
    }
}

//*********************************************************************
// Buffer RTCM data and send to ESP-NOW peer
void espnowProcessRTCM(byte incoming)
{
    // If we are paired,
    // Or if the radio is broadcasting
    // Then add bytes to the outgoing buffer
    if (espNowState == ESPNOW_PAIRED || espNowState == ESPNOW_BROADCASTING)
    {
        // Move this byte into ESP NOW to send buffer
        espnowOutgoing[espnowOutgoingSpot++] = incoming;
        espnowLastAdd = millis();
    }

    // Send buffer when full
    if (espnowOutgoingSpot == sizeof(espnowOutgoing))
    {
        espnowOutgoingSpot = 0; // Wrap

        if (espNowState == ESPNOW_PAIRED)
            esp_now_send(0, (uint8_t *)&espnowOutgoing, sizeof(espnowOutgoing)); // Send packet to all peers
        else // if (espNowState == ESPNOW_BROADCASTING)
        {
            esp_now_send(espNowBroadcastAddr, (uint8_t *)&espnowOutgoing,
                         sizeof(espnowOutgoing)); // Send packet via broadcast
        }

        delay(10); // We need a small delay between sending multiple packets

        espnowBytesSent += sizeof(espnowOutgoing);

        espnowOutgoingRTCM = true;
    }
}

//*********************************************************************
// Remove a given MAC address from the peer list
esp_err_t espnowRemovePeer(const uint8_t *peerMac)
{
    esp_err_t response = esp_now_del_peer(peerMac);
    if (response != ESP_OK)
    {
        if (settings.debugEspNow == true)
            systemPrintf("Failed to remove peer: %s\r\n", esp_err_to_name(response));
    }

    return (response);
}

//----------------------------------------------------------------------
// ESP-NOW bringup from example 4_9_ESP_NOW
//   1. Set station mode
//   2. Create nowSerial as new ESP_NOW_Serial_Class
//   3. nowSerial.begin
// ESP-NOW bringup from RTK
//   1. Get WiFi mode
//   2. Set WiFi station mode if necessary
//   3. Get WiFi station protocols
//   4. Set WIFI_PROTOCOL_LR protocol
//   5. Call esp_now_init
//   6. Call esp_wifi_set_promiscuous(true)
//   7. Set promiscuous receive callback [esp_wifi_set_promiscuous_rx_cb(promiscuous_rx_cb)]
//      to get RSSI of action frames
//   8. Assign a channel if necessary, call espnowSetChannel
//   9. Set receive callback [esp_now_register_recv_cb(espNowOnDataReceived)]
//  10. Add peers from settings
//      A. If no peers exist
//          i.   Determine if broadcast peer exists, call esp_now_is_peer_exist
//          ii.  Add broadcast peer if necessary, call espNowAddPeer
//          iii. Set ESP-NOW state, call espNowSetState(ESP_NOW_BROADCASTING)
//      B. If peers exist,
//          i.  Set ESP-NOW state, call espNowSetState(ESP_NOW_PAIRED)
//          ii. Loop through peers listed in settings, for each
//              a. Determine if peer exists, call esp_now_is_peer_exist
//              b. Add peer if necessary, call espNowAddPeer
//
// In espNowOnDataReceived
//  11. Save ESP-NOW RSSI
//  12. Set lastEspnowRssiUpdate = millis()
//  13. If in ESP_NOW_PAIRING state
//      A. Validate message CRC
//      B. If valid CRC
//          i.  Save peer MAC address
//          ii. espNowSetState(ESPNOW_MAC_RECEIVED)
//  14. Else if ESPNOW_MAC_RECEIVED state
//      A. If ESP-NOW is corrections source, correctionLastSeen(CORR_ESPNOW)
//          i.  gnss->pushRawData
//  15. Set espnowIncomingRTCM
//
// ESP-NOW shutdown from RTK
//   1. esp_wifi_set_promiscuous(false)
//   2. esp_wifi_set_promiscuous_rx_cb(nullptr)
//   3. esp_now_unregister_recv_cb()
//   4. Remove all peers by calling espnowRemovePeer
//   5. Get WiFi mode
//   6. Set WiFi station mode if necessary
//   7. esp_wifi_get_protocol
//   8. Turn off long range protocol if necessary, call esp_wifi_set_protocol
//   9. Turn off ESP-NOW. call esp_now_deinit
//  10. Set ESP-NOW state, call espNowSetState(ESPNOW_OFF)
//  11. Restart WiFi if necessary
//----------------------------------------------------------------------

//*********************************************************************
// Update the state of the ESP Now state machine
//
//      +--------------------+
//      |     ESPNOW_OFF     |
//      +--------------------+
//          |             |
//          |             | No pairs listed
//          |             V
//          |  +---------------------+
//          |  | ESPNOW_BROADCASTING |
//          |  +---------------------+
//          |             |
//          |             |
//          |             V
//          |  +--------------------+
//          |  |   ESPNOW_PAIRING   |
//          |  +--------------------+
//          |             |
//          |             |
//          |             V
//          |  +--------------------+
//          |  | ESPNOW_MAC_RECEIVED |
//          |  +--------------------+
//          |             |
//          |             |
//          |             V
//          |  +--------------------+
//          '->|   ESPNOW_PAIRED    |
//             +--------------------+
//
// Send RTCM in either ESPNOW_BROADCASTING or ESPNOW_PAIRED state.
// Receive RTCM in ESPNOW_BROADCASTING, ESPNOW_MAC_RECEIVED and
// ESPNOW_PAIRED states.
//*********************************************************************

//*********************************************************************
// Regularly call during pairing to see if we've received a Pairing message
bool espNowProcessRxPairedMessage()
{
    if (espNowState == ESPNOW_MAC_RECEIVED)
    {
        // Remove broadcast peer
        espnowRemovePeer(espNowBroadcastAddr);

        if (esp_now_is_peer_exist(receivedMAC) == true)
        {
            if (settings.debugEspNow == true)
                systemPrintln("Peer already exists");
        }
        else
        {
            // Add new peer to system
            espNowAddPeer(receivedMAC);

            // Record this MAC to peer list
            memcpy(settings.espnowPeers[settings.espnowPeerCount], receivedMAC, 6);
            settings.espnowPeerCount++;
            settings.espnowPeerCount %= ESPNOW_MAX_PEERS;
        }

        // Send message directly to the received MAC (not unicast), then exit
        espNowSendPairMessage(receivedMAC);

        // Enable radio. User may have arrived here from the setup menu rather than serial menu.
        settings.enableEspNow = true;

        recordSystemSettings(); // Record enableEspNow and espnowPeerCount to NVM

        espNowSetState(ESPNOW_PAIRED);
        return (true);
    }
    return (false);
}

//*********************************************************************
// Callback when data is received
void espNowRxHandler(const esp_now_recv_info *mac,
                     const uint8_t *incomingData,
                     int len)
{
//    typedef struct esp_now_recv_info {
//        uint8_t * src_addr;             // Source address of ESPNOW packet
//        uint8_t * des_addr;             // Destination address of ESPNOW packet
//        wifi_pkt_rx_ctrl_t * rx_ctrl;   // Rx control info of ESPNOW packet
//    } esp_now_recv_info_t;

    uint8_t receivedMAC[6];

    if (espNowState == ESPNOW_PAIRING)
    {
        ESP_NOW_PAIR_MESSAGE pairMessage;
        if (len == sizeof(pairMessage)) // First error check
        {
            memcpy(&pairMessage, incomingData, len);

            // Check CRC
            uint8_t tempCRC = 0;
            for (int x = 0; x < 6; x++)
                tempCRC += pairMessage.macAddress[x];

            if (tempCRC == pairMessage.crc) // 2nd error check
            {
                memcpy(&receivedMAC, pairMessage.macAddress, 6);
                espNowSetState(ESPNOW_MAC_RECEIVED);
            }
            // else Pair CRC failed
        }
    }
    else if (settings.debugEspNow)
        systemPrintf("*** ESP-NOW: RX %02x:%02x:%02x:%02x:%02x:%02x --> %02x:%02x:%02x:%02x:%02x:%02x, %d bytes, rssi: %d\r\n",
                     mac->src_addr[0], mac->src_addr[1], mac->src_addr[2],
                     mac->src_addr[3], mac->src_addr[4], mac->src_addr[5],
                     mac->des_addr[0], mac->des_addr[1], mac->des_addr[2],
                     mac->des_addr[3], mac->des_addr[4], mac->des_addr[5],
                     len, packetRSSI);
}

//*********************************************************************
// Update the state of the ESP-NOW subsystem
void espNowSetState(ESPNOWState newState)
{
    const char * name[] =
    {
        "ESPNOW_OFF",
        "ESPNOW_BROADCASTING",
        "ESPNOW_PAIRING",
        "ESPNOW_MAC_RECEIVED",
        "ESPNOW_PAIRED",
    };
    const int nameCount = sizeof(name) / sizeof(name[0]);
    const char * newName;
    char nLine[80];
    const char * oldName;
    char oLine[80];

    if (settings.debugEspNow == true)
    {
        // Get the old state name
        if (espNowState < ESPNOW_MAX)
            oldName = name[espNowState];
        else
        {
            sprintf(oLine, "Unknown state: %d", espNowState);
            oldName = &oLine[0];
        }

        // Get the new state name
        if (newState < ESPNOW_MAX)
            newName = name[newState];
        else
        {
            sprintf(nLine, "Unknown state: %d", newState);
            newName = &nLine[0];
        }

        // Display the state change
        if (newState == espNowState)
            systemPrintf("ESP-NOW: *%s\r\n", newName);
        else
            systemPrintf("ESP-NOW: %s --> %s\r\n", oldName, newName);
    }
    espNowState = newState;
}

//*********************************************************************
// Create special pair packet to a given MAC
esp_err_t espNowSendPairMessage(const uint8_t *sendToMac)
{
    // Assemble message to send
    ESP_NOW_PAIR_MESSAGE pairMessage;

    // Get unit MAC address
    memcpy(pairMessage.macAddress, wifiMACAddress, 6);
    pairMessage.encrypt = false;
    pairMessage.channel = 0;

    pairMessage.crc = 0; // Calculate CRC
    for (int x = 0; x < 6; x++)
        pairMessage.crc += wifiMACAddress[x];

    return (esp_now_send(sendToMac, (uint8_t *)&pairMessage, sizeof(pairMessage))); // Send packet to given MAC
}

//*********************************************************************
// Start ESP-NOW layer
bool espNowStart()
{
    int index;
    bool started;
    esp_err_t status;

    do
    {
        started = false;

        //   5. Call esp_now_init
        if (settings.debugEspNow)
            systemPrintf("Calling esp_now_init\r\n");
        status = esp_now_init();
        if (status != ESP_OK)
        {
            systemPrintf("ERROR: Failed to initialize ESP-NOW, status: %d\r\n", status);
            break;
        }

        //   9. Set receive callback [esp_now_register_recv_cb(espNowOnDataReceived)]
        if (settings.debugEspNow)
            systemPrintf("Calling esp_now_register_recv_cb\r\n");
        status = esp_now_register_recv_cb(espNowRxHandler);
        if (status != ESP_OK)
        {
            systemPrintf("ERROR: Failed to set ESP_NOW RX callback, status: %d\r\n", status);
            break;
        }

        //  10. Add peers from settings
        //      i.  Set ESP-NOW state, call espNowSetState(ESPNOW_PAIRED)
        if (settings.debugEspNow)
            systemPrintf("Calling espNowSetState\r\n");
        espNowSetState(ESPNOW_PAIRED);

        //     ii. Loop through peers listed in settings, for each
        for (index = 0; index < ESPNOW_MAX_PEERS; index++)
        {
            //      a. Determine if peer exists, call esp_now_is_peer_exist
            if (settings.debugEspNow)
                systemPrintf("Calling esp_now_is_peer_exist\r\n");
            if (esp_now_is_peer_exist(settings.espnowPeers[index]) == false)
            {
                //  b. Add peer if necessary, call espNowAddPeer
                if (settings.debugEspNow)
                    systemPrintf("Calling espNowAddPeer\r\n");
                status = espNowAddPeer(&settings.espnowPeers[index][0]);
                if (status != ESP_OK)
                {
                    systemPrintf("ERROR: Failed to add ESP-NOW peer %02x:%02x:%02x:%02x:%02x:%02x, status: %d\r\n",
                                 settings.espnowPeers[index][0],
                                 settings.espnowPeers[index][1],
                                 settings.espnowPeers[index][2],
                                 settings.espnowPeers[index][3],
                                 settings.espnowPeers[index][4],
                                 settings.espnowPeers[index][5],
                                 status);
                    break;
                }
            }

            // Determine if an error occurred
            if (index < ESPNOW_MAX_PEERS)
                break;
        }

        // ESP-NOW has started successfully
        if (settings.debugEspNow)
            systemPrintf("ESP-NOW online\r\n");

        started = true;
    } while (0);

    return started;
}

//*********************************************************************
// A blocking function that is used to pair two devices
// either through the serial menu or AP config
void espNowStaticPairing()
{
    systemPrintln("Begin ESP NOW Pairing");

    // Start ESP-Now if needed, put ESP-Now into broadcast state
    espNowBeginPairing();

    // Begin sending our MAC every 250ms until a remote device sends us there info
    randomSeed(millis());

    systemPrintln("Begin pairing. Place other unit in pairing mode. Press any key to exit.");
    clearBuffer();

    bool exitPair = false;
    while (exitPair == false)
    {
        if (systemAvailable())
        {
            systemPrintln("User pressed button. Pairing canceled.");
            break;
        }

        int timeout = 1000 + random(0, 100); // Delay 1000 to 1100ms
        for (int x = 0; x < timeout; x++)
        {
            delay(1);

            if (espNowProcessRxPairedMessage() == true) // Check if we've received a pairing message
            {
                systemPrintln("Pairing compete");
                exitPair = true;
                break;
            }
        }

        espNowSendPairMessage(); // Send unit's MAC address over broadcast, no ack, no encryption

        systemPrintln("Scanning for other radio...");
    }
}

//*********************************************************************
// Stop ESP-NOW layer
bool espNowStop()
{
    uint8_t mode;
    esp_now_peer_num_t peerCount;
    uint8_t primaryChannel;
    uint8_t protocols;
    wifi_second_chan_t secondaryChannel;
    bool stopped;
    esp_err_t status;

    do
    {
        stopped = false;

        //   3. esp_now_unregister_recv_cb()
        if (settings.debugEspNow)
            systemPrintf("Calling esp_now_unregister_recv_cb\r\n");
        status = esp_now_unregister_recv_cb();
        if (status != ESP_OK)
        {
            systemPrintf("ERROR: Failed to clear ESP_NOW RX callback, status: %d\r\n", status);
            break;
        }
        if (settings.debugEspNow)
            systemPrintf("ESP-NOW: RX callback removed\r\n");

        if (settings.debugEspNow)
            systemPrintf("ESP-NOW offline\r\n");

        //   4. Remove all peers by calling espnowRemovePeer
        if (settings.debugEspNow)
            systemPrintf("Calling esp_now_is_peer_exist\r\n");
        if (esp_now_is_peer_exist(espNowBroadcastAddr))
        {
            if (settings.debugEspNow)
                systemPrintf("Calling esp_now_del_peer\r\n");
            status = esp_now_del_peer(espNowBroadcastAddr);
            if (status != ESP_OK)
            {
                systemPrintf("ERROR: Failed to delete broadcast peer, status: %d\r\n", status);
                break;
            }
            if (settings.debugEspNow)
                systemPrintf("ESP-NOW removed broadcast peer\r\n");
        }

        // Walk the unicast peers
        while (1)
        {
            esp_now_peer_info_t peerInfo;

            // Get the next unicast peer
            if (settings.debugEspNow)
                systemPrintf("Calling esp_now_fetch_peer\r\n");
            status = esp_now_fetch_peer(true, &peerInfo);
            if (status != ESP_OK)
                break;

            // Remove the unicast peer
            if (settings.debugEspNow)
                systemPrintf("Calling esp_now_del_peer\r\n");
            status = esp_now_del_peer(peerInfo.peer_addr);
            if (status != ESP_OK)
            {
                systemPrintf("ERROR: Failed to delete peer %02x:%02x:%02x:%02x:%02x:%02x, status: %d\r\n",
                             peerInfo.peer_addr[0], peerInfo.peer_addr[1],
                             peerInfo.peer_addr[2], peerInfo.peer_addr[3],
                             peerInfo.peer_addr[4], peerInfo.peer_addr[5],
                             status);
                break;
            }
            if (settings.debugEspNow)
                systemPrintf("ESP-NOW removed peer %02x:%02x:%02x:%02x:%02x:%02x\r\n",
                             peerInfo.peer_addr[0], peerInfo.peer_addr[1],
                             peerInfo.peer_addr[2], peerInfo.peer_addr[3],
                             peerInfo.peer_addr[4], peerInfo.peer_addr[5]);
        }
        if (status != ESP_ERR_ESPNOW_NOT_FOUND)
        {
            systemPrintf("ERROR: Failed to get peer info for deletion, status: %d\r\n", status);
            break;
        }

        // Get the number of peers
        if (settings.debugEspNow)
        {
            systemPrintf("Calling esp_now_get_peer_num\r\n");
            status = esp_now_get_peer_num(&peerCount);
            if (status != ESP_OK)
            {
                systemPrintf("ERROR: Failed to get the peer count, status: %d\r\n", status);
                break;
            }
            systemPrintf("peerCount: %d\r\n", peerCount);
        }

        // Stop the ESP-NOW state machine
        espNowSetState(ESPNOW_OFF);

        //   9. Turn off ESP-NOW. call esp_now_deinit
        if (settings.debugEspNow)
            systemPrintf("Calling esp_now_deinit\r\n");
        status = esp_now_deinit();
        if (status != ESP_OK)
        {
            systemPrintf("ERROR: Failed to deinit ESP-NOW, status: %d\r\n", status);
            break;
        }

        //  11. Restart WiFi if necessary

        // ESP-NOW has stopped successfully
        if (settings.debugEspNow)
            systemPrintf("ESP-NOW stopped\r\n");
        stopped = true;
    } while (0);

    // Return the stopped status
    return stopped;
}

//*********************************************************************
// Called from main loop
// Control incoming/outgoing RTCM data from internal ESP NOW radio
// Use the ESP32 to directly transmit/receive RTCM over 2.4GHz (no WiFi needed)
void espNowUpdate()
{
    if (settings.enableEspNow == true)
    {
        if (espNowState == ESPNOW_PAIRED || espNowState == ESPNOW_BROADCASTING)
        {
            // If it's been longer than a few ms since we last added a byte to the buffer
            // then we've reached the end of the RTCM stream. Send partial buffer.
            if (espnowOutgoingSpot > 0 && (millis() - espnowLastAdd) > 50)
            {
                if (espNowState == ESPNOW_PAIRED)
                    esp_now_send(0, (uint8_t *)&espnowOutgoing, espnowOutgoingSpot); // Send partial packet to all peers
                else // if (espNowState == ESPNOW_BROADCASTING)
                {
                    esp_now_send(espNowBroadcastAddr, (uint8_t *)&espnowOutgoing,
                                 espnowOutgoingSpot); // Send packet via broadcast
                }

                if (!inMainMenu)
                {
                    if (settings.debugEspNow == true)
                        systemPrintf("ESPNOW transmitted %d RTCM bytes\r\n", espnowBytesSent + espnowOutgoingSpot);
                }
                espnowBytesSent = 0;
                espnowOutgoingSpot = 0; // Reset
            }

            // If we don't receive an ESP NOW packet after some time, set RSSI to very negative
            // This removes the ESPNOW icon from the display when the link goes down
            if (millis() - lastEspnowRssiUpdate > 5000 && espnowRSSI > -255)
                espnowRSSI = -255;
        }
    }
}

/*
  Use ESP NOW protocol to transmit RTCM between RTK Products via 2.4GHz

  How pairing works:
    1. Device enters pairing mode
    2. Device adds the broadcast MAC (all 0xFFs) as peer
    3. Device waits for incoming pairing packet from remote
    4. If valid pairing packet received, add peer, immediately transmit a pairing packet to that peer and exit.

    ESP NOW is bare metal, there is no guaranteed packet delivery. For RTCM byte transmissions using ESP NOW:
      We don't care about dropped packets or packets out of order. The ZED will check the integrity of the RTCM packet.
      We don't care if the ESP NOW packet is corrupt or not. RTCM has its own CRC. RTK needs valid RTCM once every
      few seconds so a single dropped frame is not critical.
*/

// Called from main loop
// Control incoming/outgoing RTCM data from internal ESP NOW radio
// Use the ESP32 to directly transmit/receive RTCM over 2.4GHz (no WiFi needed)
void updateEspnow()
{
    if (settings.enableEspNow == true)
    {
        if (espNowState == ESPNOW_PAIRED || espNowState == ESPNOW_BROADCASTING)
        {
            // If it's been longer than a few ms since we last added a byte to the buffer
            // then we've reached the end of the RTCM stream. Send partial buffer.
            if (espnowOutgoingSpot > 0 && (millis() - espnowLastAdd) > 50)
            {
                if (espNowState == ESPNOW_PAIRED)
                    esp_now_send(0, (uint8_t *)&espnowOutgoing, espnowOutgoingSpot); // Send partial packet to all peers
                else // if (espNowState == ESPNOW_BROADCASTING)
                {
                    esp_now_send(espNowBroadcastAddr, (uint8_t *)&espnowOutgoing,
                                 espnowOutgoingSpot); // Send packet via broadcast
                }

                if (!inMainMenu)
                {
                    if (settings.debugEspNow == true)
                        systemPrintf("ESPNOW transmitted %d RTCM bytes\r\n", espnowBytesSent + espnowOutgoingSpot);
                }
                espnowBytesSent = 0;
                espnowOutgoingSpot = 0; // Reset
            }

            // If we don't receive an ESP NOW packet after some time, set RSSI to very negative
            // This removes the ESPNOW icon from the display when the link goes down
            if (millis() - lastEspnowRssiUpdate > 5000 && espnowRSSI > -255)
                espnowRSSI = -255;
        }
    }
}

// Callback when data is sent
void espnowOnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    //  systemPrint("Last Packet Send Status: ");
    //  if (status == ESPNOW_SEND_SUCCESS)
    //    systemPrintln("Delivery Success");
    //  else
    //    systemPrintln("Delivery Fail");
}

// Callback for all RX Packets
// Get RSSI of all incoming management packets: https://esp32.com/viewtopic.php?t=13889
#ifdef COMPILE_ESPNOW
void promiscuous_rx_cb(void *buf, wifi_promiscuous_pkt_type_t type)
{
    // All espnow traffic uses action frames which are a subtype of the mgmnt frames so filter out everything else.
    if (type != WIFI_PKT_MGMT)
        return;

    const wifi_promiscuous_pkt_t *ppkt = (wifi_promiscuous_pkt_t *)buf;
    packetRSSI = ppkt->rx_ctrl.rssi;
}
#endif // COMPILE_ESPNOW

// If WiFi is already enabled, simply add the LR protocol
// If the radio is off entirely, start the radio, turn on only the LR protocol
void espnowStart()
{
    if (settings.enableEspNow == false)
    {
        espnowStop();
        return;
    }

    // Before we can issue esp_wifi_() commands WiFi must be started
    if (WiFi.getMode() != WIFI_STA)
        WiFi.mode(WIFI_STA);

    // Verify that the necessary protocols are set
    uint8_t protocols = 0;
    esp_err_t response = esp_wifi_get_protocol(WIFI_IF_STA, &protocols);
    if (response != ESP_OK)
        systemPrintf("espnowStart: Failed to get protocols: %s\r\n", esp_err_to_name(response));

    if ((WIFI_IS_RUNNING() == false) && espNowState == ESPNOW_OFF)
    {
        // Radio is off, turn it on
        if (protocols != (WIFI_PROTOCOL_LR))
        {
            response = esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR); // Stops WiFi Station.
            if (response != ESP_OK)
                systemPrintf("espnowStart: Error setting ESP-Now lone protocol: %s\r\n", esp_err_to_name(response));
            else
            {
                if (settings.debugEspNow == true)
                    systemPrintln("WiFi off, ESP-Now added to protocols");
            }
        }
        else
        {
            if (settings.debugEspNow == true)
                systemPrintln("LR protocol already in place");
        }
    }
    // If WiFi is on but ESP NOW is off, then enable LR protocol
    else if (WIFI_IS_RUNNING() && espNowState == ESPNOW_OFF)
    {
        // Enable WiFi + ESP-Now
        // Enable long range, PHY rate of ESP32 will be 512Kbps or 256Kbps
        if (protocols != (WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR))
        {
            response = esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N |
                                                              WIFI_PROTOCOL_LR);
            if (response != ESP_OK)
                systemPrintf("espnowStart: Error setting ESP-Now + WiFi protocols: %s\r\n", esp_err_to_name(response));
            else
            {
                if (settings.debugEspNow == true)
                    systemPrintln("WiFi on, ESP-Now added to protocols");
            }
        }
        else
        {
            if (settings.debugEspNow == true)
                systemPrintln("WiFi was already on, and LR protocol already in place");
        }
    }

    // If ESP-Now is already active, do nothing
    else
    {
        if (settings.debugEspNow == true)
            systemPrintln("ESP-Now already on.");
    }

    // WiFi.setSleep(false); //We must disable sleep so that ESP-NOW can readily receive packets
    // See: https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/issues/241

    // Init ESP-NOW
    if (esp_now_init() != ESP_OK)
    {
        systemPrintln("espnowStart: Error starting ESP-Now");
        return;
    }

    // Use promiscuous callback to capture RSSI of packet
    response = esp_wifi_set_promiscuous(true);
    if (response != ESP_OK)
        systemPrintf("espnowStart: Error setting promiscuous mode: %s\r\n", esp_err_to_name(response));

    esp_wifi_set_promiscuous_rx_cb(&promiscuous_rx_cb);

    // Assign channel if not connected to an AP
    if (WIFI_IS_CONNECTED() == false)
        espnowSetChannel(settings.wifiChannel);

    // Register callbacks
    // esp_now_register_send_cb(espnowOnDataSent);
    esp_now_register_recv_cb(espNowOnDataReceived);

    if (settings.espnowPeerCount == 0)
    {
        // Enter broadcast mode
        if (esp_now_is_peer_exist(espNowBroadcastAddr) == true)
        {
            if (settings.debugEspNow == true)
                systemPrintln("Broadcast peer already exists");
        }
        else
        {
            esp_err_t result = espNowAddPeer(espNowBroadcastAddr);
            if (result != ESP_OK)
            {
                if (settings.debugEspNow == true)
                    systemPrintln("Failed to add broadcast peer");
            }
            else
            {
                if (settings.debugEspNow == true)
                    systemPrintln("Broadcast peer added");
            }
        }

        espNowSetState(ESPNOW_BROADCASTING);
    }
    else
    {
        // If we have peers, move to paired state
        espNowSetState(ESPNOW_PAIRED);

        if (settings.debugEspNow == true)
            systemPrintf("Adding %d espnow peers\r\n", settings.espnowPeerCount);

        for (int x = 0; x < settings.espnowPeerCount; x++)
        {
            if (esp_now_is_peer_exist(settings.espnowPeers[x]) == true)
            {
                if (settings.debugEspNow == true)
                    systemPrintf("Peer #%d already exists\r\n", x);
            }
            else
            {
                esp_err_t result = espNowAddPeer(settings.espnowPeers[x]);
                if (result != ESP_OK)
                {
                    if (settings.debugEspNow == true)
                        systemPrintf("Failed to add peer #%d\r\n", x);
                }
            }
        }
    }

    systemPrintln("ESP-Now Started");
}

// If WiFi is already enabled, simply remove the LR protocol
// If WiFi is off, stop the radio entirely
void espnowStop()
{
    if (espNowState == ESPNOW_OFF)
        return;

    // Turn off promiscuous WiFi mode
    esp_err_t response = esp_wifi_set_promiscuous(false);
    if (response != ESP_OK)
        systemPrintf("espnowStop: Failed to set promiscuous mode: %s\r\n", esp_err_to_name(response));

    esp_wifi_set_promiscuous_rx_cb(nullptr);

    // Deregister callbacks
    // esp_now_unregister_send_cb();
    response = esp_now_unregister_recv_cb();
    if (response != ESP_OK)
        systemPrintf("espnowStop: Failed to unregister receive callback: %s\r\n", esp_err_to_name(response));

    // Forget all ESP-Now Peers
    for (int x = 0; x < settings.espnowPeerCount; x++)
        espnowRemovePeer(settings.espnowPeers[x]);

    if (WiFi.getMode() != WIFI_STA)
        WiFi.mode(WIFI_STA);

    // Verify that the necessary protocols are set
    uint8_t protocols = 0;
    response = esp_wifi_get_protocol(WIFI_IF_STA, &protocols);
    if (response != ESP_OK)
        systemPrintf("espnowStop: Failed to get protocols: %s\r\n", esp_err_to_name(response));

    // Leave WiFi with default settings (no WIFI_PROTOCOL_LR for ESP NOW)
    if (protocols != (WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N))
    {
        response = esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
        if (response != ESP_OK)
            systemPrintf("espnowStop: Error setting WiFi protocols: %s\r\n", esp_err_to_name(response));
        else
        {
            if (settings.debugEspNow == true)
                systemPrintln("espnowStop: WiFi on, ESP-Now removed from protocols");
        }
    }
    else
    {
        if (settings.debugEspNow == true)
            systemPrintln("espnowStop: ESP-Now already removed from protocols");
    }

    // Deinit ESP-NOW
    if (esp_now_deinit() != ESP_OK)
    {
        systemPrintln("Error deinitializing ESP-NOW");
        return;
    }

    espNowSetState(ESPNOW_OFF);

    if (WIFI_IS_RUNNING() == false)
    {
        // ESP-NOW was the only thing using the radio so turn WiFi radio off entirely
        WiFi.mode(WIFI_OFF);

        if (settings.debugEspNow == true)
            systemPrintln("WiFi Radio off entirely");
    }
    // If WiFi is on, then restart WiFi
    else if (WIFI_IS_RUNNING())
    {
        if (settings.debugEspNow == true)
            systemPrintln("ESP-Now starting WiFi");
        wifiStart(); // Force WiFi to restart
    }
}

// Returns the current channel being used by WiFi
uint8_t espnowGetChannel()
{
    if (espNowState == ESPNOW_OFF)
        return 0;

    bool originalPromiscuousMode = false;
    esp_err_t response = esp_wifi_get_promiscuous(&originalPromiscuousMode);
    if (response != ESP_OK)
        systemPrintf("getPromiscuous failed: %s\r\n", esp_err_to_name(response));

    // We must be in promiscuous mode to get the channel number
    if (originalPromiscuousMode == false)
        esp_wifi_set_promiscuous(true);

    uint8_t primaryChannelNumber = 0;
    wifi_second_chan_t secondaryChannelNumber;

    response = esp_wifi_get_channel(&primaryChannelNumber, &secondaryChannelNumber);
    if (response != ESP_OK)
        systemPrintf("getChannel failed: %s\r\n", esp_err_to_name(response));

    // Return to the original mode
    if (originalPromiscuousMode == false)
        esp_wifi_set_promiscuous(false);

    return (primaryChannelNumber);
}

// Returns the current channel being used by WiFi
bool espnowSetChannel(uint8_t channelNumber)
{
    if (WIFI_IS_CONNECTED() == true)
    {
        systemPrintln("ESP-NOW channel can't be modified while WiFi is connected.");
        return (false);
    }

    bool originalPromiscuousMode = false;
    esp_err_t response = esp_wifi_get_promiscuous(&originalPromiscuousMode);
    if (response != ESP_OK)
        systemPrintf("getPromiscuous failed: %s\r\n", esp_err_to_name(response));

    // We must be in promiscuous mode to set the channel number
    if (originalPromiscuousMode == false)
        esp_wifi_set_promiscuous(true);

    bool setSuccess = false;
    response = esp_wifi_set_channel(channelNumber, WIFI_SECOND_CHAN_NONE);
    if (response != ESP_OK)
        systemPrintf("setChannel to %d failed: %s\r\n", channelNumber, esp_err_to_name(response));
    else
        setSuccess = true;

    // Return to the original mode
    if (originalPromiscuousMode == false)
        esp_wifi_set_promiscuous(false);

    return (setSuccess);
}

#endif  // COMPILE_ESPNOW
