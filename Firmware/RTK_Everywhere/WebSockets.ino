/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
WebSockets.ino

  Web socket support
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#ifdef COMPILE_AP

//----------------------------------------
// Constants
//----------------------------------------

static const int webSocketsStackSize = 1024 * 20;   // Needs to be large enough to hold the full settingsCSV

//----------------------------------------
// New types
//----------------------------------------

typedef struct _WEB_SOCKETS_CLIENT
{
    struct _WEB_SOCKETS_CLIENT * _flink;
    struct _WEB_SOCKETS_CLIENT * _blink;
    httpd_req_t * _request;
    int _socketFD;
} WEB_SOCKETS_CLIENT;

//----------------------------------------
// Locals
//----------------------------------------

static WEB_SOCKETS_CLIENT * webSocketsClientListHead;
static WEB_SOCKETS_CLIENT * webSocketsClientListTail;
static httpd_handle_t webSocketsHandle;
static SemaphoreHandle_t webSocketsMutex;

//----------------------------------------
// Create a csv string with the dynamic data to update (current coordinates,
// battery level, etc)
//----------------------------------------
void webSocketsCreateDynamicDataString(char *csvList)
{
    csvList[0] = '\0'; // Erase current settings string

    // Current coordinates come from HPPOSLLH call back
    stringRecord(csvList, "geodeticLat", gnss->getLatitude(), haeNumberOfDecimals);
    stringRecord(csvList, "geodeticLon", gnss->getLongitude(), haeNumberOfDecimals);
    stringRecord(csvList, "geodeticAlt", gnss->getAltitude(), 3);

    double ecefX = 0;
    double ecefY = 0;
    double ecefZ = 0;

    geodeticToEcef(gnss->getLatitude(), gnss->getLongitude(), gnss->getAltitude(), &ecefX, &ecefY, &ecefZ);

    stringRecord(csvList, "ecefX", ecefX, 3);
    stringRecord(csvList, "ecefY", ecefY, 3);
    stringRecord(csvList, "ecefZ", ecefZ, 3);

    if (online.batteryFuelGauge == false) // Product has no battery
    {
        stringRecord(csvList, "batteryIconFileName", (char *)"src/BatteryBlank.png");
        stringRecord(csvList, "batteryPercent", (char *)" ");
    }
    else
    {
        // Determine battery icon
        int iconLevel = 0;
        if (batteryLevelPercent < 25)
            iconLevel = 0;
        else if (batteryLevelPercent < 50)
            iconLevel = 1;
        else if (batteryLevelPercent < 75)
            iconLevel = 2;
        else // batt level > 75
            iconLevel = 3;

        char batteryIconFileName[sizeof("src/Battery2_Charging.png__")]; // sizeof() includes 1 for \0 termination

        if (isCharging())
            snprintf(batteryIconFileName, sizeof(batteryIconFileName), "src/Battery%d_Charging.png", iconLevel);
        else
            snprintf(batteryIconFileName, sizeof(batteryIconFileName), "src/Battery%d.png", iconLevel);

        stringRecord(csvList, "batteryIconFileName", batteryIconFileName);

        // Limit batteryLevelPercent to sane levels
        if (batteryLevelPercent > 100)
            batteryLevelPercent = 100;

        // Determine battery percent
        char batteryPercent[sizeof("+100%__")];
        if (isCharging())
            snprintf(batteryPercent, sizeof(batteryPercent), "+%d%%", batteryLevelPercent);
        else
            snprintf(batteryPercent, sizeof(batteryPercent), "%d%%", batteryLevelPercent);
        stringRecord(csvList, "batteryPercent", batteryPercent);
    }

    strcat(csvList, "\0");
}

//----------------------------------------
// Report back to the web config page with a CSV that contains the either CURRENT or
// the latest version as obtained by the OTA state machine
//----------------------------------------
void webSocketsCreateFirmwareVersionString(char *firmwareString)
{
    char newVersionCSV[100];

    firmwareString[0] = '\0'; // Erase current settings string

    // Create a string of the unit's current firmware version
    char currentVersion[21];
    firmwareVersionGet(currentVersion, sizeof(currentVersion), enableRCFirmware);

    // Compare the unit's version against the reported version from OTA
    if (firmwareVersionIsReportedNewer(otaReportedVersion, currentVersion) == true)
    {
        if (settings.debugWebServer == true)
            systemPrintln("WebSockets: New firmware version detected");
        snprintf(newVersionCSV, sizeof(newVersionCSV), "%s,", otaReportedVersion);
    }
    else
    {
        if (settings.debugWebServer == true)
            systemPrintln("No new firmware available");
        snprintf(newVersionCSV, sizeof(newVersionCSV), "CURRENT,");
    }

    stringRecord(firmwareString, "newFirmwareVersion", newVersionCSV);

    strcat(firmwareString, "\0");
}

