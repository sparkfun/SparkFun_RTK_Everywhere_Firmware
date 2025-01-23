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

int wifiFailedConnectionAttempts = 0; // Count the number of connection attempts between restarts

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

// Start timeout
static uint32_t wifiStartTimeout;
static uint32_t wifiStartLastTry; // The last time WiFi start was attempted

// WiFi Timer usage:
//  * Measure interval to display IP address
static unsigned long wifiDisplayTimer;

// DNS server for Captive Portal
extern DNSServer dnsServer;

static bool wifiStationRunning;
static bool wifiApRunning;

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
// Starts WiFi in STA, AP, or STA_AP mode depending on bools
// Returns true if STA connects, or if AP is started
//----------------------------------------
bool wifiConnect(bool startWiFiStation, bool startWiFiAP, unsigned long timeout)
{
    // Is a change needed?
    if (startWiFiStation && startWiFiAP && WiFi.getMode() == WIFI_AP_STA && WiFi.status() == WL_CONNECTED)
        return (true); // There is nothing needing to be changed

    if (startWiFiStation && WiFi.getMode() == WIFI_STA && WiFi.status() == WL_CONNECTED)
        return (true); // There is nothing needing to be changed

    if (startWiFiAP && WiFi.getMode() == WIFI_AP)
        return (true); // There is nothing needing to be changed

    wifiStationRunning = false; // Mark it as offline while we mess about
    wifiApRunning = false;      // Mark it as offline while we mess about

    wifi_mode_t wifiMode = WIFI_OFF;
    wifi_interface_t wifiInterface = WIFI_IF_STA;

    // Establish what WiFi mode we need to be in
    if (startWiFiStation && startWiFiAP)
    {
        systemPrintln("Starting WiFi AP+Station");
        wifiMode = WIFI_AP_STA;
        wifiInterface = WIFI_IF_AP; // There is no WIFI_IF_AP_STA
    }
    else if (startWiFiStation)
    {
        systemPrintln("Starting WiFi Station");
        wifiMode = WIFI_STA;
        wifiInterface = WIFI_IF_STA;
    }
    else if (startWiFiAP)
    {
        systemPrintln("Starting WiFi AP");
        wifiMode = WIFI_AP;
        wifiInterface = WIFI_IF_AP;
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

        // Determine the AP name
        // If in web config mode then 'RTK Config'
        // otherwise 'RTK Caster'

        char softApSsid[strlen("RTK Config")]; // Must be short enough to fit OLED Width

        if (inWebConfigMode())
            strncpy(softApSsid, "RTK Config", sizeof(softApSsid));
        // snprintf("%s", sizeof(softApSsid), softApSsid, (const char)"RTK Config"); // TODO use
        // settings.webconfigApName
        else
            strncpy(softApSsid, "RTK Caster", sizeof(softApSsid));
        // snprintf("%s", sizeof(softApSsid), softApSsid, (const char)"RTK Caster");

        if (WiFi.softAP(softApSsid) == false)
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

        wifiApRunning = true;

        // If we're only here to start the AP, then we're done
        if (wifiMode == WIFI_AP)
            return true;
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
        wifiResetTimeout(); // If we successfully connected then reset the throttling timeout
        wifiStationRunning = true;
        return true;
    }
    if (wifiStatus == WL_DISCONNECTED)
        systemPrint("No friendly WiFi networks detected.\r\n");
    else
    {
        systemPrintf("WiFi failed to connect: error #%d - %s\r\n", wifiStatus, wifiPrintState((wl_status_t)wifiStatus));
    }
    wifiStationRunning = false;
    return false;
}

