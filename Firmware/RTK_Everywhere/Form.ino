/*------------------------------------------------------------------------------
Form.ino

  Start and stop the web-server, provide the form and handle browser input.
------------------------------------------------------------------------------*/

#ifdef COMPILE_AP

// Once connected to the access point for WiFi Config, the ESP32 sends current setting values in one long string to
// websocket After user clicks 'save', data is validated via main.js and a long string of values is returned.

bool websocketConnected = false;

// Inspired by:
// https://github.com/espressif/arduino-esp32/blob/master/libraries/WebServer/examples/MultiHomedServers/MultiHomedServers.ino
// https://esp32.com/viewtopic.php?t=37384
// https://github.com/espressif/esp-idf/blob/master/examples/protocols/http_server/ws_echo_server/main/ws_echo_server.c
// https://github.com/espressif/arduino-esp32/blob/master/libraries/ESP32/examples/Camera/CameraWebServer/CameraWebServer.ino
// https://esp32.com/viewtopic.php?t=24445

// These are useful:
// https://github.com/mo-thunderz/Esp32WifiPart2/blob/main/Arduino/ESP32WebserverWebsocket/ESP32WebserverWebsocket.ino
// https://www.youtube.com/watch?v=15X0WvGaVg8

void sendStringToWebsocket(const char *stringToSend)
{
    if (!websocketConnected)
    {
        systemPrintf("sendStringToWebsocket: not connected - could not send: %s\r\n", stringToSend);
        return;
    }

    // To send content to the webserver, we would call: webserver->sendContent(stringToSend);
    //
    // But here we want to send content to the websocket (wsserver)...

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t*)stringToSend;
    ws_pkt.len = strlen(stringToSend);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    // If we use httpd_ws_send_frame, it requires a req.
    //esp_err_t ret = httpd_ws_send_frame(last_ws_req, &ws_pkt);
    //if (ret != ESP_OK) {
    //    ESP_LOGE(TAG, "httpd_ws_send_frame failed with %d", ret);
    //}

    // If we use httpd_ws_send_frame_async, it requires a fd.
    esp_err_t ret = httpd_ws_send_frame_async(*wsserver, last_ws_fd, &ws_pkt);
    if (ret != ESP_OK) {
        systemPrintf("httpd_ws_send_frame failed with %d\r\n", ret);
    }
    else
    {
        if (settings.debugWebConfig == true)
            systemPrintf("sendStringToWebsocket: %s\r\n", stringToSend);
    }

}

static esp_err_t ws_handler(httpd_req_t *req)
{
    // Log the req, so we can reuse it for httpd_ws_send_frame
    // TODO: do we need to be cleverer about this?
    //last_ws_req = req;

    if (req->method == HTTP_GET) {
        // Log the fd, so we can reuse it for httpd_ws_send_frame_async
        // TODO: do we need to be cleverer about this?
        last_ws_fd = httpd_req_to_sockfd(req);

        if (settings.debugWebConfig == true)
            systemPrintf("Handshake done, the new ws connection was opened with fd %d\r\n", last_ws_fd);

        websocketConnected = true;
        lastDynamicDataUpdate = millis();
        sendStringToWebsocket(settingsCSV);

        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    /* Set max_len = 0 to get the frame len */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        systemPrintf("httpd_ws_recv_frame failed to get frame len with %d\r\n", ret);
        return ret;
    }
    if (settings.debugWebConfig == true)
        systemPrintf("frame len is %d\r\n", ws_pkt.len);
    if (ws_pkt.len) {
        /* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
        if (online.psram == true)
            buf = (uint8_t *)ps_malloc(ws_pkt.len + 1);
        else
            buf = (uint8_t *)malloc(ws_pkt.len + 1);
        
        if (buf == NULL) {
            systemPrintln("Failed to malloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;
        /* Set max_len = ws_pkt.len to get the frame payload */
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            systemPrintf("httpd_ws_recv_frame failed with %d\r\n", ret);
            free(buf);
            return ret;
        }
    }
    if (settings.debugWebConfig == true)
        systemPrintf("Packet type: %d\r\n", ws_pkt.type);
    // HTTPD_WS_TYPE_CONTINUE   = 0x0,
    // HTTPD_WS_TYPE_TEXT       = 0x1,
    // HTTPD_WS_TYPE_BINARY     = 0x2,
    // HTTPD_WS_TYPE_CLOSE      = 0x8,
    // HTTPD_WS_TYPE_PING       = 0x9,
    // HTTPD_WS_TYPE_PONG       = 0xA


    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT)
    {
        if (settings.debugWebConfig == true)
            systemPrintf("Got packet with message: %s\r\n", ws_pkt.payload);

        if (currentlyParsingData == false)
        {
            for (int i = 0; i < ws_pkt.len; i++)
            {
                incomingSettings[incomingSettingsSpot++] = ws_pkt.payload[i];
                incomingSettingsSpot %= AP_CONFIG_SETTING_SIZE;
            }
            timeSinceLastIncomingSetting = millis();
        }
    }
    else if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE)
    {
        if (settings.debugWebConfig == true)
            systemPrintln("Client closed or refreshed the web page");

        createSettingsString(settingsCSV);
        websocketConnected = false;
    }

    free(buf);
    return ret;
}