//----------------------------------------
// Handler for web sockets requests
//----------------------------------------
static esp_err_t webSocketsHandler(httpd_req_t *req)
{
    WEB_SOCKETS_CLIENT * client;
    WEB_SOCKETS_CLIENT * entry;

    // Log the req, so we can reuse it for httpd_ws_send_frame
    // TODO: do we need to be cleverer about this?
    // last_ws_req = req;

    if (req->method == HTTP_GET)
    {
        // Allocate a WEB_SOCKETS_CLIENT structure
        client = (WEB_SOCKETS_CLIENT *)rtkMalloc(sizeof(WEB_SOCKETS_CLIENT), "WEB_SOCKETS_CLIENT");
        if (client == nullptr)
        {
            if (settings.debugWebServer == true)
                systemPrintf("ERROR: Failed to allocate WEB_SOCKETS_CLIENT!\r\n");
            return ESP_FAIL;
        }

        // Save the client context
        client->_request = req;
        client->_socketFD = httpd_req_to_sockfd(req);

        // Single thread access to the list of clients;
        xSemaphoreTake(webSocketsMutex, portMAX_DELAY);

        // ListHead -> ... -> client (flink) -> nullptr;
        // ListTail -> client (blink) -> ... -> nullptr;
        // Add this client to the list
        client->_flink = nullptr;
        entry = webSocketsClientListTail;
        client->_blink = entry;
        if (entry)
            entry->_flink = client;
        else
            webSocketsClientListHead = client;
        webSocketsClientListTail = client;

        // Release the synchronization
        xSemaphoreGive(webSocketsMutex);

        if (settings.debugWebServer == true)
            systemPrintf("webSockets: Added client, _request: %p, _socketFD: %d\r\n",
                         client->_request, client->_socketFD);

        lastDynamicDataUpdate = millis();

        // Send new settings to browser.
        char * settingsCsvList = (char *)rtkMalloc(AP_CONFIG_SETTING_SIZE, "Command CSV settings list");
        if (settingsCsvList)
        {
            createSettingsString(settingsCsvList);
            webSocketsSendString(settingsCsvList);
            rtkFree(settingsCsvList, "Command CSV settings list");
            return ESP_OK;
        }
        if (settings.debugWebServer == true)
            systemPrintf("ERROR: Failed to allocate settings CSV list!\r\n");
        return ESP_FAIL;
    }

    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    /* Set max_len = 0 to get the frame len */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK)
    {
        systemPrintf("WebSockets: httpd_ws_recv_frame failed to get frame len with %d\r\n", ret);
        return ret;
    }
    if (settings.debugWebServer == true)
        systemPrintf("WebSockets: frame len is %d\r\n", ws_pkt.len);
    if (ws_pkt.len)
    {
        /* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
        buf = (uint8_t *)rtkMalloc(ws_pkt.len + 1, "Payload buffer (buf)");
        if (buf == NULL)
        {
            systemPrintln("WebSockets: Failed to malloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;
        /* Set max_len = ws_pkt.len to get the frame payload */
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK)
        {
            systemPrintf("WebSockets: httpd_ws_recv_frame failed with %d\r\n", ret);
            rtkFree(buf, "Payload buffer (buf)");
            return ret;
        }
    }
    if (settings.debugWebServer == true)
    {
        const char *pktType;
        size_t length = ws_pkt.len;
        switch (ws_pkt.type)
        {
        default:
            pktType = nullptr;
            break;
        case HTTPD_WS_TYPE_CONTINUE:
            pktType = "HTTPD_WS_TYPE_CONTINUE";
            break;
        case HTTPD_WS_TYPE_TEXT:
            pktType = "HTTPD_WS_TYPE_TEXT";
            break;
        case HTTPD_WS_TYPE_BINARY:
            pktType = "HTTPD_WS_TYPE_BINARY";
            break;
        case HTTPD_WS_TYPE_CLOSE:
            pktType = "HTTPD_WS_TYPE_CLOSE";
            break;
        case HTTPD_WS_TYPE_PING:
            pktType = "HTTPD_WS_TYPE_PING";
            break;
        case HTTPD_WS_TYPE_PONG:
            pktType = "HTTPD_WS_TYPE_PONG";
            break;
        }
        systemPrintf("WebSockets: Packet: %p, %d bytes, type: %d%s%s%s\r\n", ws_pkt.payload, length, ws_pkt.type,
                     pktType ? " (" : "", pktType ? pktType : "", pktType ? ")" : "");
        if (length > 0x40)
            length = 0x40;
        dumpBuffer(ws_pkt.payload, length);
    }

    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT)
    {
        if (currentlyParsingData == false)
        {
            for (int i = 0; i < ws_pkt.len; i++)
            {
                incomingSettings[incomingSettingsSpot++] = ws_pkt.payload[i];
                if (incomingSettingsSpot == AP_CONFIG_SETTING_SIZE)
                    systemPrintln("WebSockets: incomingSettings wrap-around. Increase AP_CONFIG_SETTING_SIZE");
                incomingSettingsSpot %= AP_CONFIG_SETTING_SIZE;
            }
            timeSinceLastIncomingSetting = millis();
        }
        else
        {
            if (settings.debugWebServer == true)
                systemPrintln("WebSockets: Ignoring packet due to parsing block");
        }
    }
    else if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE)
    {
        if (settings.debugWebServer == true)
            systemPrintln("WebSockets: Client closed or refreshed the web page");

        createSettingsString(settingsCSV);
    }

    rtkFree(buf, "Payload buffer (buf)");
    return ret;
}

