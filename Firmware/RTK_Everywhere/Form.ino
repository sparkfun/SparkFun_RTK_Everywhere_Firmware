/*------------------------------------------------------------------------------
Form.ino

  Start and stop the web-server, provide the form and handle browser input.
------------------------------------------------------------------------------*/

#ifdef COMPILE_AP

// Once connected to the access point for WiFi Config, the ESP32 sends current setting values in one long string to
// websocket After user clicks 'save', data is validated via main.js and a long string of values is returned.

bool websocketConnected = false;

class CaptiveRequestHandler : public AsyncWebHandler
{
  public:
    // https://en.wikipedia.org/wiki/Captive_portal
    String urls[5] = {"/hotspot-detect.html", "/library/test/success.html", "/generate_204", "/ncsi.txt",
                      "/check_network_status.txt"};
    CaptiveRequestHandler()
    {
    }
    virtual ~CaptiveRequestHandler()
    {
    }

    bool canHandle(AsyncWebServerRequest *request)
    {
        for (int i = 0; i < 5; i++)
        {
            if (request->url().equals(urls[i]))
                return true;
        }
        return false;
    }

    // Provide a custom small site for redirecting the user to the config site
    // HTTP redirect does not work and the relative links on the default config site do not work, because the phone is
    // requesting a different server
    void handleRequest(AsyncWebServerRequest *request)
    {
        String logmessage = "Captive Portal Client:" + request->client()->remoteIP().toString() + " " + request->url();
        systemPrintln(logmessage);
        AsyncResponseStream *response = request->beginResponseStream("text/html");
        response->print("<!DOCTYPE html><html><head><title>RTK Config</title></head><body>");
        response->print("<div class='container'>");
        response->printf("<div align='center' class='col-sm-12'><img src='http://%s/src/rtk-setup.png' alt='SparkFun "
                         "RTK WiFi Setup'></div>",
                         WiFi.softAPIP().toString().c_str());
        response->printf("<div align='center'><h3>Configure your RTK receiver <a href='http://%s/'>here</a></h3></div>",
                         WiFi.softAPIP().toString().c_str());
        response->print("</div></body></html>");
        request->send(response);
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

        if (settings.mdnsEnable == true)
        {
            if (MDNS.begin("rtk") == false) // This should make the module findable from 'rtk.local' in browser
                systemPrintln("Error setting up MDNS responder!");
            else
                MDNS.addService("http", "tcp", 80); // Add service to MDNS-SD
        }

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

        webserver = new AsyncWebServer(httpPort);
        if (!webserver)
        {
            systemPrintln("ERROR: Failed to allocate webserver");
            break;
        }
        websocket = new AsyncWebSocket("/ws");
        if (!websocket)
        {
            systemPrintln("ERROR: Failed to allocate websocket");
            break;
        }

        if (settings.enableCaptivePortal == true)
            webserver->addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER); // only when requested from AP

        websocket->onEvent(onWsEvent);
        webserver->addHandler(websocket);

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
        // * /listCorrections responds with a CSV of correction sources and their priorities
        // * /listMessages responds with a CSV of messages supported by this platform
        // * /listMessagesBase responds with a CSV of RTCM Base messages supported by this platform
        // * /file allows the download or deletion of a file

        webserver->onNotFound(notFound);

        webserver->onFileUpload(
            handleUpload); // Run handleUpload function when any file is uploaded. Must be before server.on() calls.