static const httpd_uri_t ws = {
        .uri        = "/ws",
        .method     = HTTP_GET,
        .handler    = ws_handler,
        .user_ctx   = NULL,
        .is_websocket = true,
        .handle_ws_control_frames = true,
        .supported_subprotocol = NULL
};

static void start_wsserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Use different ports for websocket and webserver - use port 81 for the websocket - also defined in main.js
    config.server_port = 81;

    // Increase the stack size from 4K to ~15K
    config.stack_size = updateWebSocketStackSize;

    // Start the httpd server
    if (settings.debugWebConfig == true)
        systemPrintf("Starting wsserver on port: %d\r\n", config.server_port);

    if (wsserver == nullptr)
        wsserver = new httpd_handle_t;

    if (httpd_start(wsserver, &config) == ESP_OK) {
        // Registering the ws handler
        if (settings.debugWebConfig == true)
            systemPrintln("Registering URI handlers");
        httpd_register_uri_handler(*wsserver, &ws);
        return;
    }

    systemPrintln("Error starting wsserver!");
}

void stop_wsserver()
{
    createSettingsString(settingsCSV);
    websocketConnected = false;

    if (*wsserver)
    {
        // Stop the httpd server
        esp_err_t ret = httpd_stop(*wsserver);
        //*wsserver = nullptr;
    }
}

// ===== Request Handler class used to answer more complex requests =====
// https://github.com/espressif/arduino-esp32/blob/master/libraries/WebServer/examples/WebServer/WebServer.ino
class CaptiveRequestHandler : public RequestHandler
{
  public:
    // https://en.wikipedia.org/wiki/Captive_portal
    String urls[5] = {"/hotspot-detect.html", "/library/test/success.html", "/generate_204", "/ncsi.txt",
                      "/check_network_status.txt"};
    CaptiveRequestHandler()
    {
        if (settings.debugWebConfig == true)
            systemPrintln("CaptiveRequestHandler is registered");
    }
    virtual ~CaptiveRequestHandler()
    {
    }

    bool canHandle(HTTPMethod requestMethod, String uri) override {
        for (int i = 0; i < sizeof(urls); i++)
        {
            if (uri == urls[i])
                return true;
        }
        return false;
    }

    bool handle(WebServer &server, HTTPMethod requestMethod, String requestUri) override
    {
        String logmessage = "Captive Portal Client:" + server.client().remoteIP().toString() + " " + requestUri;
        if (settings.debugWebConfig == true)
            systemPrintln(logmessage);
        String response = "<!DOCTYPE html><html><head><title>RTK Config</title></head><body>";
        response += "<div class='container'>";
        response += "<div align='center' class='col-sm-12'><img src='http://";
        response += WiFi.softAPIP().toString();
        response += "/src/rtk-setup.png' alt='SparkFun RTK WiFi Setup'></div>";
        response += "<div align='center'><h3>Configure your RTK receiver <a href='http://";
        response += WiFi.softAPIP().toString();
        response += "/'>here</a></h3></div>";
        response += "</div></body></html>";
        server.send(200, "text/html", response);

        return true;
    }
};


