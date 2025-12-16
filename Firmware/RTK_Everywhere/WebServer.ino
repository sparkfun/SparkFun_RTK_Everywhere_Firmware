/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
WebServer.ino

  Start and stop the web-server, provide the form and handle browser input.
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#ifdef COMPILE_AP

//----------------------------------------
// Macros
//----------------------------------------

#define GET_PAGE(page, type, data)                                                                                     \
    webServer->on(page, HTTP_GET, []() {                                                                               \
        String length;                                                                                                 \
        if (settings.debugWebServer == true)                                                                           \
            systemPrintf("WebServer: Sending %s (%p, %d bytes)\r\n", page, (void *)data, sizeof(data));                \
        webServer->sendHeader("Content-Encoding", "gzip");                                                             \
        length = String(sizeof(data));                                                                                 \
        webServer->sendHeader("Content-Length", length.c_str());                                                       \
        webServer->send_P(200, type, (const char *)data, sizeof(data));                                                \
    });

//----------------------------------------
// Locals
//----------------------------------------

// Once connected to the access point for WiFi Config, the ESP32 sends current setting values in one long string to
// websocket After user clicks 'save', data is validated via main.js and a long string of values is returned.

static WebServer *webServer;

static TaskHandle_t updateWebServerTaskHandle;
static const uint8_t updateWebServerTaskPriority = 0; // 3 being the highest, and 0 being the lowest
static const int webServerTaskStackSize = 1024 * 4;   // Needs to be large enough to hold the file manager file list

// Inspired by:
// https://github.com/espressif/arduino-esp32/blob/master/libraries/WebServer/examples/MultiHomedServers/MultiHomedServers.ino
// https://esp32.com/viewtopic.php?t=37384
// https://github.com/espressif/esp-idf/blob/master/examples/protocols/http_server/ws_echo_server/main/ws_echo_server.c
// https://github.com/espressif/arduino-esp32/blob/master/libraries/ESP32/examples/Camera/CameraWebServer/CameraWebServer.ino
// https://esp32.com/viewtopic.php?t=24445

// These are useful:
// https://github.com/mo-thunderz/Esp32WifiPart2/blob/main/Arduino/ESP32WebserverWebsocket/ESP32WebserverWebsocket.ino
// https://www.youtube.com/watch?v=15X0WvGaVg8
// https://github.com/espressif/arduino-esp32/blob/master/libraries/WebServer/examples/WebServer/WebServer.ino

//----------------------------------------
// Handler for firmware file downloads
//----------------------------------------
static void handleFileManager()
{
    // This section does not tolerate semaphore transactions
    String logmessage =
        "handleFileManager: Client:" + webServer->client().remoteIP().toString() + " " + webServer->uri();

    if (webServer->hasArg("name") && webServer->hasArg("action"))
    {
        String fileName = webServer->arg("name");
        String fileAction = webServer->arg("action");

        logmessage = "Client:" + webServer->client().remoteIP().toString() + " " + webServer->uri() +
                     "?name=" + fileName + "&action=" + fileAction;

        char slashFileName[60];
        snprintf(slashFileName, sizeof(slashFileName), "/%s", fileName.c_str());

        bool fileExists = sd->exists(slashFileName);

        if (fileExists == false)
        {
            logmessage += " ERROR: file ";
            logmessage += slashFileName;
            logmessage += " does not exist";
            webServer->send(400, "text/plain", "ERROR: file does not exist");
        }
        else
        {
            logmessage += " file exists";

            if (fileAction == "download")
            {
                logmessage += " downloaded";

                // This would be SO much easier with webServer->streamFile
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
                    webServer->send(202, "text/plain", "ERROR: File already downloading");
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
                        webServer->setContentLength(dataAvailable);
                        webServer->sendHeader("Cache-Control", "no-cache");
                        webServer->sendHeader("Content-Disposition", "attachment; filename=" + String(fileName));
                        webServer->sendHeader("Access-Control-Allow-Origin", "*");
                        webServer->send(200, "application/octet-stream", "");
                        firstSend = false;
                    }

                    webServer->sendContent((const char *)buf, sending);

                    if (sending < maxLen) // Last send?
                    {
                        managerTempFile->close();

                        managerFileOpen = false;

                        webSocketsSendString("fmNext,1,"); // Tell browser to send next file if needed
                    }

                    dataAvailable -= sending;
                }

                delete[] buf;
            }
            else if (fileAction == "delete")
            {
                logmessage += " deleted";
                sd->remove(slashFileName);
                webServer->send(200, "text/plain", "Deleted File: " + fileName);
            }
            else
            {
                logmessage += " ERROR: invalid action param supplied";
                webServer->send(400, "text/plain", "ERROR: invalid action param supplied");
            }
        }
        systemPrintln(logmessage);
    }
    else
    {
        webServer->send(400, "text/plain", "ERROR: name and action params required");
    }
}

