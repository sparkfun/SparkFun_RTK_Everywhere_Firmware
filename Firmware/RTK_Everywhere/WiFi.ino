/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
  WiFi Status Values:
    WL_CONNECTED: assigned when connected to a WiFi network
    WL_CONNECTION_LOST: assigned when the connection is lost
    WL_CONNECT_FAILED: assigned when the connection fails for all the attempts
    WL_DISCONNECTED: assigned when disconnected from a network
    WL_IDLE_STATUS: it is a temporary status assigned when WiFi.begin() is called and
                    remains active until the number of attempts expires (resulting in
                    WL_CONNECT_FAILED) or a connection is established (resulting in
                    WL_CONNECTED)
    WL_NO_SHIELD: assigned when no WiFi shield is present
    WL_NO_SSID_AVAIL: assigned when no SSID are available
    WL_SCAN_COMPLETED: assigned when the scan networks is completed
  =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

//----------------------------------------
// Globals
//----------------------------------------

int wifiConnectionAttempts; // Count the number of connection attempts between restarts

bool restartWiFi = false; // Restart WiFi if user changes anything

#ifdef COMPILE_WIFI

WiFiMulti *wifiMulti;

//----------------------------------------
// Constants
//----------------------------------------

#define WIFI_MAX_TIMEOUT (15 * 60 * 1000)
#define WIFI_MIN_TIMEOUT (15 * 1000)

//----------------------------------------
// Locals
//----------------------------------------

static uint32_t wifiLastConnectionAttempt;

// Throttle the time between connection attempts
// ms - Max of 4,294,967,295 or 4.3M seconds or 71,000 minutes or 1193 hours or 49 days between attempts
static uint32_t wifiConnectionAttemptsTotal; // Count the number of connection attempts absolutely
static uint32_t wifiConnectionAttemptTimeout;

// Start timeout
static uint32_t wifiStartTimeout;

// WiFi Timer usage:
//  * Measure interval to display IP address
static unsigned long wifiDisplayTimer;

// DNS server for Captive Portal
static DNSServer dnsServer;

static bool wifiRunning;

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// WiFi Routines
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

//----------------------------------------
// Set WiFi credentials
// Enable TCP connections
//----------------------------------------
void menuWiFi()
{
    while (1)
    {
        networkDisplayInterface(NETWORK_WIFI);

        systemPrintln();
        systemPrintln("Menu: WiFi Networks");

        for (int x = 0; x < MAX_WIFI_NETWORKS; x++)
        {
            systemPrintf("%d) SSID %d: %s\r\n", (x * 2) + 1, x + 1, settings.wifiNetworks[x].ssid);
            systemPrintf("%d) Password %d: %s\r\n", (x * 2) + 2, x + 1, settings.wifiNetworks[x].password);
        }

        systemPrint("a) Configure device via WiFi Access Point or connect to WiFi: ");
        systemPrintf("%s\r\n", settings.wifiConfigOverAP ? "AP" : "WiFi");

        systemPrint("c) Captive Portal: ");
        systemPrintf("%s\r\n", settings.enableCaptivePortal ? "Enabled" : "Disabled");

        systemPrintln("x) Exit");

        byte incoming = getUserInputCharacterNumber();

        if (incoming >= 1 && incoming <= MAX_WIFI_NETWORKS * 2)
        {
            int arraySlot = ((incoming - 1) / 2); // Adjust incoming to array starting at 0

            if (incoming % 2 == 1)
            {
                systemPrintf("Enter SSID network %d: ", arraySlot + 1);
                getUserInputString(settings.wifiNetworks[arraySlot].ssid,
                                   sizeof(settings.wifiNetworks[arraySlot].ssid));
                restartWiFi = true; // If we are modifying the SSID table, force restart of WiFi
            }
            else
            {
                systemPrintf("Enter Password for %s: ", settings.wifiNetworks[arraySlot].ssid);
                getUserInputString(settings.wifiNetworks[arraySlot].password,
                                   sizeof(settings.wifiNetworks[arraySlot].password));
                restartWiFi = true; // If we are modifying the SSID table, force restart of WiFi
            }
        }
        else if (incoming == 'a')
        {
            settings.wifiConfigOverAP ^= 1;
            restartWiFi = true;
        }
        else if (incoming == 'c')
        {
            settings.enableCaptivePortal ^= 1;
        }
        else if (incoming == 'x')
            break;
        else if (incoming == INPUT_RESPONSE_GETCHARACTERNUMBER_EMPTY)
            break;
        else if (incoming == INPUT_RESPONSE_GETCHARACTERNUMBER_TIMEOUT)
            break;
        else
            printUnknown(incoming);
    }

    // Erase passwords from empty SSID entries
    for (int x = 0; x < MAX_WIFI_NETWORKS; x++)
    {
        if (strlen(settings.wifiNetworks[x].ssid) == 0)
            strcpy(settings.wifiNetworks[x].password, "");
    }

    clearBuffer(); // Empty buffer of any newline chars
}