// Start webserver in AP mode
bool startWebServer(bool startWiFi = true, int httpPort = 80)
{
    do
    {
        ntripClientStop(true); // Do not allocate new wifiClient
        for (int serverIndex = 0; serverIndex < NTRIP_SERVER_MAX; serverIndex++)
            ntripServerStop(serverIndex, true); // Do not allocate new wifiClient

        if (startWiFi)
            if (wifiStartAP() == false) // Exits calling wifiConnect()
                break;

        // Start the multicast DNS server
        mdnsStartMulticastDNS();

        // Freed by stopWebServer
        if (online.psram == true)
            incomingSettings = (char *)ps_malloc(AP_CONFIG_SETTING_SIZE);
        else
            incomingSettings = (char *)malloc(AP_CONFIG_SETTING_SIZE);

        if (!incomingSettings)
        {
            systemPrintln("ERROR: Failed to allocate incomingSettings");
            break;
        }
        memset(incomingSettings, 0, AP_CONFIG_SETTING_SIZE);

        // Pre-load settings CSV
        // Freed by stopWebServer
        if (online.psram == true)
            settingsCSV = (char *)ps_malloc(AP_CONFIG_SETTING_SIZE);
        else
            settingsCSV = (char *)malloc(AP_CONFIG_SETTING_SIZE);

        if (!settingsCSV)
        {
            systemPrintln("ERROR: Failed to allocate settingsCSV");
            break;
        }
        createSettingsString(settingsCSV);

        // https://github.com/espressif/arduino-esp32/blob/master/libraries/DNSServer/examples/CaptivePortal/CaptivePortal.ino
        if (settings.enableCaptivePortal == true)
        {
            dnsserver = new DNSServer;
            dnsserver->start();
        }

        webserver = new WebServer(httpPort);
        // TODO: webserver = new WebServer(WiFi.localIP(), httpPort);
        // TODO: webserver = new WebServer(ETH.localIP(), httpPort);

        if (!webserver)
        {
            systemPrintln("ERROR: Failed to allocate webserver");
            break;
        }

        if (settings.enableCaptivePortal == true)
        {
            webserver->addHandler(new CaptiveRequestHandler());

            // TODO: add a handler for /connecttest.txt
        }

        // * index.html (not gz'd)
        // * favicon.ico

        // * /src/bootstrap.bundle.min.js - Needed for popper
        // * /src/bootstrap.min.css
        // * /src/bootstrap.min.js
        // * /src/jquery-3.6.0.min.js
        // * /src/main.js (not gz'd)
        // * /src/rtk-setup.png
        // * /src/style.css

        // * /src/fonts/icomoon.eot
        // * /src/fonts/icomoon.svg
        // * /src/fonts/icomoon.ttf
        // * /src/fonts/icomoon.woof

        // * /listfiles responds with a CSV of files and sizes in root
        // * /listMessages responds with a CSV of messages supported by this platform
        // * /listMessagesBase responds with a CSV of RTCM Base messages supported by this platform
        // * /file allows the download or deletion of a file

        webserver->onNotFound(notFound);

        webserver->on("/", HTTP_GET, []() {
            webserver->sendHeader("Content-Encoding", "gzip");
            webserver->send_P(200, "text/html", (const char *)index_html, sizeof(index_html));
        });

        webserver->on("/favicon.ico", HTTP_GET, []() {
            webserver->sendHeader("Content-Encoding", "gzip");
            webserver->send_P(200, "text/plain", (const char *)favicon_ico, sizeof(favicon_ico));
        });

        webserver->on("/src/bootstrap.bundle.min.js", HTTP_GET, []() {
            webserver->sendHeader("Content-Encoding", "gzip");
            webserver->send_P(200, "text/javascript", (const char *)bootstrap_bundle_min_js, sizeof(bootstrap_bundle_min_js));
        });

        webserver->on("/src/bootstrap.min.css", HTTP_GET, []() {
            webserver->sendHeader("Content-Encoding", "gzip");
            webserver->send_P(200, "text/css", (const char *)bootstrap_min_css, sizeof(bootstrap_min_css));
        });

        webserver->on("/src/bootstrap.min.js", HTTP_GET, []() {
            webserver->sendHeader("Content-Encoding", "gzip");
            webserver->send_P(200, "text/javascript", (const char *)bootstrap_min_js, sizeof(bootstrap_min_js));
        });

        webserver->on("/src/jquery-3.6.0.min.js", HTTP_GET, []() {
            webserver->sendHeader("Content-Encoding", "gzip");
            webserver->send_P(200, "text/javascript", (const char *)jquery_js, sizeof(jquery_js));
        });

        webserver->on("/src/main.js", HTTP_GET, []() {
            webserver->sendHeader("Content-Encoding", "gzip");
            webserver->send_P(200, "text/javascript", (const char *)main_js, sizeof(main_js));
        });

        webserver->on("/src/rtk-setup.png", HTTP_GET, []() {
            webserver->sendHeader("Content-Encoding", "gzip");
            if (productVariant == RTK_EVK)
                webserver->send_P(200, "image/png", (const char *)rtkSetup_png, sizeof(rtkSetup_png));
            else
                webserver->send_P(200, "image/png", (const char *)rtkSetupWiFi_png, sizeof(rtkSetupWiFi_png));
        });

        // Battery icons
        webserver->on("/src/BatteryBlank.png", HTTP_GET, []() {
            webserver->sendHeader("Content-Encoding", "gzip");
            webserver->send_P(200, "image/png", (const char *)batteryBlank_png, sizeof(batteryBlank_png));
        });
        webserver->on("/src/Battery0.png", HTTP_GET, []() {
            webserver->sendHeader("Content-Encoding", "gzip");
            webserver->send_P(200, "image/png", (const char *)battery0_png, sizeof(battery0_png));
        });
        webserver->on("/src/Battery1.png", HTTP_GET, []() {
            webserver->sendHeader("Content-Encoding", "gzip");
            webserver->send_P(200, "image/png", (const char *)battery1_png, sizeof(battery1_png));
        });
        webserver->on("/src/Battery2.png", HTTP_GET, []() {
            webserver->sendHeader("Content-Encoding", "gzip");
            webserver->send_P(200, "image/png", (const char *)battery2_png, sizeof(battery2_png));
        });
        webserver->on("/src/Battery3.png", HTTP_GET, []() {
            webserver->sendHeader("Content-Encoding", "gzip");
            webserver->send_P(200, "image/png", (const char *)battery3_png, sizeof(battery3_png));
        });

        webserver->on("/src/Battery0_Charging.png", HTTP_GET, []() {
            webserver->sendHeader("Content-Encoding", "gzip");
            webserver->send_P(200, "image/png", (const char *)battery0_Charging_png, sizeof(battery0_Charging_png));
        });
        webserver->on("/src/Battery1_Charging.png", HTTP_GET, []() {
            webserver->sendHeader("Content-Encoding", "gzip");
            webserver->send_P(200, "image/png", (const char *)battery1_Charging_png, sizeof(battery1_Charging_png));
        });
        webserver->on("/src/Battery2_Charging.png", HTTP_GET, []() {
            webserver->sendHeader("Content-Encoding", "gzip");
            webserver->send_P(200, "image/png", (const char *)battery2_Charging_png, sizeof(battery2_Charging_png));
        });
        webserver->on("/src/Battery3_Charging.png", HTTP_GET, []() {
            webserver->sendHeader("Content-Encoding", "gzip");
            webserver->send_P(200, "image/png", (const char *)battery3_Charging_png, sizeof(battery3_Charging_png));
        });

        webserver->on("/src/style.css", HTTP_GET, []() {
            webserver->sendHeader("Content-Encoding", "gzip");
            webserver->send_P(200, "text/css", (const char *)style_css, sizeof(style_css));
        });

        webserver->on("/src/fonts/icomoon.eot", HTTP_GET, []() {
            webserver->sendHeader("Content-Encoding", "gzip");
            webserver->send_P(200, "text/plain", (const char *)icomoon_eot, sizeof(icomoon_eot));
        });

        webserver->on("/src/fonts/icomoon.svg", HTTP_GET, []() {
            webserver->sendHeader("Content-Encoding", "gzip");
            webserver->send_P(200, "text/plain", (const char *)icomoon_svg, sizeof(icomoon_svg));
        });

        webserver->on("/src/fonts/icomoon.ttf", HTTP_GET, []() {
            webserver->sendHeader("Content-Encoding", "gzip");
            webserver->send_P(200, "text/plain", (const char *)icomoon_ttf, sizeof(icomoon_ttf));
        });

        webserver->on("/src/fonts/icomoon.woof", HTTP_GET, []() {
            webserver->sendHeader("Content-Encoding", "gzip");
            webserver->send_P(200, "text/plain", (const char *)icomoon_woof, sizeof(icomoon_woof));
        });

        // Handler for the /uploadFile form POST
        webserver->on(
            "/uploadFile", HTTP_POST,
            []() {
                webserver->send(200, "text/plain", "");
            },
            handleUpload); // Run handleUpload function when file manager file is uploaded

        // Handler for the /uploadFirmware form POST
        webserver->on(
            "/uploadFirmware", HTTP_POST, 
            []() {
                webserver->send(200, "text/plain", "");
            },
            handleFirmwareFileUpload);

        // Handler for file manager
        webserver->on("/listfiles", HTTP_GET, []() {
            String logmessage = "Client:" + webserver->client().remoteIP().toString() + " " + webserver->uri();
            if (settings.debugWebConfig == true)
                systemPrintln(logmessage);
            String files;
            getFileList(files);
            webserver->send(200, "text/plain", files);
        });

        // Handler for supported messages list
        webserver->on("/listMessages", HTTP_GET, []() {
            String logmessage = "Client:" + webserver->client().remoteIP().toString() + " " + webserver->uri();
            if (settings.debugWebConfig == true)
                systemPrintln(logmessage);
            String messages;
            createMessageList(messages);
            if (settings.debugWebConfig == true)
                systemPrintln(messages);
            webserver->send(200, "text/plain", messages);
        });

        // Handler for supported RTCM/Base messages list
        webserver->on("/listMessagesBase", HTTP_GET, []() {
            String logmessage = "Client:" + webserver->client().remoteIP().toString() + " " + webserver->uri();
            if (settings.debugWebConfig == true)
                systemPrintln(logmessage);
            String messageList;
            createMessageListBase(messageList);
            if (settings.debugWebConfig == true)
                systemPrintln(messageList);
            webserver->send(200, "text/plain", messageList);
        });

        // Handler for file manager
        webserver->on("/file", HTTP_GET, handleFileManager);

        webserver->begin();

        // Starts task for updating webserver with handleClient
        if (task.updateWebServerTaskRunning == false)
            xTaskCreate(updateWebServerTask,
                        "UpdateWebServer",            // Just for humans
                        updateWebServerTaskStackSize, // Stack Size - needs to be large enough to hold the file manager list
                        nullptr,                // Task input parameter
                        updateWebServerTaskPriority,
                        &updateWebServerTaskHandle); // Task handle

        if (settings.debugWebConfig == true)
            systemPrintln("Web Server Started");
        reportHeapNow(false);

        // Start the web socket server on port 81 using <esp_http_server.h>
        start_wsserver();

        if (settings.debugWebConfig == true)
            systemPrintln("Web Socket Server Started");
        reportHeapNow(false);

        return true;
    } while (0);

    // Release the resources
    stop_wsserver();
    stopWebServer();
    return false;
}