//----------------------------------------
// Handler for firmware file upload
//----------------------------------------
static void handleFirmwareFileUpload()
{
    String fileName = "";

    HTTPUpload &upload = webServer->upload();

    if (upload.status == UPLOAD_FILE_START)
    {
        // Check file name against valid firmware names
        const char *BIN_EXT = "bin";
        const char *BIN_HEADER = "/RTK_Everywhere";

        fileName = upload.filename;

        int fnameLen = fileName.length();
        char fname[fnameLen + 2] = {'/'}; // Filename must start with / or VERY bad things happen on SD_MMC
        fileName.toCharArray(&fname[1], fnameLen + 1);
        fname[fnameLen + 1] = '\0'; // Terminate array

        // Check 'bin' extension
        if (strcmp(BIN_EXT, &fname[strlen(fname) - strlen(BIN_EXT)]) == 0)
        {
            // Check for '/RTK_Everywhere' start of file name
            if (strncmp(fname, BIN_HEADER, strlen(BIN_HEADER)) == 0)
            {
                // Begin update process
                if (!Update.begin(UPDATE_SIZE_UNKNOWN))
                {
                    Update.printError(Serial);
                    webServer->send(400, "text/plain", "OTA could not begin");
                    return;
                }
            }
            else
            {
                systemPrintf("handleFirmwareFileUpload: Unknown: %s\r\n", fname);
                webServer->send(400, "text/html", "<b>Error:</b> Unknown file type");
                return;
            }
        }
        else
        {
            systemPrintf("handleFirmwareFileUpload: Unknown: %s\r\n", fname);
            webServer->send(400, "text/html", "<b>Error:</b> Unknown file type");
            return;
        }
    }

    // Write chunked data to the free sketch space
    else if (upload.status == UPLOAD_FILE_WRITE)
    {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
        {
            webServer->send(400, "text/plain", "OTA could not begin");
            return;
        }
        else
        {
            // See issue #811
            // The Update.write seems to upload the whole file in one go
            // This code never gets called...

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
                webSocketsSendString(statusMsg);
            }
        }
    }

    else if (upload.status == UPLOAD_FILE_END)
    {
        if (!Update.end(true))
        {
            Update.printError(Serial);
            webServer->send(400, "text/plain", "Could not end OTA");
            return;
        }
        else
        {
            webSocketsSendString("firmwareUploadComplete,1,");
            systemPrintln("Firmware update complete. Restarting");
            delay(500);
            ESP.restart();
        }
    }
}

//----------------------------------------
// Handles uploading of user files to SD
// https://github.com/espressif/arduino-esp32/blob/master/libraries/WebServer/examples/FSBrowser/FSBrowser.ino
//----------------------------------------
void handleUpload()
{
    HTTPUpload &upload = webServer->upload();

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
            webServer->send(202, "text/plain", "ERROR: File already uploading");
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
        webServer->sendHeader("Location", "/");
        webServer->send(302, "text/plain", "");
    }
}

//----------------------------------------
// Generate the Not Found page
//----------------------------------------
void notFound()
{
    if (settings.enableCaptivePortal == true && knownCaptiveUrl(webServer->uri()) == true)
    {
        if (settings.debugWebServer == true)
        {
            String logmessage =
                "Known captive URI: " + webServer->client().remoteIP().toString() + " " + webServer->uri();
            systemPrintln(logmessage);
        }
        webServer->sendHeader("Location", "/");
        webServer->send(302, "text/plain", "Redirect to Web Config");
        return;
    }

    String logmessage = "notFound: " + webServer->client().remoteIP().toString() + " " + webServer->uri();
    systemPrintln(logmessage);
    webServer->send(404, "text/plain", "Not found");
}

// These are the various files or endpoints that browsers will attempt to access to see if internet access is available
// If one is requested, redirect user to captive portal
String captiveUrls[] = {
    "/hotspot-detect.html", "/library/test/success.html", "/generate_204", "/ncsi.txt", "/check_network_status.txt",
    "/connecttest.txt"};