        webserver->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response =
                request->beginResponse_P(200, "text/html", index_html, sizeof(index_html));
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        });

        webserver->on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response =
                request->beginResponse_P(200, "text/plain", favicon_ico, sizeof(favicon_ico));
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        });

        webserver->on("/src/bootstrap.bundle.min.js", HTTP_GET, [](AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response = request->beginResponse_P(200, "text/javascript", bootstrap_bundle_min_js,
                                                                        sizeof(bootstrap_bundle_min_js));
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        });

        webserver->on("/src/bootstrap.min.css", HTTP_GET, [](AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response =
                request->beginResponse_P(200, "text/css", bootstrap_min_css, sizeof(bootstrap_min_css));
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        });

        webserver->on("/src/bootstrap.min.js", HTTP_GET, [](AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response =
                request->beginResponse_P(200, "text/javascript", bootstrap_min_js, sizeof(bootstrap_min_js));
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        });

        webserver->on("/src/jquery-3.6.0.min.js", HTTP_GET, [](AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response =
                request->beginResponse_P(200, "text/javascript", jquery_js, sizeof(jquery_js));
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        });

        webserver->on("/src/main.js", HTTP_GET, [](AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response =
                request->beginResponse_P(200, "text/javascript", main_js, sizeof(main_js));
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        });

        webserver->on("/src/rtk-setup.png", HTTP_GET, [](AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response;
            if (productVariant == RTK_EVK)
                response = request->beginResponse_P(200, "image/png", rtkSetup_png, sizeof(rtkSetup_png));
            else
                response = request->beginResponse_P(200, "image/png", rtkSetupWiFi_png, sizeof(rtkSetupWiFi_png));
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        });

        // Battery icons
        webserver->on("/src/BatteryBlank.png", HTTP_GET, [](AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response =
                request->beginResponse_P(200, "image/png", batteryBlank_png, sizeof(batteryBlank_png));
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        });
        webserver->on("/src/Battery0.png", HTTP_GET, [](AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response =
                request->beginResponse_P(200, "image/png", battery0_png, sizeof(battery0_png));
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        });
        webserver->on("/src/Battery1.png", HTTP_GET, [](AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response =
                request->beginResponse_P(200, "image/png", battery1_png, sizeof(battery1_png));
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        });
        webserver->on("/src/Battery2.png", HTTP_GET, [](AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response =
                request->beginResponse_P(200, "image/png", battery2_png, sizeof(battery2_png));
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        });
        webserver->on("/src/Battery3.png", HTTP_GET, [](AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response =
                request->beginResponse_P(200, "image/png", battery3_png, sizeof(battery3_png));
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        });

        webserver->on("/src/Battery0_Charging.png", HTTP_GET, [](AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response =
                request->beginResponse_P(200, "image/png", battery0_Charging_png, sizeof(battery0_Charging_png));
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        });
        webserver->on("/src/Battery1_Charging.png", HTTP_GET, [](AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response =
                request->beginResponse_P(200, "image/png", battery1_Charging_png, sizeof(battery1_Charging_png));
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        });
        webserver->on("/src/Battery2_Charging.png", HTTP_GET, [](AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response =
                request->beginResponse_P(200, "image/png", battery2_Charging_png, sizeof(battery2_Charging_png));
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        });
        webserver->on("/src/Battery3_Charging.png", HTTP_GET, [](AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response =
                request->beginResponse_P(200, "image/png", battery3_Charging_png, sizeof(battery3_Charging_png));
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        });

        webserver->on("/src/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response = request->beginResponse_P(200, "text/css", style_css, sizeof(style_css));
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        });

        webserver->on("/src/fonts/icomoon.eot", HTTP_GET, [](AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response =
                request->beginResponse_P(200, "text/plain", icomoon_eot, sizeof(icomoon_eot));
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        });

        webserver->on("/src/fonts/icomoon.svg", HTTP_GET, [](AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response =
                request->beginResponse_P(200, "text/plain", icomoon_svg, sizeof(icomoon_svg));
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        });

        webserver->on("/src/fonts/icomoon.ttf", HTTP_GET, [](AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response =
                request->beginResponse_P(200, "text/plain", icomoon_ttf, sizeof(icomoon_ttf));
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        });

        webserver->on("/src/fonts/icomoon.woof", HTTP_GET, [](AsyncWebServerRequest *request) {
            AsyncWebServerResponse *response =
                request->beginResponse_P(200, "text/plain", icomoon_woof, sizeof(icomoon_woof));
            response->addHeader("Content-Encoding", "gzip");
            request->send(response);
        });

        // Handler for the /upload form POST
        webserver->on(
            "/upload", HTTP_POST, [](AsyncWebServerRequest *request) { request->send(200); }, handleFirmwareFileUpload);

        // Handler for file manager
        webserver->on("/listfiles", HTTP_GET, [](AsyncWebServerRequest *request) {
            String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
            systemPrintln(logmessage);
            String files;
            getFileList(files);
            request->send(200, "text/plain", files);
        });

        /*
        // Handler for corrections priorities list
        webserver->on("/listCorrections", HTTP_GET, [](AsyncWebServerRequest *request) {
            String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
            systemPrintln(logmessage);
            String corrections;
            createCorrectionsList(corrections);
            request->send(200, "text/plain", corrections);
        });
        */

        // Handler for supported messages list
        webserver->on("/listMessages", HTTP_GET, [](AsyncWebServerRequest *request) {
            String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
            systemPrintln(logmessage);
            String messages;
            createMessageList(messages);
            systemPrintln(messages);
            request->send(200, "text/plain", messages);
        });

        // Handler for supported RTCM/Base messages list
        webserver->on("/listMessagesBase", HTTP_GET, [](AsyncWebServerRequest *request) {
            String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
            systemPrintln(logmessage);
            String messageList;
            createMessageListBase(messageList);
            systemPrintln(messageList);
            request->send(200, "text/plain", messageList);
        });

        // Handler for file manager
        webserver->on("/file", HTTP_GET, [](AsyncWebServerRequest *request) { handleFileManager(request); });

        webserver->begin();

        if (settings.debugWiFiConfig == true)
            systemPrintln("Web Server Started");
        reportHeapNow(false);
        return true;
    } while (0);

    // Release the resources
    stopWebServer();
    return false;
}

