/**********************************************************************
  ESPNOW.ino

  Handle the ESP-NOW events
**********************************************************************/

#ifdef COMPILE_ESPNOW

//****************************************
// Constants
//****************************************

const uint8_t peerBroadcast[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

//****************************************
// Locals
//****************************************

bool espNowDebug;
bool espNowDisplay;
ESP_NOW_STATE espNowState;
bool espNowVerbose;

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
    if (espNowDebug)
        systemPrintf("Calling esp_now_add_peer\r\n");
    esp_err_t result = esp_now_add_peer(&peerInfo);
    if (result != ESP_OK)
    {
        systemPrintf("ERROR: Failed to add ESP-NOW peer %02x:%02x:%02x:%02x:%02x:%02x, result: %d\r\n",
                     peerBroadcast[0], peerBroadcast[1],
                     peerBroadcast[2], peerBroadcast[3],
                     peerBroadcast[4], peerBroadcast[5],
                     result);
    }
    else if (espNowDebug)
        systemPrintf("Added ESP-NOW peer %02x:%02x:%02x:%02x:%02x:%02x\r\n",
                     peerBroadcast[0], peerBroadcast[1],
                     peerBroadcast[2], peerBroadcast[3],
                     peerBroadcast[4], peerBroadcast[5]);
    return result;
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
//   9. Set receive callback [esp_now_register_recv_cb(espnowOnDataReceived)]
//  10. Add peers from settings
//      A. If no peers exist
//          i.   Determine if broadcast peer exists, call esp_now_is_peer_exist
//          ii.  Add broadcast peer if necessary, call espnowAddPeer
//          iii. Set ESP-NOW state, call espnowSetState(ESP_NOW_BROADCASTING)
//      B. If peers exist,
//          i.  Set ESP-NOW state, call espnowSetState(ESP_NOW_PAIRED)
//          ii. Loop through peers listed in settings, for each
//              a. Determine if peer exists, call esp_now_is_peer_exist
//              b. Add peer if necessary, call espnowAddPeer
//
// In espnowOnDataReceived
//  11. Save ESP-NOW RSSI
//  12. Set lastEspnowRssiUpdate = millis()
//  13. If in ESP_NOW_PAIRING state
//      A. Validate message CRC
//      B. If valid CRC
//          i.  Save peer MAC address
//          ii. espnowSetState(ESP_NOW_MAC_RECEIVED)
//  14. Else if ESP_NOW_MAC_RECEIVED state
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
//  10. Set ESP-NOW state, call espnowSetState(ESP_NOW_OFF)
//  11. Restart WiFi if necessary
//----------------------------------------------------------------------

//*********************************************************************
// Update the state of the ESP Now state machine
//
//      +---------------------+
//      |     ESP_NOW_OFF     |
//      +---------------------+
//          |             |
//          |             | No pairs listed
//          |             V
//          |  +----------------------+
//          |  | ESP_NOW_BROADCASTING |
//          |  +----------------------+
//          |             |
//          |             |
//          |             V
//          |  +---------------------+
//          |  |   ESP_NOW_PAIRING   |
//          |  +---------------------+
//          |             |
//          |             |
//          |             V
//          |  +---------------------+
//          |  | ESP_NOW_MAC_RECEIVED |
//          |  +---------------------+
//          |             |
//          |             |
//          |             V
//          |  +---------------------+
//          '->|   ESP_NOW_PAIRED    |
//             +---------------------+
//
// Send RTCM in either ESP_NOW_BROADCASTING or ESP_NOW_PAIRED state.
// Receive RTCM in ESP_NOW_BROADCASTING, ESP_NOW_MAC_RECEIVED and
// ESP_NOW_PAIRED states.
//*********************************************************************

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

    if (espNowState == ESP_NOW_PAIRING)
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
                espNowSetState(ESP_NOW_MAC_RECEIVED);
            }
            // else Pair CRC failed
        }
    }
    else
    {
        if (espNowDisplay == true)
            systemPrintf("*** ESP-NOW: RX %02x:%02x:%02x:%02x:%02x:%02x --> %02x:%02x:%02x:%02x:%02x:%02x, %d bytes, rssi: %d\r\n",
                         mac->src_addr[0], mac->src_addr[1], mac->src_addr[2],
                         mac->src_addr[3], mac->src_addr[4], mac->src_addr[5],
                         mac->des_addr[0], mac->des_addr[1], mac->des_addr[2],
                         mac->des_addr[3], mac->des_addr[4], mac->des_addr[5],
                         len, packetRSSI);
    }
}