void updateWebServerTask(void *e)
{
    // Start notification
    task.updateWebServerTaskRunning = true;
    if (settings.printTaskStartStop)
        systemPrintln("Task updateWebServerTask started");

    // Verify that the task is still running
    task.updateWebServerTaskStopRequest = false;
    while (task.updateWebServerTaskStopRequest == false)
    {
        // Display an alive message
        if (PERIODIC_DISPLAY(PD_TASK_UPDATE_WEBSERVER))
        {
            PERIODIC_CLEAR(PD_TASK_UPDATE_WEBSERVER);
            systemPrintln("updateWebServerTask running");
        }

        webserver->handleClient();

        feedWdt();
        taskYIELD();
    }

    // Stop notification
    if (settings.printTaskStartStop)
        systemPrintln("Task updateWebServerTask stopped");
    task.updateWebServerTaskRunning = false;
    vTaskDelete(updateWebServerTaskHandle);
}

void stopWebServer()
{
    if (task.updateWebServerTaskRunning)
        task.updateWebServerTaskStopRequest = true;

    if (webserver != nullptr)
    {
        webserver->close();
        free(webserver);
        webserver = nullptr;
    }

    // Stop the multicast DNS server
    mdnsStopMulticastDNS();

    if (settingsCSV != nullptr)
    {
        free(settingsCSV);
        settingsCSV = nullptr;
    }

    if (incomingSettings != nullptr)
    {
        free(incomingSettings);
        incomingSettings = nullptr;
    }

    do
        delay(10);
    while (task.updateWebServerTaskRunning);
}

