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
// Locals
//----------------------------------------

static int last_ws_fd;
// httpd_req_t *last_ws_req;
static httpd_handle_t webSocketsHandle;

//----------------------------------------
// Create a csv string with the dynamic data to update (current coordinates,
// battery level, etc)
//----------------------------------------
void webSocketsCreateDynamicDataString(char *settingsCSV)
{
    settingsCSV[0] = '\0'; // Erase current settings string

    // Current coordinates come from HPPOSLLH call back
    stringRecord(settingsCSV, "geodeticLat", gnss->getLatitude(), haeNumberOfDecimals);
    stringRecord(settingsCSV, "geodeticLon", gnss->getLongitude(), haeNumberOfDecimals);
    stringRecord(settingsCSV, "geodeticAlt", gnss->getAltitude(), 3);

    double ecefX = 0;
    double ecefY = 0;
    double ecefZ = 0;

    geodeticToEcef(gnss->getLatitude(), gnss->getLongitude(), gnss->getAltitude(), &ecefX, &ecefY, &ecefZ);

    stringRecord(settingsCSV, "ecefX", ecefX, 3);
    stringRecord(settingsCSV, "ecefY", ecefY, 3);
    stringRecord(settingsCSV, "ecefZ", ecefZ, 3);

    if (online.batteryFuelGauge == false) // Product has no battery
    {
        stringRecord(settingsCSV, "batteryIconFileName", (char *)"src/BatteryBlank.png");
        stringRecord(settingsCSV, "batteryPercent", (char *)" ");
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

        stringRecord(settingsCSV, "batteryIconFileName", batteryIconFileName);

        // Limit batteryLevelPercent to sane levels
        if (batteryLevelPercent > 100)
            batteryLevelPercent = 100;

        // Determine battery percent
        char batteryPercent[sizeof("+100%__")];
        if (isCharging())
            snprintf(batteryPercent, sizeof(batteryPercent), "+%d%%", batteryLevelPercent);
        else
            snprintf(batteryPercent, sizeof(batteryPercent), "%d%%", batteryLevelPercent);
        stringRecord(settingsCSV, "batteryPercent", batteryPercent);
    }

    strcat(settingsCSV, "\0");
}

//----------------------------------------
// Report back to the web config page with a CSV that contains the either CURRENT or
// the latest version as obtained by the OTA state machine
//----------------------------------------
void webSocketsCreateFirmwareVersionString(char *settingsCSV)
{
    char newVersionCSV[100];

    settingsCSV[0] = '\0'; // Erase current settings string

    // Create a string of the unit's current firmware version
    char currentVersion[21];
    firmwareVersionGet(currentVersion, sizeof(currentVersion), enableRCFirmware);

    // Compare the unit's version against the reported version from OTA
    if (firmwareVersionIsReportedNewer(otaReportedVersion, currentVersion) == true)
    {
        if (settings.debugWebServer == true)
            systemPrintln("New version detected");
        snprintf(newVersionCSV, sizeof(newVersionCSV), "%s,", otaReportedVersion);
    }
    else
    {
        if (settings.debugWebServer == true)
            systemPrintln("No new firmware available");
        snprintf(newVersionCSV, sizeof(newVersionCSV), "CURRENT,");
    }

    stringRecord(settingsCSV, "newFirmwareVersion", newVersionCSV);

    strcat(settingsCSV, "\0");
}