static const uint8_t captiveUrlCount = sizeof(captiveUrls) / sizeof(captiveUrls[0]);

// Check if given URI is a captive portal endpoint
bool knownCaptiveUrl(String uri)
{
    for (int i = 0; i < captiveUrlCount; i++)
    {
        if (uri == captiveUrls[i])
            return true;
    }
    return false;
}

//----------------------------------------
// Stop the web server
//----------------------------------------
void stopWebServer()
{
    if (task.updateWebServerTaskRunning)
        task.updateWebServerTaskStopRequest = true;

    // Wait for task to stop running
    do
        delay(10);
    while (task.updateWebServerTaskRunning);

    if (webServer != nullptr)
    {
        webServer->close();
        delete webServer;
        webServer = nullptr;
    }

    if (incomingSettings != nullptr)
    {
        rtkFree(incomingSettings, "Settings buffer (incomingSettings)");
        incomingSettings = nullptr;
    }
}

//----------------------------------------
//----------------------------------------
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

        webServer->handleClient();

        feedWdt();
        taskYIELD();
    }

    // Stop notification
    if (settings.printTaskStartStop)
        systemPrintln("Task updateWebServerTask stopped");
    task.updateWebServerTaskRunning = false;
    vTaskDelete(updateWebServerTaskHandle);
}