void notFound()
{
    String logmessage = "notFound: Client:" + webserver->client().remoteIP().toString() + " " + webserver->uri();
    systemPrintln(logmessage);
    webserver->send(404, "text/plain", "Not found");
}

// Handler for firmware file downloads
static void handleFileManager()
{
    // This section does not tolerate semaphore transactions
    String logmessage = "handleFileManager: Client:" + webserver->client().remoteIP().toString() + " " + webserver->uri();

    if (webserver->hasArg("name") && webserver->hasArg("action"))
    {
        String fileName = webserver->arg("name");
        String fileAction = webserver->arg("action");

        logmessage = "Client:" + webserver->client().remoteIP().toString() + " " + webserver->uri() +
                     "?name=" + fileName + "&action=" + fileAction;

        char slashFileName[60];
        snprintf(slashFileName, sizeof(slashFileName), "/%s", fileName.c_str());

        bool fileExists = sd->exists(slashFileName);

        if (fileExists == false)
        {
            logmessage += " ERROR: file ";
            logmessage += slashFileName;
            logmessage += " does not exist";
            webserver->send(400, "text/plain", "ERROR: file does not exist");
        }
        else
        {
            logmessage += " file exists";

            if (fileAction == "download")
            {
                logmessage += " downloaded";

                // This would be SO much easier with webserver->streamFile
                // except streamFile only works with File - not SdFile...
                // TODO: if we ever upgrade to SD from SdFat, replace this with streamFile

                if (managerFileOpen == false)
                {
                    // Allocate the managerTempFile
                    if (!managerTempFile)
                    {
                        managerTempFile = new SdFile;
                        if (!managerTempFile)
                        {
                            systemPrintln("Failed to allocate managerTempFile!");
                            return;
                        }
                    }

                    if (managerTempFile->open(slashFileName, O_READ) == true)
                        managerFileOpen = true;
                    else
                        systemPrintln("Error: File Manager failed to open file");
                }
                else
                {
                    // File is already in use. Wait your turn.
                    webserver->send(202, "text/plain", "ERROR: File already downloading");
                }

                const size_t maxLen = 8192;
                uint8_t *buf = new uint8_t[maxLen];

                size_t dataAvailable = managerTempFile->size();

                bool firstSend = true;

                while (dataAvailable > 0)
                {
                    size_t sending;

                    if (dataAvailable > maxLen)
                    {
                        sending = managerTempFile->read(buf, maxLen);
                    }
                    else
                    {
                        sending = managerTempFile->read(buf, dataAvailable);
                    }

                    if (firstSend) // First send?
                    {
                        webserver->setContentLength(dataAvailable);
                        webserver->sendHeader("Cache-Control", "no-cache");
                        webserver->sendHeader("Content-Disposition", "attachment; filename=" + String(fileName));
                        webserver->sendHeader("Access-Control-Allow-Origin", "*");
                        webserver->send(200, "application/octet-stream", "");
                        firstSend = false;
                    }

                    webserver->sendContent((const char *)buf, sending);

                    if (sending < maxLen) // Last send?
                    {
                        managerTempFile->close();

                        managerFileOpen = false;

                        sendStringToWebsocket("fmNext,1,"); // Tell browser to send next file if needed
                    }
                
                    dataAvailable -= sending;
                }

                delete[] buf;
            }
            else if (fileAction == "delete")
            {
                logmessage += " deleted";
                sd->remove(slashFileName);
                webserver->send(200, "text/plain", "Deleted File: " + fileName);
            }
            else
            {
                logmessage += " ERROR: invalid action param supplied";
                webserver->send(400, "text/plain", "ERROR: invalid action param supplied");
            }
        }
        systemPrintln(logmessage);
    }
    else
    {
        webserver->send(400, "text/plain", "ERROR: name and action params required");
    }
}