//----------------------------------------
// Starts WiFi in STA, AP, or STA_AP mode depending on networkGetConsumerTypes()
// Returns true if STA connects, or if AP is started, or if STA_AP is successful
//----------------------------------------
bool wifiConnect(bool startWiFiStation, bool startWiFiAP, unsigned long timeout)
{
    // Is a change needed?
    if (startWiFiStation && startWiFiAP && WiFi.getMode() == WIFI_AP_STA)
        return (true); // There is nothing needing to be changed

    if (startWiFiStation && WiFi.getMode() == WIFI_STA)
        return (true); // There is nothing needing to be changed

    if (startWiFiAP && WiFi.getMode() == WIFI_AP)
        return (true); // There is nothing needing to be changed

    wifiRunning = false; //Mark it as offline while we mess about
    
    wifi_mode_t wifiMode = WIFI_OFF;
    wifi_interface_t wifiInterface = WIFI_IF_STA;

    if (networkGetConsumerTypes() == NETCONSUMER_WIFI_STA)
    {
        systemPrintln("Starting WiFi Station");
        wifiMode = WIFI_STA;
        wifiInterface = WIFI_IF_STA;
    }
    else if (networkGetConsumerTypes() == NETCONSUMER_WIFI_AP)
    {
        systemPrintln("Starting WiFi AP");
        wifiMode = WIFI_AP;
        wifiInterface = WIFI_IF_AP;
    }
    else if (networkGetConsumerTypes() == NETCONSUMER_WIFI_AP_STA)
    {
        systemPrintln("Starting WiFi AP+Station");
        wifiMode = WIFI_AP_STA;
        wifiInterface = WIFI_IF_AP_STA;
    }

    displayWiFiConnect();

    if (WiFi.mode(wifiMode) == false) // Start WiFi in the appropriate mode
    {
        systemPrintln("WiFi failed to set mode");
        return (false);
    }

    // Verify that the necessary protocols are set
    uint8_t protocols = 0;
    esp_err_t response = esp_wifi_get_protocol(wifiInterface, &protocols);
    if (response != ESP_OK)
        systemPrintf("Failed to get protocols: %s\r\n", esp_err_to_name(response));

    // If ESP-NOW is running, blend in ESP-NOW protocol.
    if (espnowState > ESPNOW_OFF)
    {
        if (protocols != (WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR))
        {
            esp_err_t response =
                esp_wifi_set_protocol(wifiInterface, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N |
                                                         WIFI_PROTOCOL_LR); // Enable WiFi + ESP-Now.
            if (response != ESP_OK)
                systemPrintf("Error setting WiFi + ESP-NOW protocols: %s\r\n", esp_err_to_name(response));
        }
    }
    else
    {
        // Make sure default WiFi protocols are in place
        if (protocols != (WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N))
        {
            esp_err_t response = esp_wifi_set_protocol(wifiInterface, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G |
                                                                          WIFI_PROTOCOL_11N); // Enable WiFi.
            if (response != ESP_OK)
                systemPrintf("Error setting WiFi protocols: %s\r\n", esp_err_to_name(response));
        }
    }

    // Start AP with fixed IP
    if (wifiMode == WIFI_AP || wifiMode == WIFI_AP_STA)
    {
        IPAddress local_IP(192, 168, 4, 1);
        IPAddress gateway(192, 168, 4, 1);
        IPAddress subnet(255, 255, 255, 0);

        WiFi.softAPConfig(local_IP, gateway, subnet);

        const char *softApSsid = "RTK Config";
        if (WiFi.softAP(softApSsid) == false) // Must be short enough to fit OLED Width
        {
            systemPrintln("WiFi AP failed to start");
            return (false);
        }
        systemPrintf("WiFi AP '%s' started with IP: ", softApSsid);
        systemPrintln(WiFi.softAPIP());

        // Start DNS Server
        if (dnsServer.start(53, "*", WiFi.softAPIP()) == false)
        {
            systemPrintln("WiFi DNS Server failed to start");
            return (false);
        }
        else
        {
            if (settings.debugWifiState == true)
                systemPrintln("DNS Server started");
        }

        // If we're only here to start the AP, then we're done
        if (wifiMode == WIFI_AP)
        {
            wifiRunning = true;
            return true;
        }
    }

    systemPrintln("Connecting to WiFi... ");

    if (wifiMulti == nullptr)
        wifiMulti = new WiFiMulti;

    // Load SSIDs
    wifiMulti->APlistClean();
    for (int x = 0; x < MAX_WIFI_NETWORKS; x++)
    {
        if (strlen(settings.wifiNetworks[x].ssid) > 0)
            wifiMulti->addAP((const char *)&settings.wifiNetworks[x].ssid,
                             (const char *)&settings.wifiNetworks[x].password);
    }

    int wifiStatus = wifiMulti->run(timeout);
    if (wifiStatus == WL_CONNECTED)
    {
        wifiRunning = true;
        return true;
    }
    if (wifiStatus == WL_DISCONNECTED)
        systemPrint("No friendly WiFi networks detected.\r\n");
    else
    {
        systemPrintf("WiFi failed to connect: error #%d - %s\r\n", wifiStatus, wifiPrintState((wl_status_t)wifiStatus));
    }
    wifiRunning = false;
    return false;
}