void stopWebServer()
{
    if (webserver != nullptr)
    {
        webserver->end();
        free(webserver);
        webserver = nullptr;
    }

    if (websocket != nullptr)
    {
        delete websocket;
        websocket = nullptr;
    }

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

    if (settings.debugWiFiConfig == true)
        systemPrintln("Web Server Stopped");
    reportHeapNow(false);
}

void notFound(AsyncWebServerRequest *request)
{
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();
    systemPrintln(logmessage);
    request->send(404, "text/plain", "Not found");
}

// Handler for firmware file downloads
static void handleFileManager(AsyncWebServerRequest *request)
{
    // This section does not tolerate semaphore transactions
    String logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url();

    if (request->hasParam("name") && request->hasParam("action"))
    {
        const char *fileName = request->getParam("name")->value().c_str();
        const char *fileAction = request->getParam("action")->value().c_str();

        logmessage = "Client:" + request->client()->remoteIP().toString() + " " + request->url() +
                     "?name=" + String(fileName) + "&action=" + String(fileAction);

        char slashFileName[60];
        snprintf(slashFileName, sizeof(slashFileName), "/%s", request->getParam("name")->value().c_str());

        bool fileExists;
        fileExists = sd->exists(slashFileName);

        if (fileExists == false)
        {
            systemPrintln(logmessage + " ERROR: file does not exist");
            request->send(400, "text/plain", "ERROR: file does not exist");
        }
        else
        {
            systemPrintln(logmessage + " file exists");

            if (strcmp(fileAction, "download") == 0)
            {
                logmessage += " downloaded";

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
                    request->send(202, "text/plain", "ERROR: File already downloading");
                }

                int dataAvailable;
                dataAvailable = managerTempFile->size() - managerTempFile->position();

                AsyncWebServerResponse *response = request->beginResponse(
                    "text/plain", dataAvailable, [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
                        uint32_t bytes = 0;
                        uint32_t availableBytes;
                        availableBytes = managerTempFile->available();

                        if (availableBytes > maxLen)
                        {
                            bytes = managerTempFile->read(buffer, maxLen);
                        }
                        else
                        {
                            bytes = managerTempFile->read(buffer, availableBytes);
                            managerTempFile->close();

                            managerFileOpen = false;

                            websocket->textAll("fmNext,1,"); // Tell browser to send next file if needed
                        }

                        return bytes;
                    });

                response->addHeader("Cache-Control", "no-cache");
                response->addHeader("Content-Disposition", "attachment; filename=" + String(fileName));
                response->addHeader("Access-Control-Allow-Origin", "*");
                request->send(response);
            }
            else if (strcmp(fileAction, "delete") == 0)
            {
                logmessage += " deleted";
                sd->remove(slashFileName);
                request->send(200, "text/plain", "Deleted File: " + String(fileName));
            }
            else
            {
                logmessage += " ERROR: invalid action param supplied";
                request->send(400, "text/plain", "ERROR: invalid action param supplied");
            }
            systemPrintln(logmessage);
        }
    }
    else
    {
        request->send(400, "text/plain", "ERROR: name and action params required");
    }
}

// Handler for firmware file upload
static void handleFirmwareFileUpload(AsyncWebServerRequest *request, String fileName, size_t index, uint8_t *data,
                                     size_t len, bool final)
{
    if (!index)
    {
        // Check file name against valid firmware names
        const char *BIN_EXT = "bin";
        const char *BIN_HEADER = "RTK_Everywhere_Firmware";

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
                    return request->send(400, "text/plain", "OTA could not begin");
                }
            }
            else
            {
                systemPrintf("Unknown: %s\r\n", fname);
                return request->send(400, "text/html", "<b>Error:</b> Unknown file type");
            }
        }
        else
        {
            systemPrintf("Unknown: %s\r\n", fname);
            return request->send(400, "text/html", "<b>Error:</b> Unknown file type");
        }
    }

    // Write chunked data to the free sketch space
    if (len)
    {
        if (Update.write(data, len) != len)
            return request->send(400, "text/plain", "OTA could not begin");
        else
        {
            binBytesSent += len;

            // Send an update to browser every 100k
            if (binBytesSent - binBytesLastUpdate > 100000)
            {
                binBytesLastUpdate = binBytesSent;

                char bytesSentMsg[100];
                snprintf(bytesSentMsg, sizeof(bytesSentMsg), "%'d bytes sent", binBytesSent);

                systemPrintf("bytesSentMsg: %s\r\n", bytesSentMsg);

                char statusMsg[200] = {'\0'};
                stringRecord(statusMsg, "firmwareUploadStatus",
                             bytesSentMsg); // Convert to "firmwareUploadMsg,11214 bytes sent,"

                systemPrintf("msg: %s\r\n", statusMsg);
                websocket->textAll(statusMsg);
            }
        }
    }

    if (final)
    {
        if (!Update.end(true))
        {
            Update.printError(Serial);
            return request->send(400, "text/plain", "Could not end OTA");
        }
        else
        {
            websocket->textAll("firmwareUploadComplete,1,");
            systemPrintln("Firmware update complete. Restarting");
            delay(500);
            ESP.restart();
        }
    }
}