// Handler for firmware file upload
static void handleFirmwareFileUpload()
{
    String fileName = "";

    HTTPUpload &upload = webserver->upload();

    if (upload.status == UPLOAD_FILE_START)
    {
        // Check file name against valid firmware names
        const char *BIN_EXT = "bin";
        const char *BIN_HEADER = "RTK_Everywhere_Firmware";

        fileName = upload.filename;

        int fnameLen = fileName.length();
        char fname[fnameLen + 2] = {'/'}; // Filename must start with / or VERY bad things happen on SD_MMC
        fileName.toCharArray(&fname[1], fnameLen + 1);
        fname[fnameLen + 1] = '\0'; // Terminate array

        // Check 'bin' extension
        if (strcmp(BIN_EXT, &fname[strlen(fname) - strlen(BIN_EXT)]) == 0)
        {
            // Check for 'RTK_Everywhere_Firmware' start of file name
            if (strncmp(fname, BIN_HEADER, strlen(BIN_HEADER)) == 0)
            {
                // Begin update process
                if (!Update.begin(UPDATE_SIZE_UNKNOWN))
                {
                    Update.printError(Serial);
                    webserver->send(400, "text/plain", "OTA could not begin");
                    return;
                }
            }
            else
            {
                systemPrintf("handleFirmwareFileUpload: Unknown: %s\r\n", fname);
                webserver->send(400, "text/html", "<b>Error:</b> Unknown file type");
                return;
            }
        }
        else
        {
            systemPrintf("handleFirmwareFileUpload: Unknown: %s\r\n", fname);
            webserver->send(400, "text/html", "<b>Error:</b> Unknown file type");
            return;
        }
    }

    // Write chunked data to the free sketch space
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
        {
            webserver->send(400, "text/plain", "OTA could not begin");
            return;
        }
        else
        {
            binBytesSent = upload.currentSize;

            // Send an update to browser every 100k
            if (binBytesSent - binBytesLastUpdate > 100000)
            {
                binBytesLastUpdate = binBytesSent;

                char bytesSentMsg[100];
                snprintf(bytesSentMsg, sizeof(bytesSentMsg), "%'d bytes sent", binBytesSent);

                char statusMsg[200] = {'\0'};
                stringRecord(statusMsg, "firmwareUploadStatus",
                             bytesSentMsg); // Convert to "firmwareUploadMsg,11214 bytes sent,"

                systemPrintf("msg: %s\r\n", statusMsg);
                sendStringToWebsocket(statusMsg);
            }
        }
    }

    else if (upload.status == UPLOAD_FILE_END)
    {
        if (!Update.end(true))
        {
            Update.printError(Serial);
            webserver->send(400, "text/plain", "Could not end OTA");
            return;
        }
        else
        {
            sendStringToWebsocket("firmwareUploadComplete,1,");
            systemPrintln("Firmware update complete. Restarting");
            delay(500);
            ESP.restart();
        }
    }
}

/*
// TODO: delete this. This is the old method - using AsyncWebSocketClient
// Events triggered by web sockets
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data,
               size_t len)
{
    if (type == WS_EVT_CONNECT)
    {
        if (settings.debugWebConfig == true)
            systemPrintln("Websocket client connected");
        client->text(settingsCSV);
        lastDynamicDataUpdate = millis();
        websocketConnected = true;
    }
    else if (type == WS_EVT_DISCONNECT)
    {
        if (settings.debugWebConfig == true)
            systemPrintln("Websocket client disconnected");

        // User has either refreshed the page or disconnected. Recompile the current settings.
        createSettingsString(settingsCSV);
        websocketConnected = false;
    }
    else if (type == WS_EVT_DATA)
    {
        if (currentlyParsingData == false)
        {
            for (int i = 0; i < len; i++)
            {
                incomingSettings[incomingSettingsSpot++] = data[i];
                incomingSettingsSpot %= AP_CONFIG_SETTING_SIZE;
            }
            timeSinceLastIncomingSetting = millis();
        }
    }
    else
    {
        if (settings.debugWebConfig == true)
            systemPrintf("onWsEvent: unrecognised AwsEventType %d\r\n", type);
    }
}
*/