//*********************************************************************
// Update the state of the ESP-NOW subsystem
void espNowSetState(ESP_NOW_STATE newState)
{
    const char * name[] =
    {
        "ESP_NOW_OFF",
        "ESP_NOW_BROADCASTING",
        "ESP_NOW_PAIRING",
        "ESP_NOW_MAC_RECEIVED",
        "ESP_NOW_PAIRED",
    };
    const int nameCount = sizeof(name) / sizeof(name[0]);
    const char * newName;
    char nLine[80];
    const char * oldName;
    char oLine[80];

    if (espNowDebug == true)
    {
        // Get the old state name
        if (espNowState < ESP_NOW_MAX)
            oldName = name[espNowState];
        else
        {
            sprintf(oLine, "Unknown state: %d", espNowState);
            oldName = &oLine[0];
        }

        // Get the new state name
        if (newState < ESP_NOW_MAX)
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
        if (espNowDebug && espNowVerbose)
            systemPrintf("Calling esp_now_init\r\n");
        status = esp_now_init();
        if (status != ESP_OK)
        {
            systemPrintf("ERROR: Failed to initialize ESP-NOW, status: %d\r\n", status);
            break;
        }

        //   9. Set receive callback [esp_now_register_recv_cb(espnowOnDataReceived)]
        if (espNowDebug && espNowVerbose)
            systemPrintf("Calling esp_now_register_recv_cb\r\n");
        status = esp_now_register_recv_cb(espNowRxHandler);
        if (status != ESP_OK)
        {
            systemPrintf("ERROR: Failed to set ESP_NOW RX callback, status: %d\r\n", status);
            break;
        }

        //  10. Add peers from settings
        //      i.  Set ESP-NOW state, call espNowSetState(ESP_NOW_PAIRED)
        if (espNowDebug && espNowVerbose)
            systemPrintf("Calling espNowSetState\r\n");
        espNowSetState(ESP_NOW_PAIRED);

        //     ii. Loop through peers listed in settings, for each
        for (index = 0; index < ESPNOW_MAX_PEERS; index++)
        {
            //      a. Determine if peer exists, call esp_now_is_peer_exist
            if (espNowDebug && espNowVerbose)
                systemPrintf("Calling esp_now_is_peer_exist\r\n");
            if (esp_now_is_peer_exist(settings.espnowPeers[index]) == false)
            {
                //  b. Add peer if necessary, call espnowAddPeer
                if (espNowDebug && espNowVerbose)
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
        if (espNowDebug)
            systemPrintf("ESP-NOW online\r\n");

        started = true;
    } while (0);

    return started;
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
        if (espNowDebug)
            systemPrintf("Calling esp_now_unregister_recv_cb\r\n");
        status = esp_now_unregister_recv_cb();
        if (status != ESP_OK)
        {
            systemPrintf("ERROR: Failed to clear ESP_NOW RX callback, status: %d\r\n", status);
            break;
        }
        if (espNowDebug && espNowVerbose)
            systemPrintf("ESP-NOW: RX callback removed\r\n");

        if (espNowDisplay)
            systemPrintf("ESP-NOW offline\r\n");

        //   4. Remove all peers by calling espnowRemovePeer
        if (espNowDebug && espNowVerbose)
            systemPrintf("Calling esp_now_is_peer_exist\r\n");
        if (esp_now_is_peer_exist(peerBroadcast))
        {
            if (espNowDebug && espNowVerbose)
                systemPrintf("Calling esp_now_del_peer\r\n");
            status = esp_now_del_peer(peerBroadcast);
            if (status != ESP_OK)
            {
                systemPrintf("ERROR: Failed to delete broadcast peer, status: %d\r\n", status);
                break;
            }
            if (espNowDebug && espNowVerbose)
                systemPrintf("ESP-NOW removed broadcast peer\r\n");
        }

        // Walk the unicast peers
        while (1)
        {
            esp_now_peer_info_t peerInfo;

            // Get the next unicast peer
            if (espNowDebug && espNowVerbose)
                systemPrintf("Calling esp_now_fetch_peer\r\n");
            status = esp_now_fetch_peer(true, &peerInfo);
            if (status != ESP_OK)
                break;

            // Remove the unicast peer
            if (espNowDebug && espNowVerbose)
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
            if (espNowDebug && espNowVerbose)
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
        if (espNowDebug && espNowVerbose)
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
        espNowSetState(ESP_NOW_OFF);

        //   9. Turn off ESP-NOW. call esp_now_deinit
        if (espNowDebug && espNowVerbose)
            systemPrintf("Calling esp_now_deinit\r\n");
        status = esp_now_deinit();
        if (status != ESP_OK)
        {
            systemPrintf("ERROR: Failed to deinit ESP-NOW, status: %d\r\n", status);
            break;
        }

        //  11. Restart WiFi if necessary

        // ESP-NOW has stopped successfully
        if (espNowDebug)
            systemPrintf("ESP-NOW stopped\r\n");
        stopped = true;
    } while (0);

    // Return the stopped status
    return stopped;
}

#endif  // COMPILE_ESPNOW