// Events triggered by web sockets
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data,
               size_t len)
{
    if (type == WS_EVT_CONNECT)
    {
        if (settings.debugWiFiConfig == true)
            systemPrintln("Websocket client connected");
        client->text(settingsCSV);
        lastDynamicDataUpdate = millis();
        websocketConnected = true;
    }
    else if (type == WS_EVT_DISCONNECT)
    {
        if (settings.debugWiFiConfig == true)
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
        if (settings.debugWiFiConfig == true)
            systemPrintf("onWsEvent: unrecognised AwsEventType %d\r\n", type);
    }
}

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

        // log_d("settingName: %s value: %s", settingName, valueStr);

        updateSettingWithValue(settingName, valueStr);

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
        if (settings.debugWiFiConfig == true)
            systemPrintln("Sending receipt confirmation of settings");
        websocket->textAll("confirmDataReceipt,1,");
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

    if (settings.debugWiFiConfig == true)
        systemPrintf("returnText (%d bytes): %s\r\n", returnText.length(), returnText.c_str());
}

/*
// When called, responds with the corrections sources and their priorities
// Source name and priority are formatted in CSV, formatted to html by JS
void createCorrectionsList(String &returnText)
{
    returnText = "";

    for (int s = 0; s < correctionsSource::CORR_NUM; s++)
    {
        returnText += String("correctionsPriority.") +
                      String(correctionsSourceNames[s]) + "," +
                      String(settings.correctionsSourcesPriority[s]) + ",";
    }

    if (settings.debugWiFiConfig == true)
        systemPrintf("returnText (%d bytes): %s\r\n", returnText.length(), returnText.c_str());
}
*/

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

    if (settings.debugWiFiConfig == true)
        systemPrintf("returnText (%d bytes): %s\r\n", returnText.length(), returnText.c_str());
}

// When called, responds with the RTCM/Base messages supported on this platform
// Message name and current rate are formatted in CSV, formatted to html by JS
void createMessageListBase(String &returnText)
{
    returnText = "";

    if (present.gnss_zedf9p)
    {
        int firstRTCMRecord = zedGetMessageNumberByName("UBX_RTCM_1005");

        for (int messageNumber = 0; messageNumber < MAX_UBX_MSG_RTCM; messageNumber++)
        {
            if (messageSupported(firstRTCMRecord + messageNumber) == true)
                returnText += "messageRateBase_" + String(ubxMessages[messageNumber + firstRTCMRecord].msgTextName) +
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

    if (settings.debugWiFiConfig == true)
        systemPrintf("returnText (%d bytes): %s\r\n", returnText.length(), returnText.c_str());
}

// Handles uploading of user files to SD
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
    String logmessage = "";

    if (!index)
    {
        logmessage = "Upload Start: " + String(filename);

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
        // Attempt to gain access to the SD card
        if (xSemaphoreTake(sdCardSemaphore, fatSemaphore_longWait_ms) == pdPASS)
        {
            markSemaphore(FUNCTION_FILEMANAGER_UPLOAD1);

            managerTempFile->open(tempFileName, O_CREAT | O_APPEND | O_WRITE);

            xSemaphoreGive(sdCardSemaphore);
        }

        systemPrintln(logmessage);
    }

    if (len)
    {
        // Attempt to gain access to the SD card
        if (xSemaphoreTake(sdCardSemaphore, fatSemaphore_longWait_ms) == pdPASS)
        {
            markSemaphore(FUNCTION_FILEMANAGER_UPLOAD2);

            managerTempFile->write(data, len); // stream the incoming chunk to the opened file

            xSemaphoreGive(sdCardSemaphore);
        }
    }

    if (final)
    {
        logmessage = "Upload Complete: " + String(filename) + ",size: " + String(index + len);

        // Attempt to gain access to the SD card
        if (xSemaphoreTake(sdCardSemaphore, fatSemaphore_longWait_ms) == pdPASS)
        {
            markSemaphore(FUNCTION_FILEMANAGER_UPLOAD3);

            sdUpdateFileCreateTimestamp(managerTempFile); // Update the file create time & date

            managerTempFile->close();

            xSemaphoreGive(sdCardSemaphore);
        }

        systemPrintln(logmessage);
        request->redirect("/");
    }
}

void sendStringToWebsocket(char *stringToSend)
{
    websocket->textAll(stringToSend);
}

#endif // COMPILE_AP