// Create a csv string with the dynamic data to update (current coordinates, battery level, etc)
void createDynamicDataString(char *settingsCSV)
{
    settingsCSV[0] = '\0'; // Erase current settings string

    // Current coordinates come from HPPOSLLH call back
    stringRecord(settingsCSV, "geodeticLat", gnssGetLatitude(), haeNumberOfDecimals);
    stringRecord(settingsCSV, "geodeticLon", gnssGetLongitude(), haeNumberOfDecimals);
    stringRecord(settingsCSV, "geodeticAlt", gnssGetAltitude(), 3);

    double ecefX = 0;
    double ecefY = 0;
    double ecefZ = 0;

    geodeticToEcef(gnssGetLatitude(), gnssGetLongitude(), gnssGetAltitude(), &ecefX, &ecefY, &ecefZ);

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

// Break CSV into setting constituents
// Can't use strtok because we may have two commas next to each other, ie
// measurementRateHz,4.00,measurementRateSec,,dynamicModel,0,
bool parseIncomingSettings()
{
    char settingName[100] = {'\0'};
    char valueStr[150] = {'\0'}; // stationGeodetic1,ANameThatIsTooLongToBeDisplayed 40.09029479 -105.18505761 1560.089

    char *commaPtr = incomingSettings;
    char *headPtr = incomingSettings;

    int counter = 0;
    int maxAttempts = 500;
    while (*headPtr) // Check if we've reached the end of the string
    {
        // Spin to first comma
        commaPtr = strstr(headPtr, ",");
        if (commaPtr != nullptr)
        {
            *commaPtr = '\0';
            strcpy(settingName, headPtr);
            headPtr = commaPtr + 1;
        }

        commaPtr = strstr(headPtr, ",");
        if (commaPtr != nullptr)
        {
            *commaPtr = '\0';
            strcpy(valueStr, headPtr);
            headPtr = commaPtr + 1;
        }

        if (settings.debugWebConfig == true)
            systemPrintf("settingName: %s value: %s\r\n", settingName, valueStr);

        updateSettingWithValue(false, settingName, valueStr);

        // Avoid infinite loop if response is malformed
        counter++;
        if (counter == maxAttempts)
        {
            systemPrintln("Error: Incoming settings malformed.");
            break;
        }
    }

    if (counter < maxAttempts)
    {
        // Confirm receipt
        if (settings.debugWebConfig == true)
            systemPrintln("Sending receipt confirmation of settings");
        sendStringToWebsocket("confirmDataReceipt,1,");
    }

    return (true);
}

// When called, responds with the root folder list of files on SD card
// Name and size are formatted in CSV, formatted to html by JS
void getFileList(String &returnText)
{
    returnText = "";

    // Update the SD Size and Free Space
    String cardSize;
    stringHumanReadableSize(cardSize, sdCardSize);
    returnText += "sdSize," + cardSize + ",";
    String freeSpace;
    stringHumanReadableSize(freeSpace, sdFreeSpace);
    returnText += "sdFreeSpace," + freeSpace + ",";

    char fileName[50]; // Handle long file names

    // Attempt to gain access to the SD card
    if (xSemaphoreTake(sdCardSemaphore, fatSemaphore_longWait_ms) == pdPASS)
    {
        markSemaphore(FUNCTION_FILEMANAGER_UPLOAD1);

        SdFile root;
        root.open("/"); // Open root
        SdFile file;
        uint16_t fileCount = 0;

        while (file.openNext(&root, O_READ))
        {
            if (file.isFile())
            {
                fileCount++;

                file.getName(fileName, sizeof(fileName));

                String fileSize;
                stringHumanReadableSize(fileSize, file.fileSize());
                returnText += "fmName," + String(fileName) + ",fmSize," + fileSize + ",";
            }
        }

        root.close();
        file.close();

        xSemaphoreGive(sdCardSemaphore);
    }
    else
    {
        char semaphoreHolder[50];
        getSemaphoreFunction(semaphoreHolder);

        // This is an error because the current settings no longer match the settings
        // on the microSD card, and will not be restored to the expected settings!
        systemPrintf("sdCardSemaphore failed to yield, held by %s, Form.ino line %d\r\n", semaphoreHolder, __LINE__);
    }

    if (settings.debugWebConfig == true)
        systemPrintf("returnText (%d bytes): %s\r\n", returnText.length(), returnText.c_str());
}

// When called, responds with the messages supported on this platform
// Message name and current rate are formatted in CSV, formatted to html by JS
void createMessageList(String &returnText)
{
    returnText = "";

    if (present.gnss_zedf9p)
    {
        for (int messageNumber = 0; messageNumber < MAX_UBX_MSG; messageNumber++)
        {
            if (messageSupported(messageNumber) == true)
                returnText += "ubxMessageRate_" + String(ubxMessages[messageNumber].msgTextName) + "," +
                              String(settings.ubxMessageRates[messageNumber]) + ",";
        }
    }

    else if (present.gnss_um980)
    {
        for (int messageNumber = 0; messageNumber < MAX_UM980_NMEA_MSG; messageNumber++)
        {
            returnText += "messageRateNMEA_" + String(umMessagesNMEA[messageNumber].msgTextName) + "," +
                          String(settings.um980MessageRatesNMEA[messageNumber]) + ",";
        }
        for (int messageNumber = 0; messageNumber < MAX_UM980_RTCM_MSG; messageNumber++)
        {
            returnText += "messageRateRTCMRover_" + String(umMessagesRTCM[messageNumber].msgTextName) + "," +
                          String(settings.um980MessageRatesRTCMRover[messageNumber]) + ",";
        }
    }

    if (settings.debugWebConfig == true)
        systemPrintf("returnText (%d bytes): %s\r\n", returnText.length(), returnText.c_str());
}

// When called, responds with the RTCM/Base messages supported on this platform
// Message name and current rate are formatted in CSV, formatted to html by JS
void createMessageListBase(String &returnText)
{
    returnText = "";

    if (present.gnss_zedf9p)
    {
        int firstRTCMRecord = zedGetMessageNumberByName("RTCM_1005");

        for (int messageNumber = 0; messageNumber < MAX_UBX_MSG_RTCM; messageNumber++)
        {
            if (messageSupported(firstRTCMRecord + messageNumber) == true)
                returnText += "ubxMessageRateBase_" + String(ubxMessages[messageNumber + firstRTCMRecord].msgTextName) +
                              "," + String(settings.ubxMessageRatesBase[messageNumber]) + ","; // UBX_RTCM_1074Base,4,
        }
    }

    else if (present.gnss_um980)
    {
        for (int messageNumber = 0; messageNumber < MAX_UM980_RTCM_MSG; messageNumber++)
        {
            returnText += "messageRateRTCMBase_" + String(umMessagesRTCM[messageNumber].msgTextName) + "," +
                          String(settings.um980MessageRatesRTCMBase[messageNumber]) + ",";
        }
    }

    if (settings.debugWebConfig == true)
        systemPrintf("returnText (%d bytes): %s\r\n", returnText.length(), returnText.c_str());
}

// Handles uploading of user files to SD
// https://github.com/espressif/arduino-esp32/blob/master/libraries/WebServer/examples/FSBrowser/FSBrowser.ino
void handleUpload()
{
    HTTPUpload &upload = webserver->upload();

    if (upload.status == UPLOAD_FILE_START)
    {
        String filename = upload.filename;

        String logmessage = "Upload Start: " + filename;

        int fileNameLen = filename.length();
        char tempFileName[fileNameLen + 2] = {'/'}; // Filename must start with / or VERY bad things happen on SD_MMC
        filename.toCharArray(&tempFileName[1], fileNameLen + 1);
        tempFileName[fileNameLen + 1] = '\0'; // Terminate array

        // Allocate the managerTempFile
        if (!managerTempFile)
        {
            managerTempFile = new SdFile;
            if (!managerTempFile)
            {
                systemPrintln("Failed to allocate managerTempFile!");
                return;
            }
        }

        if (managerFileOpen == false)
        {
            // Attempt to gain access to the SD card
            if (xSemaphoreTake(sdCardSemaphore, fatSemaphore_longWait_ms) == pdPASS)
            {
                markSemaphore(FUNCTION_FILEMANAGER_UPLOAD1);

                if (managerTempFile->open(tempFileName, O_CREAT | O_APPEND | O_WRITE) == true)
                    managerFileOpen = true;
                else
                    systemPrintln("Error: handleUpload failed to open file");

                xSemaphoreGive(sdCardSemaphore);
            }
        }
        else
        {
            // File is already in use. Wait your turn.
            webserver->send(202, "text/plain", "ERROR: File already uploading");
        }


        systemPrintln(logmessage);
    }

    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        // Attempt to gain access to the SD card
        if (xSemaphoreTake(sdCardSemaphore, fatSemaphore_longWait_ms) == pdPASS)
        {
            markSemaphore(FUNCTION_FILEMANAGER_UPLOAD2);

            managerTempFile->write(upload.buf, upload.currentSize); // stream the incoming chunk to the opened file

            xSemaphoreGive(sdCardSemaphore);
        }
    }

    else if (upload.status == UPLOAD_FILE_END)
    {
        String logmessage = "Upload Complete: " + String(upload.filename) + ", size: " + String(upload.totalSize);

        // Attempt to gain access to the SD card
        if (xSemaphoreTake(sdCardSemaphore, fatSemaphore_longWait_ms) == pdPASS)
        {
            markSemaphore(FUNCTION_FILEMANAGER_UPLOAD3);

            sdUpdateFileCreateTimestamp(managerTempFile); // Update the file create time & date

            managerTempFile->close();
            managerFileOpen = false;

            xSemaphoreGive(sdCardSemaphore);
        }

        systemPrintln(logmessage);

        // Redirect to "/"
        webserver->sendHeader("Location", "/");
        webserver->send(302, "text/plain", "");
    }
}

#endif // COMPILE_AP