//----------------------------------------
// Display the WiFi state
//----------------------------------------
void wifiDisplayState()
{
    systemPrintf("WiFi: %s\r\n", networkIsInterfaceOnline(NETWORK_WIFI) ? "Online" : "Offline");
    systemPrintf("    MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\r\n", wifiMACAddress[0], wifiMACAddress[1],
                 wifiMACAddress[2], wifiMACAddress[3], wifiMACAddress[4], wifiMACAddress[5]);
    if (networkIsInterfaceOnline(NETWORK_WIFI))
    {
        // Get the DNS addresses
        IPAddress dns1 = WiFi.STA.dnsIP(0);
        IPAddress dns2 = WiFi.STA.dnsIP(1);
        IPAddress dns3 = WiFi.STA.dnsIP(2);

        // Get the WiFi status
        wl_status_t wifiStatus = WiFi.status();

        const char *wifiStatusString = wifiPrintState(wifiStatus);

        // Display the WiFi state
        systemPrintf("    SSID: %s\r\n", WiFi.STA.SSID());
        systemPrintf("    IP Address: %s\r\n", WiFi.STA.localIP().toString().c_str());
        systemPrintf("    Subnet Mask: %s\r\n", WiFi.STA.subnetMask().toString().c_str());
        systemPrintf("    Gateway Address: %s\r\n", WiFi.STA.gatewayIP().toString().c_str());
        if ((uint32_t)dns3)
            systemPrintf("    DNS Address: %s, %s, %s\r\n", dns1.toString().c_str(), dns2.toString().c_str(),
                         dns3.toString().c_str());
        else if ((uint32_t)dns3)
            systemPrintf("    DNS Address: %s, %s\r\n", dns1.toString().c_str(), dns2.toString().c_str());
        else
            systemPrintf("    DNS Address: %s\r\n", dns1.toString().c_str());
        systemPrintf("    WiFi Strength: %d dBm\r\n", WiFi.RSSI());
        systemPrintf("    WiFi Status: %d\r\n", wifiStatusString);
    }
}

