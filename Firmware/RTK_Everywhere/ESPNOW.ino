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
#ifdef COMPILE_ESPNOW
    if (settings.enableEspNow == true)
    {
        if (espnowState == ESPNOW_PAIRED || espnowState == ESPNOW_BROADCASTING)
        {
            // If it's been longer than a few ms since we last added a byte to the buffer
            // then we've reached the end of the RTCM stream. Send partial buffer.
            if (espnowOutgoingSpot > 0 && (millis() - espnowLastAdd) > 50)
            {
                if (espnowState == ESPNOW_PAIRED)
                    esp_now_send(0, (uint8_t *)&espnowOutgoing, espnowOutgoingSpot); // Send partial packet to all peers
                else // if (espnowState == ESPNOW_BROADCASTING)
                {
                    uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
                    esp_now_send(broadcastMac, (uint8_t *)&espnowOutgoing,
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
#endif // COMPILE_ESPNOW
}

// Create a struct for ESP NOW pairing
typedef struct PairMessage
{
    uint8_t macAddress[6];
    bool encrypt;
    uint8_t channel;
    uint8_t crc; // Simple check - add MAC together and limit to 8 bit
} PairMessage;

// Callback when data is sent
#ifdef COMPILE_ESPNOW
void espnowOnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    //  systemPrint("Last Packet Send Status: ");
    //  if (status == ESP_NOW_SEND_SUCCESS)
    //    systemPrintln("Delivery Success");
    //  else
    //    systemPrintln("Delivery Fail");
}
#endif // COMPILE_ESPNOW

#ifndef COMPILE_ESPNOW
#define esp_now_recv_info uint8_t
#endif

// Callback when data is received
void espnowOnDataReceived(const esp_now_recv_info *mac, const uint8_t *incomingData, int len)
{
#ifdef COMPILE_ESPNOW
    if (espnowState == ESPNOW_PAIRING)
    {
        if (len == sizeof(PairMessage)) // First error check
        {
            PairMessage pairMessage;
            memcpy(&pairMessage, incomingData, sizeof(pairMessage));

            // Check CRC
            uint8_t tempCRC = 0;
            for (int x = 0; x < 6; x++)
                tempCRC += pairMessage.macAddress[x];

            if (tempCRC == pairMessage.crc) // 2nd error check
            {
                memcpy(&receivedMAC, pairMessage.macAddress, 6);
                espnowSetState(ESPNOW_MAC_RECEIVED);
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
#endif // COMPILE_ESPNOW
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

#ifdef COMPILE_ESPNOW

    // Before we can issue esp_wifi_() commands WiFi must be started
    if (WiFi.getMode() != WIFI_STA)
        WiFi.mode(WIFI_STA);

    // Verify that the necessary protocols are set
    uint8_t protocols = 0;
    esp_err_t response = esp_wifi_get_protocol(WIFI_IF_STA, &protocols);
    if (response != ESP_OK)
        systemPrintf("espnowStart: Failed to get protocols: %s\r\n", esp_err_to_name(response));

    if ((wifiIsRunning() == false) && espnowState == ESPNOW_OFF)
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
    else if (wifiIsRunning() && espnowState == ESPNOW_OFF)
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
    if (wifiIsConnected() == false)
        espnowSetChannel(settings.wifiChannel);

    // Register callbacks
    // esp_now_register_send_cb(espnowOnDataSent);
    esp_now_register_recv_cb(espnowOnDataReceived);

    if (settings.espnowPeerCount == 0)
    {
        // Enter broadcast mode

        uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

        if (esp_now_is_peer_exist(broadcastMac) == true)
        {
            if (settings.debugEspNow == true)
                systemPrintln("Broadcast peer already exists");
        }
        else
        {
            esp_err_t result = espnowAddPeer(broadcastMac, false); // Encryption not support for broadcast MAC
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

        espnowSetState(ESPNOW_BROADCASTING);
    }
    else
    {
        // If we have peers, move to paired state
        espnowSetState(ESPNOW_PAIRED);

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
                esp_err_t result = espnowAddPeer(settings.espnowPeers[x]);
                if (result != ESP_OK)
                {
                    if (settings.debugEspNow == true)
                        systemPrintf("Failed to add peer #%d\r\n", x);
                }
            }
        }
    }

    systemPrintln("ESP-Now Started");
#endif // COMPILE_ESPNOW
}

// If WiFi is already enabled, simply remove the LR protocol
// If WiFi is off, stop the radio entirely
void espnowStop()
{
#ifdef COMPILE_ESPNOW
    if (espnowState == ESPNOW_OFF)
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

    espnowSetState(ESPNOW_OFF);

    if (wifiIsRunning() == false)
    {
        // ESP-NOW was the only thing using the radio so turn WiFi radio off entirely
        WiFi.mode(WIFI_OFF);

        if (settings.debugEspNow == true)
            systemPrintln("WiFi Radio off entirely");
    }
    // If WiFi is on, then restart WiFi
    else if (wifiIsRunning())
    {
        if (settings.debugEspNow == true)
            systemPrintln("ESP-Now starting WiFi");
        wifiStart(); // Force WiFi to restart
    }

#endif // COMPILE_ESPNOW
}

// Start ESP-Now if needed, put ESP-Now into broadcast state
void espnowBeginPairing()
{
    espnowStart();

    // To begin pairing, we must add the broadcast MAC to the peer list
    uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    espnowAddPeer(broadcastMac, false); // Encryption is not supported for multicast addresses

    espnowSetState(ESPNOW_PAIRING);
}

// Regularly call during pairing to see if we've received a Pairing message
bool espnowIsPaired()
{
#ifdef COMPILE_ESPNOW
    if (espnowState == ESPNOW_MAC_RECEIVED)
    {
        // Remove broadcast peer
        uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        espnowRemovePeer(broadcastMac);

        if (esp_now_is_peer_exist(receivedMAC) == true)
        {
            if (settings.debugEspNow == true)
                systemPrintln("Peer already exists");
        }
        else
        {
            // Add new peer to system
            espnowAddPeer(receivedMAC);

            // Record this MAC to peer list
            memcpy(settings.espnowPeers[settings.espnowPeerCount], receivedMAC, 6);
            settings.espnowPeerCount++;
            settings.espnowPeerCount %= ESPNOW_MAX_PEERS;
        }

        // Send message directly to the received MAC (not unicast), then exit
        espnowSendPairMessage(receivedMAC);

        // Enable radio. User may have arrived here from the setup menu rather than serial menu.
        settings.enableEspNow = true;

        recordSystemSettings(); // Record enableEspNow and espnowPeerCount to NVM

        espnowSetState(ESPNOW_PAIRED);
        return (true);
    }
#endif // COMPILE_ESPNOW
    return (false);
}

// Create special pair packet to a given MAC
esp_err_t espnowSendPairMessage(uint8_t *sendToMac)
{
#ifdef COMPILE_ESPNOW
    // Assemble message to send
    PairMessage pairMessage;

    // Get unit MAC address
    memcpy(pairMessage.macAddress, wifiMACAddress, 6);
    pairMessage.encrypt = false;
    pairMessage.channel = 0;

    pairMessage.crc = 0; // Calculate CRC
    for (int x = 0; x < 6; x++)
        pairMessage.crc += wifiMACAddress[x];

    return (esp_now_send(sendToMac, (uint8_t *)&pairMessage, sizeof(pairMessage))); // Send packet to given MAC
#else                                                                               // COMPILE_ESPNOW
    return (ESP_OK);
#endif                                                                              // COMPILE_ESPNOW
}

// Add a given MAC address to the peer list
esp_err_t espnowAddPeer(uint8_t *peerMac)
{
    return (espnowAddPeer(peerMac, true)); // Encrypt by default
}

esp_err_t espnowAddPeer(uint8_t *peerMac, bool encrypt)
{
#ifdef COMPILE_ESPNOW
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
        if (settings.debugEspNow == true)
            systemPrintf("Failed to add peer: 0x%02X%02X%02X%02X%02X%02X\r\n", peerMac[0], peerMac[1], peerMac[2],
                         peerMac[3], peerMac[4], peerMac[5]);
    }
    return (result);
#else  // COMPILE_ESPNOW
    return (ESP_OK);
#endif // COMPILE_ESPNOW
}

// Remove a given MAC address from the peer list
esp_err_t espnowRemovePeer(uint8_t *peerMac)
{
#ifdef COMPILE_ESPNOW
    esp_err_t response = esp_now_del_peer(peerMac);
    if (response != ESP_OK)
    {
        if (settings.debugEspNow == true)
            systemPrintf("Failed to remove peer: %s\r\n", esp_err_to_name(response));
    }

    return (response);
#else  // COMPILE_ESPNOW
    return (ESP_OK);
#endif // COMPILE_ESPNOW
}

// Update the state of the ESP Now state machine
void espnowSetState(ESPNOWState newState)
{
    if (espnowState == newState && settings.debugEspNow == true)
        systemPrint("*");
    espnowState = newState;

    if (settings.debugEspNow == true)
    {
        systemPrint("espnowState: ");
        switch (newState)
        {
        case ESPNOW_OFF:
            systemPrintln("ESPNOW_OFF");
            break;
        case ESPNOW_BROADCASTING:
            systemPrintln("ESPNOW_BROADCASTING");
            break;
        case ESPNOW_PAIRING:
            systemPrintln("ESPNOW_PAIRING");
            break;
        case ESPNOW_MAC_RECEIVED:
            systemPrintln("ESPNOW_MAC_RECEIVED");
            break;
        case ESPNOW_PAIRED:
            systemPrintln("ESPNOW_PAIRED");
            break;
        default:
            systemPrintf("Unknown ESPNOW state: %d\r\n", newState);
            break;
        }
    }
}

void espnowProcessRTCM(byte incoming)
{
#ifdef COMPILE_ESPNOW
    // If we are paired,
    // Or if the radio is broadcasting
    // Then add bytes to the outgoing buffer
    if (espnowState == ESPNOW_PAIRED || espnowState == ESPNOW_BROADCASTING)
    {
        // Move this byte into ESP NOW to send buffer
        espnowOutgoing[espnowOutgoingSpot++] = incoming;
        espnowLastAdd = millis();
    }

    // Send buffer when full
    if (espnowOutgoingSpot == sizeof(espnowOutgoing))
    {
        espnowOutgoingSpot = 0; // Wrap

        if (espnowState == ESPNOW_PAIRED)
            esp_now_send(0, (uint8_t *)&espnowOutgoing, sizeof(espnowOutgoing)); // Send packet to all peers
        else // if (espnowState == ESPNOW_BROADCASTING)
        {
            uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
            esp_now_send(broadcastMac, (uint8_t *)&espnowOutgoing,
                         sizeof(espnowOutgoing)); // Send packet via broadcast
        }

        delay(10); // We need a small delay between sending multiple packets

        espnowBytesSent += sizeof(espnowOutgoing);

        espnowOutgoingRTCM = true;
    }
#endif // COMPILE_ESPNOW
}

// A blocking function that is used to pair two devices
// either through the serial menu or AP config
void espnowStaticPairing()
{
    systemPrintln("Begin ESP NOW Pairing");

    // Start ESP-Now if needed, put ESP-Now into broadcast state
    espnowBeginPairing();

    // Begin sending our MAC every 250ms until a remote device sends us there info
    randomSeed(millis());

    systemPrintln("Begin pairing. Place other unit in pairing mode. Press any key to exit.");
    clearBuffer();

    uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

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

            if (espnowIsPaired() == true) // Check if we've received a pairing message
            {
                systemPrintln("Pairing compete");
                exitPair = true;
                break;
            }
        }

        espnowSendPairMessage(broadcastMac); // Send unit's MAC address over broadcast, no ack, no encryption

        systemPrintln("Scanning for other radio...");
    }
}

// Returns the current channel being used by WiFi
uint8_t espnowGetChannel()
{
#ifdef COMPILE_ESPNOW
    if (espnowState == ESPNOW_OFF)
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
#else
    return (0);
#endif
}

// Returns the current channel being used by WiFi
bool espnowSetChannel(uint8_t channelNumber)
{
#ifdef COMPILE_ESPNOW
    if (wifiIsConnected() == true)
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
#else
    return (false);
#endif
}