//----------------------------------------
// Handler for web sockets requests
//----------------------------------------
static esp_err_t webSocketsHandler(httpd_req_t *req)
{
    // Log the req, so we can reuse it for httpd_ws_send_frame
    // TODO: do we need to be cleverer about this?
    // last_ws_req = req;

    if (req->method == HTTP_GET)
    {
        // Log the fd, so we can reuse it for httpd_ws_send_frame_async
        // TODO: do we need to be cleverer about this?
        last_ws_fd = httpd_req_to_sockfd(req);

        if (settings.debugWebServer == true)
            systemPrintf("Handshake done, the new ws connection was opened with fd %d\r\n", last_ws_fd);

        websocketConnected = true;
        lastDynamicDataUpdate = millis();
        webSocketsSendString(settingsCSV);

        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    /* Set max_len = 0 to get the frame len */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK)
    {
        systemPrintf("httpd_ws_recv_frame failed to get frame len with %d\r\n", ret);
        return ret;
    }
    if (settings.debugWebServer == true)
        systemPrintf("frame len is %d\r\n", ws_pkt.len);
    if (ws_pkt.len)
    {
        /* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
        buf = (uint8_t *)rtkMalloc(ws_pkt.len + 1, "Payload buffer (buf)");
        if (buf == NULL)
        {
            systemPrintln("Failed to malloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;
        /* Set max_len = ws_pkt.len to get the frame payload */
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK)
        {
            systemPrintf("httpd_ws_recv_frame failed with %d\r\n", ret);
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
        systemPrintf("Packet: %p, %d bytes, type: %d%s%s%s\r\n", ws_pkt.payload, length, ws_pkt.type,
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
                    systemPrintln("incomingSettings wrap-around. Increase AP_CONFIG_SETTING_SIZE");
                incomingSettingsSpot %= AP_CONFIG_SETTING_SIZE;
            }
            timeSinceLastIncomingSetting = millis();
        }
        else
        {
            if (settings.debugWebServer == true)
                systemPrintln("Ignoring packet due to parsing block");
        }
    }
    else if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE)
    {
        if (settings.debugWebServer == true)
            systemPrintln("Client closed or refreshed the web page");

        createSettingsString(settingsCSV);
        websocketConnected = false;
    }

    rtkFree(buf, "Payload buffer (buf)");
    return ret;
}

//----------------------------------------
// Send the formware version via web sockets
//----------------------------------------
void webSocketsSendFirmwareVersion(void)
{
    webSocketsCreateFirmwareVersionString(settingsCSV);

    if (settings.debugWebServer)
        systemPrintf("WebServer: Firmware version requested. Sending: %s\r\n", settingsCSV);

    webSocketsSendString(settingsCSV);
}

//----------------------------------------
// Send the current settings via web sockets
//----------------------------------------
void webSocketsSendSettings(void)
{
    webSocketsCreateDynamicDataString(settingsCSV);
    webSocketsSendString(settingsCSV);
}

//----------------------------------------
// Send a string to the browser using the web socket
//----------------------------------------
void webSocketsSendString(const char *stringToSend)
{
    if (!websocketConnected)
    {
        systemPrintf("webSocketsSendString: not connected - could not send: %s\r\n", stringToSend);
        return;
    }

    // To send content to the webServer, we would call: webServer->sendContent(stringToSend);
    // But here we want to send content to the websocket (webSocketsHandle)...

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t *)stringToSend;
    ws_pkt.len = strlen(stringToSend);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    // If we use httpd_ws_send_frame, it requires a req.
    // esp_err_t ret = httpd_ws_send_frame(last_ws_req, &ws_pkt);
    // if (ret != ESP_OK) {
    //    ESP_LOGE(TAG, "httpd_ws_send_frame failed with %d", ret);
    //}

    // If we use httpd_ws_send_frame_async, it requires a fd.
    esp_err_t ret = httpd_ws_send_frame_async(webSocketsHandle, last_ws_fd, &ws_pkt);
    if (ret != ESP_OK)
    {
        systemPrintf("httpd_ws_send_frame failed with %d\r\n", ret);
    }
    else
    {
        if (settings.debugWebServer == true)
            systemPrintf("webSocketsSendString: %s\r\n", stringToSend);
    }
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
        systemPrintf("Starting webSockets on port: %d\r\n", config.server_port);

    if (settings.debugWebServer == true)
    {
        httpdDisplayConfig(&config);
        reportHeapNow(true);
    }
    status = httpd_start(&webSocketsHandle, &config);
    if (status == ESP_OK)
    {
        // Registering the ws handler
        if (settings.debugWebServer == true)
            systemPrintln("Registering URI handlers");
        httpd_register_uri_handler(webSocketsHandle, &webSocketsPage);
        return true;
    }

    // Display the failure to start
    systemPrintf("ERROR: webSockets failed to start, status: %s!\r\n", esp_err_to_name(status));
    return false;
}

//----------------------------------------
// Stop the web sockets layer
//----------------------------------------
void webSocketsStop()
{
    websocketConnected = false;

    if (webSocketsHandle != nullptr)
    {
        // Stop the httpd server
        esp_err_t status = httpd_stop(webSocketsHandle);
        if (status != ESP_OK)
            systemPrintf("ERROR: webSockets failed to stop, status: %s!\r\n", esp_err_to_name(status));
        webSocketsHandle = nullptr;
    }
}

#endif // COMPILE_AP