//----------------------------------------
// Process the WiFi events
//----------------------------------------
void wifiEvent(arduino_event_id_t event, arduino_event_info_t info)
{
    char ssid[sizeof(info.wifi_sta_connected.ssid) + 1];
    IPAddress ipAddress;

    // Take the network offline if necessary
    if (networkIsInterfaceOnline(NETWORK_WIFI) && (event != ARDUINO_EVENT_WIFI_STA_GOT_IP) &&
        (event != ARDUINO_EVENT_WIFI_STA_GOT_IP6))
    {
        networkStop(NETWORK_WIFI, settings.debugNetworkLayer); // Stop WiFi to allow it to restart
    }

    // WiFi State Machine
    //
    //   .--------+<----------+<-----------+<-------------+<----------+<----------+<------------.
    //   v        |           |            |              |           |           |             |
    // STOP --> READY --> STA_START --> SCAN_DONE --> CONNECTED --> GOT_IP --> LOST_IP --> DISCONNECTED
    //                                                    ^           ^           |             |
    //                                                    |           '-----------'             |
    //                                                    '-------------------------------------'
    //
    // Handle the event
    switch (event)
    {
    default:
        systemPrintf("ERROR: Unknown WiFi event: %d\r\n", event);
        break;

    case ARDUINO_EVENT_WIFI_OFF:
        systemPrintln("WiFi Off");
        break;

    case ARDUINO_EVENT_WIFI_READY:
        systemPrintln("WiFi Ready");
        WiFi.setHostname(settings.mdnsHostName);
        break;

    case ARDUINO_EVENT_WIFI_SCAN_DONE:
        systemPrintln("WiFi Scan Done");
        // wifi_event_sta_scan_done_t info.wifi_scan_done;
        break;

    case ARDUINO_EVENT_WIFI_STA_START:
        systemPrintln("WiFi STA Started");
        break;

    case ARDUINO_EVENT_WIFI_STA_STOP:
        systemPrintln("WiFi STA Stopped");
        break;

    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
        memcpy(ssid, info.wifi_sta_connected.ssid, info.wifi_sta_connected.ssid_len);
        ssid[info.wifi_sta_connected.ssid_len] = 0;
        systemPrintf("WiFi STA connected to %s\r\n", ssid);
        WiFi.setHostname(settings.mdnsHostName);
        break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        memcpy(ssid, info.wifi_sta_disconnected.ssid, info.wifi_sta_disconnected.ssid_len);
        ssid[info.wifi_sta_disconnected.ssid_len] = 0;
        systemPrintf("WiFi STA disconnected from %s\r\n", ssid);
        // wifi_event_sta_disconnected_t info.wifi_sta_disconnected;
        break;

    case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE:
        systemPrintln("WiFi STA Auth Mode Changed");
        // wifi_event_sta_authmode_change_t info.wifi_sta_authmode_change;
        break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        ipAddress = WiFi.localIP();
        systemPrint("WiFi STA Got IPv4: ");
        systemPrintln(ipAddress);
        networkMarkOnline(NETWORK_WIFI);
        break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP6:
        systemPrint("WiFi STA Got IPv6: ");
        systemPrintln(ipAddress);
        networkMarkOnline(NETWORK_WIFI);
        break;

    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
        systemPrintln("WiFi STA Lost IP");
        break;
    }
}

//----------------------------------------
// Determine if WIFI is connected
//----------------------------------------
bool wifiIsConnected()
{
    bool connected;

    connected = (WiFi.status() == WL_CONNECTED);
    return connected;
}

//----------------------------------------
// Determine if WIFI is running
//----------------------------------------
bool wifiIsRunning()
{
    return wifiRunning;
}

//----------------------------------------
// Counts the number of entered SSIDs
//----------------------------------------
int wifiNetworkCount()
{
    // Count SSIDs
    int networkCount = 0;
    for (int x = 0; x < MAX_WIFI_NETWORKS; x++)
    {
        if (strlen(settings.wifiNetworks[x].ssid) > 0)
            networkCount++;
    }
    return networkCount;
}

//----------------------------------------
// Given a status, return the associated state or error
//----------------------------------------
const char *wifiPrintState(wl_status_t wifiStatus)
{
    switch (wifiStatus)
    {
    case WL_NO_SHIELD:
        return ("WL_NO_SHIELD"); // 255
    case WL_STOPPED:
        return ("WL_STOPPED");
    case WL_IDLE_STATUS: // 0
        return ("WL_IDLE_STATUS");
    case WL_NO_SSID_AVAIL: // 1
        return ("WL_NO_SSID_AVAIL");
    case WL_SCAN_COMPLETED: // 2
        return ("WL_SCAN_COMPLETED");
    case WL_CONNECTED: // 3
        return ("WL_CONNECTED");
    case WL_CONNECT_FAILED: // 4
        return ("WL_CONNECT_FAILED");
    case WL_CONNECTION_LOST: // 5
        return ("WL_CONNECTION_LOST");
    case WL_DISCONNECTED: // 6
        return ("WL_DISCONNECTED");
    }
    return ("WiFi Status Unknown");
}