//----------------------------------------
// Determine if webSockets is connected to a client
//----------------------------------------
bool webSocketsIsConnected()
{
    return (webSocketsClientListHead != nullptr);
}

//----------------------------------------
// Send the formware version via web sockets
//----------------------------------------
void webSocketsSendFirmwareVersion(void)
{
    char * firmwareVersion;

    firmwareVersion = (char *)rtkMalloc(AP_FIRMWARE_VERSION_SIZE, "WebSockets firmware description");
    if (firmwareVersion == nullptr)
        systemPrintf("ERROR: WebSockets failed to allocate firmware description\r\n");
    else
    {
        webSocketsCreateFirmwareVersionString(firmwareVersion);

        if (settings.debugWebServer)
            systemPrintf("WebSockets: Firmware version requested. Sending: %s\r\n",
                         firmwareVersion);

        webSocketsSendString(firmwareVersion);
        rtkFree(firmwareVersion, "WebSockets firmware description");
    }
}

//----------------------------------------
// Send the current settings via web sockets
//----------------------------------------
void webSocketsSendSettings(void)
{
    char * settingsCsvList;

    settingsCsvList = (char *)rtkMalloc(AP_CONFIG_SETTING_SIZE, "WebSockets CSV settings list");
    if (settingsCsvList == nullptr)
        systemPrintf("ERROR: WebSockets failed to allocate settings CSV list\r\n");
    else
    {
        webSocketsCreateDynamicDataString(settingsCsvList);
        webSocketsSendString(settingsCsvList);
        rtkFree(settingsCsvList, "WebSockets CSV settings list");
    }
}