//----------------------------------------
// Create the web server and web sockets
//----------------------------------------
bool webServerAssignResources(int httpPort = 80)
{
    if (settings.debugWebServer)
        systemPrintln("Assigning web server resources");
    do
    {
        // Freed by webServerStop
        incomingSettings = (char *)rtkMalloc(AP_CONFIG_SETTING_SIZE, "Settings buffer (incomingSettings)");
        if (!incomingSettings)
        {
            systemPrintln("ERROR: Web server failed to allocate incomingSettings");
            break;
        }
        memset(incomingSettings, 0, AP_CONFIG_SETTING_SIZE);

        /* https://github.com/espressif/arduino-esp32/blob/master/libraries/DNSServer/examples/CaptivePortal/CaptivePortal.ino
         */

        // Note: MDNS should probably be begun by networkMulticastDNSUpdate, but that doesn't seem to be happening...
        //       Is the networkInterface aware that AP needs it? Let's start it manually...
        if (MDNS.begin(&settings.mdnsHostName[0]) == false)
        {
            systemPrintln("Error setting up MDNS responder!");
        }
        else
        {
            if (settings.debugNetworkLayer)
                systemPrintf("mDNS started as %s.local\r\n", settings.mdnsHostName);
        }

        webServer = new WebServer(httpPort);
        if (!webServer)
        {
            systemPrintln("ERROR: Web server failed to allocate webServer");
            break;
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

        webServer->onNotFound(notFound);

        GET_PAGE("/", "text/html", index_html);
        GET_PAGE("/favicon.ico", "text/plain", favicon_ico);
        GET_PAGE("/src/bootstrap.bundle.min.js", "text/javascript", bootstrap_bundle_min_js);
        GET_PAGE("/src/bootstrap.min.css", "text/css", bootstrap_min_css);
        GET_PAGE("/src/bootstrap.min.js", "text/javascript", bootstrap_min_js);
        GET_PAGE("/src/jquery-3.6.0.min.js", "text/javascript", jquery_js);
        GET_PAGE("/src/main.js", "text/javascript", main_js);
        if (productVariant == RTK_EVK)
        {
            GET_PAGE("/src/rtk-setup.png", "image/png", rtkSetup_png);
        }
        else
        {
            GET_PAGE("/src/rtk-setup.png", "image/png", rtkSetupWiFi_png);
        }

        // Battery icons
        GET_PAGE("/src/BatteryBlank.png", "image/png", batteryBlank_png);
        GET_PAGE("/src/Battery0.png", "image/png", battery0_png);
        GET_PAGE("/src/Battery1.png", "image/png", battery1_png);
        GET_PAGE("/src/Battery2.png", "image/png", battery2_png);
        GET_PAGE("/src/Battery3.png", "image/png", battery3_png);
        GET_PAGE("/src/Battery0_Charging.png", "image/png", battery0_Charging_png);
        GET_PAGE("/src/Battery1_Charging.png", "image/png", battery1_Charging_png);
        GET_PAGE("/src/Battery2_Charging.png", "image/png", battery2_Charging_png);
        GET_PAGE("/src/Battery3_Charging.png", "image/png", battery3_Charging_png);
        GET_PAGE("/src/style.css", "text/css", style_css);
        GET_PAGE("/src/fonts/icomoon.eot", "text/plain", icomoon_eot);
        GET_PAGE("/src/fonts/icomoon.svg", "text/plain", icomoon_svg);
        GET_PAGE("/src/fonts/icomoon.ttf", "text/plain", icomoon_ttf);
        GET_PAGE("/src/fonts/icomoon.woof", "text/plain", icomoon_woof);

        // Handler for the /uploadFile form POST
        webServer->on(
            "/uploadFile", HTTP_POST, []() { webServer->send(200, "text/plain", ""); },
            handleUpload); // Run handleUpload function when file manager file is uploaded

        // Handler for the /uploadFirmware form POST
        webServer->on(
            "/uploadFirmware", HTTP_POST, []() { webServer->send(200, "text/plain", ""); }, handleFirmwareFileUpload);

        // Handler for file manager
        webServer->on("/listfiles", HTTP_GET, []() {
            String logmessage = "Client:" + webServer->client().remoteIP().toString() + " " + webServer->uri();
            if (settings.debugWebServer == true)
                systemPrintln(logmessage);
            String files;
            webSocketsGetFileList(files);
            webServer->send(200, "text/plain", files);
        });

        // Handler for supported messages list
        webServer->on("/listMessages", HTTP_GET, []() {
            String logmessage = "Client:" + webServer->client().remoteIP().toString() + " " + webServer->uri();
            if (settings.debugWebServer == true)
                systemPrintln(logmessage);
            String messages;
            webSocketsCreateMessageList(messages);
            if (settings.debugWebServer == true)
                systemPrintln(messages);
            webServer->send(200, "text/plain", messages);
        });

        // Handler for supported RTCM/Base messages list
        webServer->on("/listMessagesBase", HTTP_GET, []() {
            String logmessage = "Client:" + webServer->client().remoteIP().toString() + " " + webServer->uri();
            if (settings.debugWebServer == true)
                systemPrintln(logmessage);
            String messageList;
            webSocketsCreateMessageListBase(messageList);
            if (settings.debugWebServer == true)
                systemPrintln(messageList);
            webServer->send(200, "text/plain", messageList);
        });

        // Handler for file manager
        webServer->on("/file", HTTP_GET, handleFileManager);

        // Start the web server
        webServer->begin();

        if (settings.mdnsEnable == true)
            MDNS.addService("http", "tcp", settings.httpPort); // Add service to MDNS

        // Starts task for updating webServer with handleClient
        if (task.updateWebServerTaskRunning == false)
            xTaskCreate(updateWebServerTask,
                        "UpdateWebServer",      // Just for humans
                        webServerTaskStackSize, // Stack Size - needs to be large enough to hold the file manager list
                        nullptr,                // Task input parameter
                        updateWebServerTaskPriority,
                        &updateWebServerTaskHandle); // Task handle

        if (settings.debugWebServer == true)
            systemPrintln("Web Server: Started");
        reportHeapNow(false);

        // Start the web socket server on port 81 using <esp_http_server.h>
        if (webSocketsStart() == false)
        {
            if (settings.debugWebServer == true)
                systemPrintln("Web Sockets failed to start");
            break;
        }

        if (settings.debugWebServer == true)
        {
            systemPrintln("Web Socket Server Started");
            reportHeapNow(true);
        }

        online.webServer = true;
        return true;
    } while (0);

    // Release the resources
    if (settings.debugWebServer == true)
        reportHeapNow(true);
    webServerReleaseResources();
    return false;
}

//----------------------------------------
//----------------------------------------
void webServerReleaseResources()
{
    if (settings.debugWebServer)
        systemPrintln("Releasing web server resources");
    if (task.updateWebServerTaskRunning)
        task.updateWebServerTaskStopRequest = true;

    // Wait for task to stop running
    do
        delay(10);
    while (task.updateWebServerTaskRunning);

    online.webServer = false;

    webSocketsStop(); // Release socket resources

    if (webServer != nullptr)
    {
        webServer->close();
        delete webServer;
        webServer = nullptr;
    }

    if (incomingSettings != nullptr)
    {
        rtkFree(incomingSettings, "Settings buffer (incomingSettings)");
        incomingSettings = nullptr;
    }
}

#endif // COMPILE_AP