//----------------------------------------
// Restart WiFi
//----------------------------------------
void wifiRestart()
{
    // Restart the AP webserver if we are in that state
    if (systemState == STATE_WIFI_CONFIG)
        requestChangeState(STATE_WIFI_CONFIG_NOT_STARTED);
    else
    {
        // Restart WiFi if we are not in AP config mode
        WIFI_STOP();

        if (networkConsumers() == 0)
        {
            // Don't restart WiFi if there is no need for it
            return;
        }

        wifiForceStart();
    }
}

//----------------------------------------
// Starts the WiFi connection state machine (moves from WIFI_STATE_OFF to WIFI_STATE_CONNECTING)
// Sets the appropriate protocols (WiFi + ESP-Now)
// If radio is off entirely, start WiFi
// If ESP-Now is active, only add the LR protocol
// Returns true if WiFi has connected and false otherwise
//----------------------------------------
bool wifiForceStart()
{
    int wifiStatus;

    if (wifiNetworkCount() == 0)
    {
        systemPrintln("Error: Please enter at least one SSID before using WiFi");
        displayNoSSIDs(2000);
        WIFI_STOP();
        return false;
    }

    // Determine if WiFi is already running
    if (!wifiRunning)
    {
        if (settings.debugWifiState == true)
            systemPrintln("Starting WiFi");

        // Determine which parts of WiFi need to be started
        bool startWiFiStation = false;
        bool startWiFiAP = false;

        // Only one bit is set, WiFi Station is default
        if (networkGetConsumerTypes() == (1 << NETCONSUMER_ANY))
            startWiFiStation = true;

        // The consumers need both
        else if (networkGetConsumerTypes() & ((1 << NETCONSUMER_AP) | (1 << NETCONSUMER_STA)))
        {
            startWiFiStation = true;
            startWiFiAP = true;
        }

        // The consumers need station
        else if (networkGetConsumerTypes() & (1 << NETCONSUMER_STA))
            startWiFiStation = true;

        // The consumers need AP
        else if (networkGetConsumerTypes() & (1 << NETCONSUMER_AP))
            startWiFiAP = true;

        // Start WiFi
        if (wifiConnect(startWiFiStation, startWiFiAP, settings.wifiConnectTimeoutMs))
        {
            wifiResetTimeout();
            if (settings.debugWifiState == true)
                systemPrintln("WiFi: Start timeout reset to zero");
        }
    }

    // If we are in AP only mode, as long as the AP is started, returned true
    if (WiFi.getMode() == WIFI_MODE_AP)
        return (true);

    // If we are in STA or AP+STA mode, return if the station connected successfully
    wifiStatus = WiFi.status();
    return (wifiStatus == WL_CONNECTED);
}

//----------------------------------------
// Set WiFi timeout back to zero
// Useful if other things (such as a successful ethernet connection) need to reset wifi timeout
//----------------------------------------
void wifiResetTimeout()
{
    wifiStartTimeout = 0;
}

//----------------------------------------
// Start WiFi with throttling
//----------------------------------------
void wifiStart(NetIndex_t index, uintptr_t parameter, bool debug)
{
    static uint32_t wifiStartLastTry;
    int seconds;
    int minutes;

    // Restart delay
    if ((millis() - wifiStartLastTry) < wifiStartTimeout)
        return;
    wifiStartLastTry = millis();

    // Start WiFi
    if (wifiForceStart())
        networkSequenceNextEntry(NETWORK_WIFI, settings.debugNetworkLayer);
    else
    {
        // Increase the timeout
        wifiStartTimeout <<= 1;
        if (!wifiStartTimeout)
            wifiStartTimeout = WIFI_MIN_TIMEOUT;
        else if (wifiStartTimeout > WIFI_MAX_TIMEOUT)
            wifiStartTimeout = WIFI_MAX_TIMEOUT;

        // Display the delay
        seconds = wifiStartTimeout / MILLISECONDS_IN_A_SECOND;
        minutes = seconds / SECONDS_IN_A_MINUTE;
        seconds -= minutes * SECONDS_IN_A_MINUTE;
        if (settings.debugWifiState)
            systemPrintf("WiFi: Delaying %2d:%02d before restarting WiFi\r\n", minutes, seconds);
    }
}