//----------------------------------------
// Display the WiFi state
//----------------------------------------
void wifiDisplayState()
{
    systemPrintf("WiFi: %s\r\n", networkInterfaceHasInternet(NETWORK_WIFI) ? "Online" : "Offline");
    systemPrintf("    MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\r\n", wifiMACAddress[0], wifiMACAddress[1],
                 wifiMACAddress[2], wifiMACAddress[3], wifiMACAddress[4], wifiMACAddress[5]);
    if (networkInterfaceHasInternet(NETWORK_WIFI))
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

    // If we are in AP or AP_STA, the network is immediately marked online
    // Once AP is online, don't stop WiFi because STA has various events
    if (WiFi.getMode() == WIFI_MODE_STA)
    {
        // Take the network offline if necessary
        if (networkInterfaceHasInternet(NETWORK_WIFI) && (event != ARDUINO_EVENT_WIFI_STA_GOT_IP) &&
            (event != ARDUINO_EVENT_WIFI_STA_GOT_IP6))
        {
            if (settings.debugWifiState)
                systemPrintf("Stopping WiFi because of event # %d\r\n", event);

            networkStop(NETWORK_WIFI, settings.debugNetworkLayer); // Stop WiFi to allow it to restart
        }
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
        if (settings.debugWifiState)
            systemPrintln("WiFi Ready");
        WiFi.setHostname(settings.mdnsHostName);
        break;

    case ARDUINO_EVENT_WIFI_SCAN_DONE:
        if (settings.debugWifiState)
            systemPrintln("WiFi Scan Done");
        // wifi_event_sta_scan_done_t info.wifi_scan_done;
        break;

    case ARDUINO_EVENT_WIFI_STA_START:
        if (settings.debugWifiState)
            systemPrintln("WiFi STA Started");
        break;

    case ARDUINO_EVENT_WIFI_STA_STOP:
        if (settings.debugWifiState)
            systemPrintln("WiFi STA Stopped");
        break;

    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
        memcpy(ssid, info.wifi_sta_connected.ssid, info.wifi_sta_connected.ssid_len);
        ssid[info.wifi_sta_connected.ssid_len] = 0;

        ipAddress = WiFi.localIP();
        systemPrintf("WiFi STA connected to %s with IP address: ", ssid);
        systemPrintln(ipAddress);

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
        if (settings.debugWifiState)
        {
            ipAddress = WiFi.localIP();
            systemPrint("WiFi STA Got IPv4: ");
            systemPrintln(ipAddress);
        }
        networkInterfaceInternetConnectionAvailable(NETWORK_WIFI);
        break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP6:
        if (settings.debugWifiState)
        {
            ipAddress = WiFi.localIP();
            systemPrint("WiFi STA Got IPv6: ");
            systemPrintln(ipAddress);
        }
        networkInterfaceInternetConnectionAvailable(NETWORK_WIFI);
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
// Determine if WiFi Station is running
//----------------------------------------
bool wifiStationIsRunning()
{
    return wifiStationRunning;
}

//----------------------------------------
// Determine if WiFi AP is running
//----------------------------------------
bool wifiApIsRunning()
{
    return wifiApRunning;
}

//----------------------------------------
// Determine if either Station or AP is running
//----------------------------------------
bool wifiIsRunning()
{
    if (wifiStationRunning || wifiApRunning)
        return true;
    return false;
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
// Starts the WiFi connection state machine (moves from WIFI_STATE_OFF to WIFI_STATE_CONNECTING)
// Sets the appropriate protocols (WiFi + ESP-Now)
// If radio is off entirely, start WiFi
// If ESP-Now is active, only add the LR protocol
// Returns true if WiFi has connected and false otherwise
//----------------------------------------
bool wifiStart()
{
    int wifiStatus;

    // Determine which parts of WiFi need to be started
    bool startWiFiStation = false;
    bool startWiFiAp = false;

    uint16_t consumerTypes = networkGetConsumerTypes();

    // The consumers need station
    if (consumerTypes & (1 << NETCONSUMER_WIFI_STA))
        startWiFiStation = true;

    // The consumers need AP
    if (consumerTypes & (1 << NETCONSUMER_WIFI_AP))
        startWiFiAp = true;

    if (startWiFiStation == false && startWiFiAp == false)
    {
        systemPrintln("wifiStart() requested without any NETCONSUMER combination");
        WIFI_STOP();
        return (false);
    }

    // Determine if WiFi is already running
    if (startWiFiStation == wifiStationRunning && startWiFiAp == wifiApRunning)
    {
        if (settings.debugWifiState == true)
            systemPrintln("WiFi is already running with requested setup");
        return (true);
    }

    // Handle special cases if no networks have been entered
    if (wifiNetworkCount() == 0)
    {
        if (startWiFiStation == true && startWiFiAp == false)
        {
            systemPrintln("Error: Please enter at least one SSID before using WiFi");
            displayNoSSIDs(2000);
            WIFI_STOP();
            return false;
        }
        else if (startWiFiStation == true && startWiFiAp == true)
        {
            systemPrintln("Error: No SSID available to start WiFi Station during AP");
            // Allow the system to continue in AP only mode
            startWiFiStation = false;
        }
    }

    // Start WiFi
    wifiConnect(startWiFiStation, startWiFiAp, settings.wifiConnectTimeoutMs);

    // If we are in AP only mode, as long as the AP is started, return true
    if (WiFi.getMode() == WIFI_MODE_AP)
        return (wifiApIsRunning);

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
    if (settings.debugWifiState == true)
        systemPrintln("WiFi: Start timeout reset to zero");
}

//----------------------------------------
//----------------------------------------
uint32_t wifiGetStartTimeout()
{
    return (wifiStartTimeout);
}

//----------------------------------------
// Reset the last WiFi start attempt
// Useful when WiFi settings have changed
//----------------------------------------
void wifiResetThrottleTimeout()
{
    wifiStartLastTry = 0;
}

//----------------------------------------
// Start WiFi with throttling
//----------------------------------------
void wifiThrottledStart(NetIndex_t index, uintptr_t parameter, bool debug)
{
    int seconds;
    int minutes;

    // Restart delay
    if ((millis() - wifiStartLastTry) < wifiStartTimeout)
        return;
    wifiStartLastTry = millis();

    // Start WiFi
    if (wifiStart())
    {
        networkSequenceNextEntry(NETWORK_WIFI, settings.debugNetworkLayer);
        wifiFailedConnectionAttempts = 0;
    }
    else
    {
        // Increase the timeout
        wifiStartTimeout <<= 1;
        if (!wifiStartTimeout)
            wifiStartTimeout = WIFI_MIN_TIMEOUT;
        else if (wifiStartTimeout > WIFI_MAX_TIMEOUT)
            wifiStartTimeout = WIFI_MAX_TIMEOUT;

        wifiFailedConnectionAttempts++;

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
    {wifiThrottledStart, 0, "Initialize WiFi"},
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

    wifiFailedConnectionAttempts = 0; // Reset the counter

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
    networkInterfaceInternetConnectionLost(NETWORK_WIFI);

    if (wifiMulti != nullptr)
        wifiMulti = nullptr;

    // Display the heap state
    reportHeapNow(settings.debugWifiState);
    wifiStationRunning = false;
    wifiApRunning = false;
}

// Needed for wifiStopSequence
void wifiStop(NetIndex_t index, uintptr_t parameter, bool debug)
{
    wifiStop();
    networkSequenceNextEntry(NETWORK_WIFI, settings.debugNetworkLayer);
}

// Returns true if we deem WiFi is not going to connect
// Used to allow cellular to start
bool wifiUnavailable()
{
    if(wifiNetworkCount() == 0)
        return true;

    if (wifiFailedConnectionAttempts > 2)
        return true;

    return false;
}

#endif // COMPILE_WIFI