//----------------------------------------
// Send a string to the browser using the web socket
//----------------------------------------
void webSocketsSendString(const char *stringToSend)
{
    WEB_SOCKETS_CLIENT * client;

    if (!webSocketsIsConnected())
    {
        systemPrintf("webSocketsSendString: not connected - could not send: %s\r\n", stringToSend);
        return;
    }

    // Describe the packet to send
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t *)stringToSend;
    ws_pkt.len = strlen(stringToSend);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    // Single thread access to the list of clients;
    xSemaphoreTake(webSocketsMutex, portMAX_DELAY);

    // Send this message to each of the clients
    client = webSocketsClientListHead;
    while (client)
    {
        // Get the next client
        WEB_SOCKETS_CLIENT * nextClient = client->_flink;

        // Send the string to to the client browser
        esp_err_t ret = httpd_ws_send_frame_async(webSocketsHandle,
                                                  client->_socketFD,
                                                  &ws_pkt);

        // Check for message send failure
        if (ret != ESP_OK)
        {
            systemPrintf("WebSockets: httpd_ws_send_frame failed with %d for client request: %x\r\n",
                         ret, client->_request);

            // Remove this client
            WEB_SOCKETS_CLIENT * previousClient = client->_blink;
            if (previousClient)
                previousClient->_flink = nextClient;
            else
                webSocketsClientListHead = nextClient;
            if (nextClient)
                nextClient->_blink = previousClient;
            else
                webSocketsClientListTail = previousClient;

            // Done with this client
            rtkFree(client, "WEB_SOCKETS_CLINET");
        }

        // Successfully sent the message
        else if (settings.debugWebServer == true)
            systemPrintf("webSocketsSendString: %s\r\n", stringToSend);

        // Get the next client
        client = nextClient;
    }

    // Release the synchronization
    xSemaphoreGive(webSocketsMutex);
}

//----------------------------------------
// Web page description
//----------------------------------------
static const httpd_uri_t webSocketsPage = {.uri = "/ws",
                                           .method = HTTP_GET,
                                           .handler = webSocketsHandler,
                                           .user_ctx = NULL,
                                           .is_websocket = true,
                                           .handle_ws_control_frames = true,
                                           .supported_subprotocol = NULL};

//----------------------------------------
// Start the web sockets layer
//----------------------------------------
bool webSocketsStart(void)
{
    esp_err_t status;

    // Get the configuration object
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Use different ports for websocket and webServer - use port 81 for the websocket - also defined in main.js
    config.server_port = 81;

    // Increase the stack size from 4K to handle page processing (settingsCSV)
    config.stack_size = webSocketsStackSize;

    // Start the httpd server
    if (settings.debugWebServer == true)
        systemPrintf("webSockets starting on port: %d\r\n", config.server_port);

    if (settings.debugWebServer == true)
    {
        httpdDisplayConfig(&config);
        reportHeapNow(true);
    }

    // Allocate the mutex
    if (webSocketsMutex == nullptr)
    {
        webSocketsMutex = xSemaphoreCreateMutex();
        if (webSocketsMutex == nullptr)
        {
            if (settings.debugWebServer)
                systemPrintf("ERROR: webSockets failed to allocate the mutex!\r\n");
            return false;
        }
    }

    status = httpd_start(&webSocketsHandle, &config);
    if (status == ESP_OK)
    {
        // Registering the ws handler
        if (settings.debugWebServer == true)
            systemPrintln("webSockets registering URI handlers");
        httpd_register_uri_handler(webSocketsHandle, &webSocketsPage);
        return true;
    }

    // Display the failure to start
    if (settings.debugWebServer)
        systemPrintf("ERROR: webSockets failed to start, status: %s!\r\n", esp_err_to_name(status));
    return false;
}

//----------------------------------------
// Stop the web sockets layer
//----------------------------------------
void webSocketsStop()
{
    WEB_SOCKETS_CLIENT * client;

    if (webSocketsHandle != nullptr)
    {
        // Single thread access to the list of clients;
        xSemaphoreTake(webSocketsMutex, portMAX_DELAY);

        // ListHead -> ... -> client (flink) -> nullptr;
        // ListTail -> client (blink) -> ... -> nullptr;
        // Discard the clients
        while (webSocketsClientListHead)
        {
            // Remove this client
            client = webSocketsClientListHead;
            webSocketsClientListHead = client->_flink;

            // Discard this client
            rtkFree(client, "WEB_SOCKETS_CLIENT");
        }
        webSocketsClientListTail = nullptr;

        // ListHead -> nullptr;
        // ListTail -> nullptr;
        // Release the synchronization
        xSemaphoreGive(webSocketsMutex);

        // Stop the httpd server
        esp_err_t status = httpd_stop(webSocketsHandle);
        if (status == ESP_OK)
            systemPrintf("webSockets stopped\r\n");
        else
            systemPrintf("ERROR: webSockets failed to stop, status: %s!\r\n", esp_err_to_name(status));
        webSocketsHandle = nullptr;
    }
}

#endif // COMPILE_AP