//----------------------------------------
// WiFi start sequence
//----------------------------------------
NETWORK_POLL_SEQUENCE wifiStartSequence[] = {
    //  State               Parameter               Description
    {wifiStart, 0, "Initialize WiFi"},
    {nullptr, 0, "Termination"},
};

//----------------------------------------
// WiFi stop sequence
//----------------------------------------
NETWORK_POLL_SEQUENCE wifiStopSequence[] = {
    //  State               Parameter               Description
    {wifiStop, 0, "Shutdown WiFi"},
    {nullptr, 0, "Termination"},
};

//----------------------------------------
// Stop WiFi and release all resources
//----------------------------------------
void wifiStop()
{
    // Stop the web server
    stopWebServer();

    // Stop the DNS server if we were using the captive portal
    if (((WiFi.getMode() == WIFI_AP) || (WiFi.getMode() == WIFI_AP_STA)) && settings.enableCaptivePortal)
        dnsServer.stop();

    wifiConnectionAttempts = 0; // Reset the timeout

    // If ESP-Now is active, change protocol to only Long Range
    if (espnowState > ESPNOW_OFF)
    {
        if (WiFi.getMode() != WIFI_STA)
            WiFi.mode(WIFI_STA);

        // Enable long range, PHY rate of ESP32 will be 512Kbps or 256Kbps
        // esp_wifi_set_protocol requires WiFi to be started
        esp_err_t response = esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR);
        if (response != ESP_OK)
            systemPrintf("wifiShutdown: Error setting ESP-Now lone protocol: %s\r\n", esp_err_to_name(response));
        else
        {
            if (settings.debugWifiState == true)
                systemPrintln("WiFi disabled, ESP-Now left in place");
        }
    }
    else
    {
        WiFi.mode(WIFI_OFF);
        if (settings.debugWifiState == true)
            systemPrintln("WiFi Stopped");
    }

    // Take the network offline
    networkMarkOffline(NETWORK_WIFI);

    if (wifiMulti != nullptr)
        wifiMulti = nullptr;

    // Display the heap state
    reportHeapNow(settings.debugWifiState);
    wifiRunning = false;
}

// Needed for wifiStopSequence
void wifiStop(NetIndex_t index, uintptr_t parameter, bool debug)
{
    wifiStop();
    networkSequenceNextEntry(NETWORK_WIFI, settings.debugNetworkLayer);
}

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// WiFi Config Support Routines - compiled out
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

//----------------------------------------
// Start the WiFi access point
//----------------------------------------
bool wifiStartAP()
{
    return (wifiStartAP(false)); // Don't force AP mode
}

//----------------------------------------
// Start the access point for user to connect to and configure device
// We can also start as a WiFi station and attempt to connect to local WiFi for config
//----------------------------------------
bool wifiStartAP(bool forceAP)
{
    if (settings.wifiConfigOverAP == true || forceAP)
    {
        // Stop any current WiFi activity
        WIFI_STOP();

        // Start in AP mode
        WiFi.mode(WIFI_AP);

        // Before starting AP mode, be sure we have default WiFi protocols enabled.
        // esp_wifi_set_protocol requires WiFi to be started
        esp_err_t response =
            esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
        if (response != ESP_OK)
            systemPrintf("wifiStartAP: Error setting WiFi protocols: %s\r\n", esp_err_to_name(response));
        else
        {
            if (settings.debugWifiState == true)
                systemPrintln("WiFi protocols set");
        }
    }
    else
    {
        // Start webserver on local WiFi instead of AP

        // Attempt to connect to local WiFi with increasing timeouts
        int x = 0;
        const int maxTries = 2;
        for (; x < maxTries; x++)
        {
            if (wifiConnect(settings.wifiConnectTimeoutMs) == true) // Attempt to connect to any SSID on settings list
            {
                wifiDisplayState();
                break;
            }
        }
        if (x == maxTries)
        {
            displayNoWiFi(2000);
            return (wifiStartAP(true)); // Because there is no local WiFi available, force AP mode so user can still get
                                        // access/configure it
        }
    }

    return (true);
}

#endif // COMPILE_WIFI
