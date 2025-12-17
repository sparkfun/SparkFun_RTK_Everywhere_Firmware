/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
WebSockets.ino

  Web socket support
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#ifdef COMPILE_AP

//----------------------------------------
// Constants
//----------------------------------------

static const int webSocketsStackSize = 1024 * 20;

const char * const image_png = "image/png";
const char * const text_css = "text/css";
const char * const text_html = "text/html";
const char * const text_javascript = "text/javascript";
const char * const text_plain = "text/plain";

#define UPLOAD_PATH         "/uploadFile"

// State machine to allow web server access to network layer
enum WebServerState
{
    WEBSERVER_STATE_OFF = 0,
    WEBSERVER_STATE_WAIT_FOR_NETWORK,
    WEBSERVER_STATE_NETWORK_CONNECTED,
    WEBSERVER_STATE_RUNNING,

    // Add new states here
    WEBSERVER_STATE_MAX
};

static const char *const webServerStateNames[] = {
    "WEBSERVER_STATE_OFF",
    "WEBSERVER_STATE_WAIT_FOR_NETWORK",
    "WEBSERVER_STATE_NETWORK_CONNECTED",
    "WEBSERVER_STATE_RUNNING",
};
static const int webServerStateEntries = sizeof(webServerStateNames) / sizeof(webServerStateNames[0]);

// These are the various files or endpoints that browsers will attempt to
// access to see if internet access is available.  If one is requested,
// redirect user to captive portal (main page "/").
const char * webSocketsCaptiveUrls[] =
{
    "/hotspot-detect.html",
    "/library/test/success.html",
    "/generate_204",
    "/ncsi.txt",
    "/check_network_status.txt",
    "/connecttest.txt"
};
const uint8_t webSocketsCaptiveUrlCount = sizeof(webSocketsCaptiveUrls) / sizeof(webSocketsCaptiveUrls[0]);

//----------------------------------------
// New types
//----------------------------------------

typedef struct _GET_PAGE_HANDLER
{
    httpd_uri_t _page;
    const char * const * _type;
    void * _data;
    size_t _length;
} GET_PAGE_HANDLER;

typedef struct _WEB_SOCKETS_CLIENT
{
    struct _WEB_SOCKETS_CLIENT * _flink;
    struct _WEB_SOCKETS_CLIENT * _blink;
    httpd_req_t * _request;
    int _socketFD;
} WEB_SOCKETS_CLIENT;

//----------------------------------------
// Macros
//----------------------------------------

#define PAGE_HANDLER(index, page, httpMethod, type, routine)    \
    {                                               \
        {                                           \
            .uri = page,                            \
            .method = httpMethod,                   \
            .handler = routine,                     \
            .user_ctx = (void *)index,              \
        },                                          \
        &type,                                      \
        nullptr,                                    \
        0,                                          \
    }

#define WEB_PAGE(index, page, type, data)           \
    {                                               \
        {                                           \
            .uri = page,                            \
            .method = HTTP_GET,                     \
            .handler = webSocketsHandlerGetPage,    \
            .user_ctx = (void *)index,              \
        },                                          \
        &type,                                      \
        (void *)data,                               \
        sizeof(data),                               \
    }

//----------------------------------------
// Locals
//----------------------------------------

static WEB_SOCKETS_CLIENT * webSocketsClientListHead;
static WEB_SOCKETS_CLIENT * webSocketsClientListTail;
static httpd_handle_t webSocketsHandle;
static SemaphoreHandle_t webSocketsMutex;
static uint8_t webServerState;

//----------------------------------------
// Forward routines
//----------------------------------------

esp_err_t webSocketsHandlerFileList(httpd_req_t *req);
esp_err_t webSocketsHandlerFileUpload(httpd_req_t *req);
esp_err_t webSocketsHandlerGetPage(httpd_req_t *req);
esp_err_t webSocketsHandlerListBaseMessages(httpd_req_t *req);
esp_err_t webSocketsHandlerListMessages(httpd_req_t *req);

//----------------------------------------
// Web page descriptions
//----------------------------------------

const GET_PAGE_HANDLER webSocketsPages[] =
{
    // Platform specific pages
    WEB_PAGE( 0, "/src/rtk-setup.png", image_png, rtkSetup_png),    // EVK
    WEB_PAGE( 1, "/src/rtk-setup.png", image_png, rtkSetupWiFi_png),    // WiFi support

    // Add special pages above this line

    // Page icon
    WEB_PAGE( 2, "/favicon.ico", text_plain, favicon_ico),

    // Fonts
    WEB_PAGE( 3, "/src/fonts/icomoon.eot", text_plain, icomoon_eot),
    WEB_PAGE( 4, "/src/fonts/icomoon.svg", text_plain, icomoon_svg),
    WEB_PAGE( 5, "/src/fonts/icomoon.ttf", text_plain, icomoon_ttf),
    WEB_PAGE( 6, "/src/fonts/icomoon.woof", text_plain, icomoon_woof),

    // Battery icons
    WEB_PAGE( 7, "/src/BatteryBlank.png", image_png, batteryBlank_png),
    WEB_PAGE( 8, "/src/Battery0.png", image_png, battery0_png),
    WEB_PAGE( 9, "/src/Battery1.png", image_png, battery1_png),
    WEB_PAGE(10, "/src/Battery2.png", image_png, battery2_png),
    WEB_PAGE(11, "/src/Battery3.png", image_png, battery3_png),
    WEB_PAGE(12, "/src/Battery0_Charging.png", image_png, battery0_Charging_png),
    WEB_PAGE(13, "/src/Battery1_Charging.png", image_png, battery1_Charging_png),
    WEB_PAGE(14, "/src/Battery2_Charging.png", image_png, battery2_Charging_png),
    WEB_PAGE(15, "/src/Battery3_Charging.png", image_png, battery3_Charging_png),

    // Bootstrap
    WEB_PAGE(16, "/src/bootstrap.bundle.min.js", text_javascript, bootstrap_bundle_min_js),
    WEB_PAGE(17, "/src/bootstrap.min.js", text_javascript, bootstrap_min_js),

    // Java script
    WEB_PAGE(18, "/src/jquery-3.6.0.min.js", text_javascript, jquery_js),
    WEB_PAGE(19, "/src/main.js", text_javascript, main_js),

    // Style sheets
    WEB_PAGE(20, "/src/bootstrap.min.css", text_css, bootstrap_min_css),
    WEB_PAGE(21, "/src/style.css", text_css, style_css),

    // File pages
    PAGE_HANDLER(22, "/listfiles", HTTP_GET, text_plain, webSocketsHandlerFileList),
    PAGE_HANDLER(23, "/file", HTTP_GET, text_plain, webSocketsHandlerFileManager),

    // Message handlers
    PAGE_HANDLER(24, "/listMessages", HTTP_GET, text_plain, webSocketsHandlerListMessages),
    PAGE_HANDLER(25, "/listMessagesBase", HTTP_GET, text_plain, webSocketsHandlerListBaseMessages),
    PAGE_HANDLER(26, UPLOAD_PATH, HTTP_POST, text_plain, webSocketsHandlerFileUpload),

    // Add pages above this line
    WEB_PAGE(27, "/", text_html, index_html),
};

#define WEB_SOCKETS_SPECIAL_PAGES   2
#define WEB_SOCKETS_TOTAL_PAGES     (sizeof(webSocketsPages) / sizeof(GET_PAGE_HANDLER))

//----------------------------------------
// Check if given URI is a captive portal endpoint
//----------------------------------------
bool webSocketsCheckForKnownCaptiveUrl(const char * uri)
{
    for (int i = 0; i < webSocketsCaptiveUrlCount; i++)
    {
        if (strcmp(uri, webSocketsCaptiveUrls[i]) == 0)
            return true;
    }
    return false;
}

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
// When called, responds with the messages supported on this platform
// Message name and current rate are formatted in CSV, formatted to html by JS
//----------------------------------------
void webSocketsCreateMessageList(String &returnText)
{
    returnText = "";
    gnss->createMessageList(returnText);
    if (settings.debugWebServer == true)
        systemPrintf("returnText (%d bytes): %s\r\n", returnText.length(), returnText.c_str());
}

//----------------------------------------
// When called, responds with the RTCM/Base messages supported on this platform
// Message name and current rate are formatted in CSV, formatted to html by JS
//----------------------------------------
void webSocketsCreateMessageListBase(String &returnText)
{
    returnText = "";
    gnss->createMessageListBase(returnText);
    if (settings.debugWebServer == true)
        systemPrintf("returnText (%d bytes): %s\r\n", returnText.length(), returnText.c_str());
}

//----------------------------------------
// Display the request
//----------------------------------------
void webSocketsDisplayRequest(httpd_req_t *req)
{
    // Display the request and response
    if (settings.debugWebServer == true)
    {
        char ipAddress[80];

        webSocketsGetClientIpAddressAndPort(req, ipAddress, sizeof(ipAddress));
        systemPrintf("WebServer Client: %s%s\r\n", ipAddress, req->uri);
    }
}

//----------------------------------------
// Display the request
//----------------------------------------
void webSocketsDisplayRequestAndData(httpd_req_t *req,
                                     const void * data,
                                     size_t bytes)
{
    // Display the request and response
    if (settings.debugWebServer == true)
    {
        char ipAddress[80];

        webSocketsGetClientIpAddressAndPort(req, ipAddress, sizeof(ipAddress));
        systemPrintf("WebServer Client: %s%s (%p, %d bytes)\r\n",
                     ipAddress, req->uri, data, bytes);
    }
}

//----------------------------------------
// Delete the specified file
//----------------------------------------
void webSocketsFileDelete(httpd_req_t * req, const char * fileName)
{
    int httpResponseCode;
    String * response;
    String responseFailed;
    String responseSuccessful;
    const char * statusMessage;

    // Build the responses
    responseFailed = "File ";
    responseFailed += &fileName[1];
    responseFailed += " does not exist";

    responseSuccessful = "Deleted file ";
    responseSuccessful += &fileName[1];

    // Attempt to gain access to the SD card
    if (settings.debugWebServer == true)
        systemPrintf("WebSockets waiting for the sdCardSemaphore semaphore\r\n");
    if (xSemaphoreTake(sdCardSemaphore, fatSemaphore_longWait_ms) != pdPASS)
    {
        statusMessage = HTTPD_500;
        responseFailed = "Failed to obtain access to the SD card!";
        response = &responseFailed;
    }
    else
    {
        // Delete the file if it exists
        if (sd->exists(fileName))
        {
            sd->remove(fileName);
            statusMessage = HTTPD_200;
            response = &responseSuccessful;
        }

        // Send the error when the file does not exist
        else
        {
            statusMessage = HTTPD_400;
            response = &responseFailed;
        }

        // Release the semaphore
        xSemaphoreGive(sdCardSemaphore);
        if (settings.debugWebServer == true)
            systemPrintf("WebSockets released the sdCardSemaphore semaphore\r\n");
    }

    // Send the response
    if (response == &responseFailed)
        responseFailed = "ERROR: " + responseFailed;
    if (settings.debugWebServer == true)
        systemPrintf("WebSockets: %s\r\n", response->c_str());
    httpd_resp_set_status(req, statusMessage);
    httpd_resp_send(req, response->c_str(), response->length());
}

//----------------------------------------
// Download the specified file
//----------------------------------------
void webSocketsFileDownload(httpd_req_t * req, const char * fileName)
{
    uint8_t * buffer;
    const size_t bufferLength = 1024; // 32768;
    int bytes;
    SdFile file;
    int httpResponseCode;
    String * response;
    String responseFailed;
    String responseSuccessful;
    esp_err_t status;
    const char * statusMessage;

    // Build the responses
    responseFailed = "File ";
    responseFailed += &fileName[1];
    responseFailed += " does not exist";

    responseSuccessful = "Downloaded file ";
    responseSuccessful += &fileName[1];

    // Attempt to gain access to the SD card
    if (settings.debugWebServer == true)
        systemPrintf("WebSockets waiting for the sdCardSemaphore semaphore\r\n");
    if (xSemaphoreTake(sdCardSemaphore, fatSemaphore_longWait_ms) != pdPASS)
    {
        statusMessage = HTTPD_500;
        responseFailed = "Failed to obtain access to the SD card!";
        response = &responseFailed;
    }
    else
    {
        // Allocate the buffer
        buffer = (uint8_t *)rtkMalloc(bufferLength, "WebSockets file download buffer");
        if (buffer)
        {
            // Download the file if it exists
            if (file.open(sd->vol(), fileName, 0))
            {
                while (1)
                {
                    bytes = file.read(buffer, bufferLength);
                    if (bytes == 0)
                    {
                        statusMessage = HTTPD_200;
                        response = &responseSuccessful;
                        break;
                    }
                    if (bytes < 0)
                    {
                        statusMessage = HTTPD_500;
                        responseFailed = "Failed during download of ";
                        responseFailed += &fileName[1];
                        response = &responseFailed;
                        break;
                    }

                    // Send the data
                    status = httpd_resp_send_chunk(req, (char *)buffer, bytes);
                    if (status != ESP_OK)
                    {
                        statusMessage = HTTPD_500;
                        responseFailed = "Failed sending download data";
                        response = &responseFailed;
                        break;
                    }
                }

                // Done with this file
                file.close();
            }

            // Send the error when the file does not exist
            else
            {
                statusMessage = HTTPD_400;
                response = &responseFailed;
            }

            // Free the download buffer
            rtkFree(buffer, "WebSockets file download buffer");
        }
        else
        {
            statusMessage = HTTPD_500;
            responseFailed = "Failed to allocate download buffer!";
            response = &responseFailed;
        }

        // Release the semaphore
        xSemaphoreGive(sdCardSemaphore);
        if (settings.debugWebServer == true)
            systemPrintf("WebSockets released the sdCardSemaphore semaphore\r\n");
    }

    // Send the response
    if (response == &responseFailed)
        responseFailed = "ERROR: " + responseFailed;
    if (settings.debugWebServer == true)
        systemPrintf("WebSockets: %s\r\n", response->c_str());
    httpd_resp_set_status(req, statusMessage);
    httpd_resp_send(req, response->c_str(), response->length());
}

//----------------------------------------
// Get the client IP address
//----------------------------------------
void webSocketsGetClientIpAddressAndPort(httpd_req_t *req,
                                         char * ipAddress,
                                         size_t ipAddressBytes)
{
    socklen_t addrBytes;
    const uint8_t ip4Address[12] =
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff
    };
    struct sockaddr_in6 ip6Address;
    bool isIp6Address;
    size_t requiredStringLength;
    int socketFD;
    char temp[32 + 7 + 1];

    // Validate the parameters
    if (ipAddress == nullptr)
    {
        systemPrintf("ERROR: ipAddress must be specified in WebSockets\r\n");
        return;
    }
    if (ipAddressBytes == 0)
    {
        systemPrintf("ERROR: ipAddressBytes must be > 0 in WebSockets\r\n");
        return;
    }
    ipAddress[0] = 0;

    // Get the socket's file descriptor
    socketFD = httpd_req_to_sockfd(req);

    // Get the IP6 address of the client from the httpd server
    addrBytes = sizeof(ip6Address);
    if (getpeername(socketFD, (struct sockaddr *)&ip6Address, &addrBytes) < 0)
    {
        if (settings.debugWebServer == true)
            systemPrintf("ERROR: WebSockets failed to get client IP address\r\n");
        return;
    }

    // Determine the type of IP address
    isIp6Address = (memcmp(&ip6Address.sin6_addr, &ip4Address, sizeof(ip4Address)) != 0);

    // Convert the IPv6 address into a string
    if (isIp6Address)
        inet_ntop(AF_INET6, &ip6Address.sin6_addr, temp, sizeof(temp));
    else
        inet_ntop(AF_INET, &ip6Address.sin6_addr.un.u8_addr[12], temp, sizeof(temp));

    // Verify the length of the output buffer
    requiredStringLength = (isIp6Address ? 1 : 0)   // Left bracket (IP6 only)
                         + strlen(temp)             // IP address
                         + (isIp6Address ? 1 : 0)   // Right bracket (IP6 only)
                         + 1    // Colon
                         + 5    // Port number
                         + 1;   // Zero termination
    if (ipAddressBytes < requiredStringLength)
    {
        if (settings.debugWebServer == true)
            systemPrintf("ERROR: WebSockets failed to get client IP address\r\n");
        return;
    }

    // Build the IP address string
    if (isIp6Address)
        sprintf(ipAddress, "[%s]:%d", temp, ip6Address.sin6_port);
    else
        sprintf(ipAddress, "%s:%d", temp, ip6Address.sin6_port);
}

//----------------------------------------
// When called, responds with the root folder list of files on SD card
// Name and size are formatted in CSV, formatted to html by JS
//----------------------------------------
void webSocketsGetFileList(String &returnText)
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

    if (settings.debugWebServer == true)
        systemPrintf("returnText (%d bytes): %s\r\n", returnText.length(), returnText.c_str());
}

//----------------------------------------
// Get the webconfig state name
//----------------------------------------
const char *webServerGetStateName(uint8_t state, char *string)
{
    if (state < WEBSERVER_STATE_MAX)
        return webServerStateNames[state];
    sprintf(string, "Web Server: Unknown state (%d)", state);
    return string;
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
    }

    rtkFree(buf, "Payload buffer (buf)");
    return ret;
}

//----------------------------------------
// Handler to list the microSD card files
//----------------------------------------
esp_err_t webSocketsHandlerFileList(httpd_req_t *req)
{
    size_t bytes;
    const char * data;
    String fileList;
    char ipAddress[80];

    // Get the file list
    webSocketsGetFileList(fileList);
    data = fileList.c_str();
    bytes = fileList.length();

    // Display the request and response
    webSocketsDisplayRequestAndData(req, data, bytes);

    // Send the response
    httpd_resp_set_type(req, text_plain);
    return httpd_resp_send(req, data, bytes);
}

//----------------------------------------
// Handler for file manager
//----------------------------------------
esp_err_t webSocketsHandlerFileManager(httpd_req_t *req)
{
    char * action;
    const char * actionParameter = "action=";
    size_t bytes;
    const char * errorMessage;
    char * fileName;
    const char * fileNameParameter = "name=";
    char * parameter;
    char * parameters;
    char * parameterBuffer;
    size_t parameterBufferLength;
    esp_err_t status;
    do
    {
        errorMessage = nullptr;

        // Display the request
        webSocketsDisplayRequest(req);

        // Allocate a buffer for the name and action
        parameterBufferLength = strlen(req->uri);
        parameterBuffer = (char *)rtkMalloc(parameterBufferLength + 1,
                                            "WebSockets file manager parameters buffer");
        if (parameterBuffer == nullptr)
        {
            errorMessage = "ERROR: WebSockets failed to allocate file manager parameters buffer!";
            break;
        }

        // Locate the parameters, then follow question mark after the webpage name
        parameters = strcpy(parameterBuffer, req->uri);
        parameters = strstr(parameters, "?");
        if (parameters == nullptr)
        {
            errorMessage = "ERROR: Parameters not passed to WebSockets page!";
            break;
        }
        parameters += 1;
        parameterBufferLength = parameterBufferLength - (parameters - parameterBuffer);

        // Locate the file name parameter
        fileName = strstr(parameters, fileNameParameter);
        if (fileName == nullptr)
        {
            errorMessage = "ERROR: name parameter not passed to WebSockets page!";
            break;
        }
        fileName += strlen(fileNameParameter);

        // Locate the action parameter
        action = strstr(parameters, actionParameter);
        if (action == nullptr)
        {
            errorMessage = "ERROR: action parameter not passed to WebSockets page!";
            break;
        }
        action += strlen(actionParameter);

        // Zero terminate the file name
        parameter = fileName;
        while ((*parameter != 0) && (*parameter != '&')) parameter++;
        *parameter = 0;

        // Zero terminate the action
        parameter = action;
        while ((*parameter != 0) && (*parameter != '&')) parameter++;
        *parameter = 0;

        // Add a slash to the beginning of the file name
        *--fileName = '/';

        // Display the parameters
        if (settings.debugWebServer == true)
        {
            systemPrintf("fileName: %s\r\n", fileName);
            systemPrintf("action: %s\r\n", action);
        }

        // Perform the action
        if (strcmp(action, "delete") == 0)
            webSocketsFileDelete(req, fileName);
        else if (strcmp(action, "download") == 0)
            webSocketsFileDownload(req, fileName);
        else
            errorMessage = "ERROR: Unknown WebSockets action!";
    } while (0);

    // Done with the buffers
    if (parameterBuffer)
        rtkFree(parameterBuffer,
                "WebSockets file manager parameters buffer");

    // Output the error
    if (errorMessage)
    {
        // Respond with 500 Internal Server Error
        systemPrintln(errorMessage);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, errorMessage);
    }
    return ESP_OK;
}

//----------------------------------------
// Handler to upload a file from the PC to the microSD card
//----------------------------------------
esp_err_t webSocketsHandlerFileUpload(httpd_req_t *req)
{
    uint8_t * buffer;
    const size_t bufferLength = 32768;
    size_t bytes;
    size_t bytesRead;
    size_t bytesWritten;
    uint8_t * data;
    size_t dataBytes;
    const char * errorMessage;
    SdFile file;
    size_t fileLength;
    char * fileName;
    const char * fileNameParameter = "filename=\"";
    char * header;
    size_t remainingLength;
    bool semaphoreAcquired;
    char * separator;
    size_t separatorLength;
    esp_err_t status;
    char * temp;

    do
    {
        buffer = nullptr;
        errorMessage = nullptr;
        header = nullptr;
        semaphoreAcquired = false;
        separator = nullptr;
        status = ESP_FAIL;

        // Display the request and response
        webSocketsDisplayRequest(req);

        // Get the separator
        separator = webSocketsReadSeparator(req);
        if (separator == nullptr)
            break;
        separatorLength = strlen(separator);

        // Display the separator
        if (settings.debugWebServer == true)
        {
            systemPrintln("Seperator");
            dumpBuffer((uint8_t *)separator, strlen(separator) + 2);
        }

        // Get the header
        header = webSocketsReadHeader(req);
        if (header == nullptr)
            break;

        // Display the separator
        if (settings.debugWebServer == true)
        {
            systemPrintln("Header");
            dumpBuffer((uint8_t *)header, strlen(header));
        }

        // Determine the file length
        fileLength = req->content_len
                   - separatorLength
                   - 2                      // CR/LF
                   - strlen(header);

        // Estimate the remaining content data
        remainingLength = 2                 // CR/LF
                        + separatorLength
                        + 2;                // Dash dash
        if (fileLength >= (remainingLength + 2))
            remainingLength += 2;
        if (fileLength < remainingLength)
        {
            errorMessage = "ERROR: WebSockets detected a bad file length!";
            break;
        }
        fileLength -= remainingLength;

        // Display the file length
        if (settings.debugWebServer == true)
            systemPrintf("fileLength: %d (0x%08x) bytes\r\n", fileLength, fileLength);

        // Get the buffer for the file download
        buffer = (uint8_t *)rtkMalloc(bufferLength, "WebSockets file data buffer");
        if (buffer == nullptr)
        {
            errorMessage = "ERROR: WebSockets failed to allocate file data buffer!";
            break;
        }

        // Locate the file name parameter
        fileName = strstr(header, fileNameParameter);
        if (fileName == nullptr)
        {
            errorMessage = "ERROR: WebSockets failed to get filename parameter!";
            break;
        }
        fileName += strlen(fileNameParameter);

        // Get the file name
        temp = fileName;
        while (*temp && (*temp != '\r') && (*temp != '\"'))
            temp += 1;

        // Zero terminate the file name
        *temp = 0;

        // Display the file name
        if (settings.debugWebServer == true)
            systemPrintf("fileName: %s\r\n", fileName);

        // Add the slash to the file name
        fileName -= 1;
        *fileName = '/';

        // Attempt to gain access to the SD card
        if (xSemaphoreTake(sdCardSemaphore, fatSemaphore_longWait_ms) != pdPASS)
        {
            errorMessage = "ERROR: WebSockets failed to obtain access to the SD card!";
            break;
        }
        semaphoreAcquired = true;

        // Attempt to open the file
        if (file.open(fileName, O_WRONLY | O_CREAT | O_TRUNC) == false)
        {
            errorMessage = "ERROR: WebSockets failed to create the file!";
            break;
        }

        // Upload the file
        bytesWritten = 0;
        dataBytes = fileLength;
        while (dataBytes > 0)
        {
            // Determine the transfer length
            bytes = dataBytes;
            if (bytes > bufferLength)
                bytes = bufferLength;

            // Get a portion of the file
            bytesRead = httpd_req_recv(req, (char *)buffer, bytes);

            // Check for end of file
            if (bytesRead == 0)
                break;

            // Write the data to the file
            bytesWritten = file.write(buffer, bytesRead);
            if (bytesWritten != bytesRead)
            {
                errorMessage = "ERROR: WebServer failed to write to to the SD card!";
                break;
            }

            // Account for the data received
            dataBytes -= bytesRead;
        }

        // Handle the errors
        if (dataBytes != 0)
            break;

        // Get remaining portion of the page content
        bytesRead = httpd_req_recv(req, (char *)buffer, remainingLength);
        if (bytesRead != remainingLength)
        {
            errorMessage = "ERROR: WebSockets failed to read the remaining content!";
            break;
        }

        // Locate the separator at the end of the file
        data = (uint8_t *)strstr((char *)buffer, separator);
        if (data == nullptr)
        {
            errorMessage = "ERROR: WebServer failed to detect end of file!";
            break;
        }

        // Backup two bytes to remove the required CR/LF before the separator
        data -= 2;

        // Write the data to the file
        bytes = data - buffer;
        if (bytes)
        {
            // Write the data to the file
            bytesWritten = file.write(buffer, bytes);
            if (bytesWritten != bytes)
            {
                errorMessage = "ERROR: WebServer failed to finish writing the file!";
                break;
            }
        }

        // Display the file length
        fileLength += bytes;
        if (settings.debugWebServer == true)
            systemPrintf("fileLength: %d (0x%08x) bytes\r\n", fileLength, fileLength);

        // Success
        httpd_resp_sendstr(req, "File uploaded successfully");
        status = ESP_OK;
    } while (0);

    // Close the file
    if (file.isOpen())
    {
        // Set the create time and date
        sdUpdateFileCreateTimestamp(&file);

        // Close the file
        file.close();

        // Delete the file on error
        if (errorMessage)
            file.remove();
    }

    // Release the semaphore
    if (semaphoreAcquired)
        xSemaphoreGive(sdCardSemaphore);

    // Display the error message
    if (errorMessage)
    {
        // Respond with 500 Internal Server Error
        systemPrintln(errorMessage);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, errorMessage);
    }

    // Done with the buffers
    if (buffer)
        rtkFree(buffer, "WebSockets file data buffer");
    if (header)
        rtkFree(header, "WebSockets header");
    if (separator)
        rtkFree(separator, "WebSockets separator");
    return status;
}

//----------------------------------------
// Handler for GET_PAGE_HANDLER structures
//----------------------------------------
esp_err_t webSocketsHandlerGetPage(httpd_req_t *req)
{
    uint32_t index;
    const GET_PAGE_HANDLER * webpage;

    // Locate the page description
    index = (intptr_t)req->user_ctx;
    webpage = &((const GET_PAGE_HANDLER *)&webSocketsPages)[index];

    // Display the request and response
    webSocketsDisplayRequestAndData(req, webpage->_data, webpage->_length);

    // Send the response
    httpd_resp_set_type(req, (const char *)*webpage->_type);
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    return httpd_resp_send(req, (const char *)webpage->_data, webpage->_length);
}

//----------------------------------------
// Display the HTTPD configuration
//----------------------------------------
void webSocketsHttpdDisplayConfig(struct httpd_config *config)
{
    /*
    httpd_config object:
            5: task_priority
        20480: stack_size
    2147483647: core_id
            81: server_port
        32768: ctrl_port
            7: max_open_sockets
            8: max_uri_handlers
            8: max_resp_headers
            5: backlog_conn
        false: lru_purge_enable
            5: recv_wait_timeout
            5: send_wait_timeout
    0x0: global_user_ctx
    0x0: global_user_ctx_free_fn
    0x0: global_transport_ctx
    0x0: global_transport_ctx_free_fn
        false: enable_so_linger
            0: linger_timeout
        false: keep_alive_enable
            0: keep_alive_idle
            0: keep_alive_interval
            0: keep_alive_count
    0x0: open_fn
    0x0: close_fn
    0x0: uri_match_fn
    */
    systemPrintf("httpd_config object:\r\n");
    systemPrintf("%10d: task_priority\r\n", config->task_priority);
    systemPrintf("%10d: stack_size\r\n", config->stack_size);
    systemPrintf("%10d: core_id\r\n", config->core_id);
    systemPrintf("%10d: server_port\r\n", config->server_port);
    systemPrintf("%10d: ctrl_port\r\n", config->ctrl_port);
    systemPrintf("%10d: max_open_sockets\r\n", config->max_open_sockets);
    systemPrintf("%10d: max_uri_handlers\r\n", config->max_uri_handlers);
    systemPrintf("%10d: max_resp_headers\r\n", config->max_resp_headers);
    systemPrintf("%10d: backlog_conn\r\n", config->backlog_conn);
    systemPrintf("%10s: lru_purge_enable\r\n", config->lru_purge_enable ? "true" : "false");
    systemPrintf("%10d: recv_wait_timeout\r\n", config->recv_wait_timeout);

    systemPrintf("%10d: send_wait_timeout\r\n", config->send_wait_timeout);
    systemPrintf("%p: global_user_ctx\r\n", config->global_user_ctx);
    systemPrintf("%p: global_user_ctx_free_fn\r\n", config->global_user_ctx_free_fn);
    systemPrintf("%p: global_transport_ctx\r\n", config->global_transport_ctx);
    systemPrintf("%p: global_transport_ctx_free_fn\r\n", (void *)config->global_transport_ctx_free_fn);
    systemPrintf("%10s: enable_so_linger\r\n", config->enable_so_linger ? "true" : "false");
    systemPrintf("%10d: linger_timeout\r\n", config->linger_timeout);
    systemPrintf("%10s: keep_alive_enable\r\n", config->keep_alive_enable ? "true" : "false");
    systemPrintf("%10d: keep_alive_idle\r\n", config->keep_alive_idle);
    systemPrintf("%10d: keep_alive_interval\r\n", config->keep_alive_interval);
    systemPrintf("%10d: keep_alive_count\r\n", config->keep_alive_count);
    systemPrintf("%p: open_fn\r\n", (void *)config->open_fn);
    systemPrintf("%p: close_fn\r\n", (void *)config->close_fn);
    systemPrintf("%p: uri_match_fn\r\n", (void *)config->uri_match_fn);
}

//----------------------------------------
// Handler for supported RTCM/Base messages list
//----------------------------------------
esp_err_t webSocketsHandlerListBaseMessages(httpd_req_t *req)
{
    size_t bytes;
    const char * data;
    char ipAddress[80];
    String messageList;

    // Get the messages list
    webSocketsCreateMessageListBase(messageList);
    data = messageList.c_str();
    bytes = messageList.length();

    // Display the request and response
    webSocketsDisplayRequestAndData(req, data, bytes);

    // Send the response
    httpd_resp_set_type(req, text_plain);
    return httpd_resp_send(req, data, bytes);
}

//----------------------------------------
// Handler for supported messages list
//----------------------------------------
esp_err_t webSocketsHandlerListMessages(httpd_req_t *req)
{
    size_t bytes;
    const char * data;
    char ipAddress[80];
    String messageList;

    // Get the messages list
    webSocketsCreateMessageList(messageList);
    data = messageList.c_str();
    bytes = messageList.length();

    // Display the request and response
    webSocketsDisplayRequestAndData(req, data, bytes);

    // Send the response
    httpd_resp_set_type(req, text_plain);
    return httpd_resp_send(req, data, bytes);
}

//----------------------------------------
// Generate the Not Found page
//----------------------------------------
esp_err_t webSocketsHandlerPageNotFound(httpd_req_t *req, httpd_err_code_t err)
{
    char ipAddress[80];
    String logMessage;

    // Handle the captive portal pages
    if (settings.enableCaptivePortal
        && webSocketsCheckForKnownCaptiveUrl(req->uri))
    {
        if (settings.debugWebServer == true)
        {
            char ipAddress[80];

            webSocketsGetClientIpAddressAndPort(req, ipAddress, sizeof(ipAddress));
            systemPrintf("WebSockets: Captive page %s referenced from %s\r\n", req->uri, ipAddress);
        }

        // Redirect to the main page
        httpd_resp_set_status(req, "302 redirect");
        httpd_resp_set_hdr(req, "Location", "/");
        return httpd_resp_sendstr(req, "Redirect to Web Config");
    }

    // Display an error
    webSocketsGetClientIpAddressAndPort(req, ipAddress, sizeof(ipAddress));
    logMessage = "WebSockets, Page ";
    logMessage += req->uri;
    logMessage += " not found, cliient: ";
    logMessage += ipAddress;
    systemPrintln(logMessage);

    // Set the 404 status code
    const char * statusText = "Error 404, page not found";
    httpd_resp_set_status(req, statusText);

    // Set the content type
    httpd_resp_set_type(req, "text/html");

    // Send the response
    String htmlResponse = "<h1>";
    htmlResponse += statusText;
    htmlResponse += "</h1><p>The requested page (";
    htmlResponse += req->uri;
    htmlResponse += ") was not found.</p>";
    httpd_resp_send(req, htmlResponse.c_str(), htmlResponse.length());

    // Return ESP_OK to indicate the error was handled successfully
    return ESP_OK;
}

//----------------------------------------
// Determine if webSockets is connected to a client
//----------------------------------------
bool webSocketsIsConnected()
{
    return (webSocketsClientListHead != nullptr);
}

//----------------------------------------
// Determine if the web server is running
//----------------------------------------
bool webServerIsRunning()
{
    if (webServerState == WEBSERVER_STATE_RUNNING)
        return (true);
    return (false);
}

//----------------------------------------
// Break CSV into setting constituents
// Can't use strtok because we may have two commas next to each other, ie
// measurementRateHz,4.00,measurementRateSec,,dynamicModel,0,
//----------------------------------------
bool webSocketsParseIncomingSettings()
{
    char settingName[100] = {'\0'};
    char valueStr[150] = {'\0'}; // stationGeodetic1,ANameThatIsTooLongToBeDisplayed 40.09029479 -105.18505761 1560.089

    bool stationGeodeticSeen = false;
    bool stationECEFSeen = false;

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

        if (settings.debugWebServer == true)
            systemPrintf("settingName: %s value: %s\r\n", settingName, valueStr);

        // Check for first stationGeodetic
        if ((strstr(settingName, "stationGeodetic") != nullptr) && (!stationGeodeticSeen))
        {
            stationGeodeticSeen = true;
            removeFile(stationCoordinateGeodeticFileName);
            if (settings.debugWebServer == true)
                systemPrintln("Station geodetic coordinate file removed");
        }

        // Check for first stationECEF
        if ((strstr(settingName, "stationECEF") != nullptr) && (!stationECEFSeen))
        {
            stationECEFSeen = true;
            removeFile(stationCoordinateECEFFileName);
            if (settings.debugWebServer == true)
                systemPrintln("Station ECEF coordinate file removed");
        }

        updateSettingWithValue(false, settingName, valueStr);

        // Avoid infinite loop if response is malformed
        counter++;
        if (counter == maxAttempts)
        {
            systemPrintln("Error: Incoming settings malformed.");
            break;
        }
    }

    systemPrintln("Parsing complete");

    return (true);
}

//----------------------------------------
// Read the header into a buffer
//----------------------------------------
char * webSocketsReadHeader(httpd_req_t *req)
{
    char * buffer;
    size_t bufferLength;
    size_t bytes;
    char data;
    const char * errorMessage;
    char * previousBuffer;
    size_t previousLength;
    esp_err_t status;
    int terminatorCount;
    size_t totalBytes;

    // Locate the header
    buffer = nullptr;
    bufferLength = 0;
    totalBytes = 0;
    while (1)
    {
        if ((bufferLength == 0) || (totalBytes >= (bufferLength - 5)))
        {
            // Save the previous buffer
            previousBuffer = buffer;
            previousLength = totalBytes;

            // Increase the buffer size
            bufferLength = bufferLength ? bufferLength << 1 : 256;

            // Allocate a new buffer
            buffer = (char *)rtkMalloc(bufferLength, "WebSockets header");
            if (buffer == nullptr)
            {
                errorMessage = "ERROR: WebSockets failed to allocate header buffer";
                break;
            }

            // Copy any existing data into the buffer
            if (previousBuffer)
            {
                memcpy(buffer, previousBuffer, previousLength);
                rtkFree(previousBuffer, "WebSockets header");
                previousBuffer = nullptr;
            }
        }

        // Read in the header into the buffer
        while (totalBytes < (bufferLength - 5))
        {
            // Get the next header byte
            bytes = httpd_req_recv(req, &data, 1);
            if (bytes == 0)
            {
                errorMessage = "ERROR: WebSockets failed to read header!";
                break;
            }

            buffer[totalBytes++] = data;
            if (data == '\r')
                break;
        }

        // Handle the error
        if (bytes == 0)
            break;

        // Check for end of the header
        if (data == '\r')
        {
            // Read in the line feed
            bytes = httpd_req_recv(req, &data, 1);
            if (bytes == 0)
            {
                errorMessage = "ERROR: WebSockets failed to read header line feed!";
                break;
            }
            buffer[totalBytes++] = data;

            // Continue reading data if this is not a line feed
            if (data != '\n')
                continue;

            // Read in the second carriage return
            bytes = httpd_req_recv(req, &data, 1);
            if (bytes == 0)
            {
                errorMessage = "ERROR: WebSockets failed to read header second carriage return!";
                break;
            }
            buffer[totalBytes++] = data;

            // Continue reading data if this is not a carriage return
            if (data != '\r')
                continue;

            // Read in the second line feed
            bytes = httpd_req_recv(req, &data, 1);
            if (bytes == 0)
            {
                errorMessage = "ERROR: WebSockets failed to read header second line feed!";
                break;
            }
            buffer[totalBytes++] = data;

            // Continue reading data if this is not a line feed
            if (data != '\n')
                continue;

            // Zero terminate the line
            buffer[totalBytes++] = 0;
            return buffer;
        }

        // Expand the buffer
    }

    // Free the buffer
    if (buffer)
        rtkFree(buffer, "WebSockets header");

    // Respond with 500 Internal Server Error
    systemPrintln(errorMessage);
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, errorMessage);

    // Indicate an error
    return nullptr;
}

//----------------------------------------
// Read the separator into a buffer
//----------------------------------------
char * webSocketsReadSeparator(httpd_req_t *req)
{
    char * buffer;
    size_t bufferLength;
    size_t bytes;
    char data;
    const char * errorMessage;
    char * previousBuffer;
    size_t previousLength;
    esp_err_t status;
    int terminatorCount;
    size_t totalBytes;

    // Locate the separator
    buffer = nullptr;
    bufferLength = 0;
    totalBytes = 0;
    while (1)
    {
        // Save the previous buffer
        previousBuffer = buffer;
        previousLength = totalBytes;

        // Increase the buffer size
        bufferLength = bufferLength ? bufferLength << 1 : 128;

        // Allocate a new buffer
        buffer = (char *)rtkMalloc(bufferLength, "WebSockets separator");
        if (buffer == nullptr)
        {
            errorMessage = "ERROR: WebSockets failed to allocate separator buffer";
            break;
        }

        // Copy any existing data into the buffer
        if (previousBuffer)
        {
            memcpy(buffer, previousBuffer, previousLength);
            rtkFree(previousBuffer, "WebSockets separator");
            previousBuffer = nullptr;
        }

        // Read in the separator into the buffer
        while (totalBytes < (bufferLength - 3))
        {
            // Get the next separator byte
            bytes = httpd_req_recv(req, &data, 1);
            if (bytes == 0)
            {
                errorMessage = "ERROR: WebSockets failed to read separator!";
                break;
            }
            buffer[totalBytes] = data;

            // Check for end of separator
            if (data == '\r')
                break;

            // Account for this data byte
            totalBytes += 1;
        }

        // Handle the error
        if (bytes == 0)
            break;

        // Check for end of the separator
        if (data == '\r')
        {
            // Finish the separator
            // Read in the line feed
            bytes = httpd_req_recv(req, &data, 1);
            if (bytes == 0)
            {
                errorMessage = "ERROR: WebSockets failed to read separator line feed!";
                break;
            }

            // Verify the line feed
            if (data != '\n')
            {
                errorMessage = "ERROR: WebSockets separator did not find the expected line feed!\r\n";
                break;
            }

            // Zero terminate the line
            buffer[totalBytes] = 0;
            return buffer;
        }

        // Expand the buffer
    }

    // Free the buffer
    if (buffer)
        rtkFree(buffer, "WebSockets separator");

    // Respond with 500 Internal Server Error
    systemPrintln(errorMessage);
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, errorMessage);

    // Indicate an error
    return nullptr;
}

//----------------------------------------
// Read the contents of the request into a buffer
//----------------------------------------
uint8_t * webSocketsReadRequestContent(httpd_req_t *req)
{
    uint8_t * buffer;
    size_t bufferLength;
    size_t bytes;
    const char * errorMessage;

    // Locate the header
    do
    {
        // Allocate a new buffer
        bufferLength = req->content_len;
        buffer = (uint8_t *)rtkMalloc(bufferLength + 1, "WebSockets request content");
        if (buffer == nullptr)
        {
            errorMessage = "ERROR: WebSockets failed to allocate request content buffer";
            break;
        }

        // Read the request content into the buffer
        bytes = httpd_req_recv(req, (char *)buffer, bufferLength);
        if (bytes != bufferLength)
        {
            errorMessage = "ERROR: WebSockets failed to read request content!";
            break;
        }

        // Zero terminate the buffer
        buffer[bufferLength] = 0;
        return buffer;
    } while (0);

    // Free the buffer
    if (buffer)
        rtkFree(buffer, "WebSockets request content");

    // Respond with 500 Internal Server Error
    systemPrintln(errorMessage);
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, errorMessage);

    // Indicate an error
    return nullptr;
}

//----------------------------------------
// Register an error handler
//----------------------------------------
bool webSocketsRegisterErrorHandler(httpd_err_code_t error,
                                    httpd_err_handler_func_t handler)
{
    esp_err_t status;

    // Register the error handler
    status = httpd_register_err_handler(webSocketsHandle, error, handler);
    if (settings.debugWebServer == true)
    {
        if (status == ESP_OK)
            systemPrintf("webSockets registered %d error handler\r\n", error);
        else
            systemPrintf("webSockets Failed to register %d error handler!\r\n", error);
    }
    return (status == ESP_OK);
}

//----------------------------------------
// Register a webpage handler
//----------------------------------------
bool webSocketsRegisterPageHandler(const httpd_uri_t * page)
{
    esp_err_t status;

    // Register the handler
    status = httpd_register_uri_handler(webSocketsHandle, page);
    if (settings.debugWebServer == true)
    {
        if (status == ESP_OK)
            systemPrintf("webSockets registered %s handler\r\n", page->uri);
        else
            systemPrintf("webSockets Failed to register %s handler!\r\n", page->uri);
    }
    return (status == ESP_OK);
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
// Set the next webconfig state
//----------------------------------------
void webServerSetState(uint8_t newState)
{
    char string1[40];
    char string2[40];
    const char *arrow = nullptr;
    const char *asterisk = nullptr;
    const char *initialState = nullptr;
    const char *endingState = nullptr;

    // Display the state transition
    if (settings.debugWebServer)
    {
        arrow = "";
        asterisk = "";
        initialState = "";
        if (newState == webServerState)
            asterisk = "*";
        else
        {
            initialState = webServerGetStateName(webServerState, string1);
            arrow = " --> ";
        }
    }

    // Set the new state
    webServerState = newState;
    if (settings.debugWebServer)
    {
        // Display the new firmware update state
        endingState = webServerGetStateName(newState, string2);
        if (!online.rtc)
            systemPrintf("%s%s%s%s\r\n", asterisk, initialState, arrow, endingState);
        else
            // Timestamp the state change
            systemPrintf("%s%s%s%s, %s\r\n", asterisk, initialState, arrow, endingState, getTimeStamp());
    }

    // Validate the state
    if (newState >= WEBSERVER_STATE_MAX)
        reportFatalError("Web Server: Invalid web config state");
}

//----------------------------------------
// Start the Web Server state machine
//----------------------------------------
void webServerStart()
{
    // Display the heap state
    reportHeapNow(settings.debugWebServer);

    if (webServerState != WEBSERVER_STATE_OFF)
    {
        if (settings.debugWebServer)
            systemPrintln("Web Server: Already running!");
    }
    else
    {
        if (settings.debugWebServer)
            systemPrintln("Web Server: Starting");

        // Start the network
        if (networkInterfaceHasInternet(NETWORK_ETHERNET))
            networkConsumerAdd(NETCONSUMER_WEB_CONFIG, NETWORK_ANY, __FILE__, __LINE__);
        else if ((settings.wifiConfigOverAP == false) || networkInterfaceHasInternet(NETWORK_WIFI_STATION))
            networkConsumerAdd(NETCONSUMER_WEB_CONFIG, NETWORK_ANY, __FILE__, __LINE__);
        else if (settings.wifiConfigOverAP)
            networkSoftApConsumerAdd(NETCONSUMER_WEB_CONFIG, __FILE__, __LINE__);
        webServerSetState(WEBSERVER_STATE_WAIT_FOR_NETWORK);
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
    const GET_PAGE_HANDLER * setupPage;

    // Get the configuration object
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Use different ports for websocket and webServer - use port 81 for the websocket - also defined in main.js
    config.server_port = 81;
    config.stack_size = webSocketsStackSize;

    // Set the number of URI handlers
    config.max_uri_handlers = WEB_SOCKETS_TOTAL_PAGES + 16;
    config.max_resp_headers = config.max_uri_handlers;

    // Start the httpd server
    if (settings.debugWebServer == true)
        systemPrintf("webSockets starting on port: %d\r\n", config.server_port);

    if (settings.debugWebServer == true)
    {
        webSocketsHttpdDisplayConfig(&config);
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

    // Start the web server
    status = httpd_start(&webSocketsHandle, &config);
    if (status == ESP_OK)
    {
        do
        {
            int i;

            if (settings.debugWebServer == true)
                systemPrintln("webSockets registering page handlers");

            // Register the page not found (404) error handler
            if (!webSocketsRegisterErrorHandler(HTTPD_404_NOT_FOUND,
                                                webSocketsHandlerPageNotFound))
                break;

            // Get the product specific web page
            if (productVariant == RTK_EVK)
                setupPage = &webSocketsPages[0];
            else
                setupPage = &webSocketsPages[1];

            // Register the product specific page
            if (!webSocketsRegisterPageHandler(&setupPage->_page))
                break;

            // Register the web socket handler
            if (!webSocketsRegisterPageHandler(&webSocketsPage))
                break;

            // Register the main pages
            for (i = WEB_SOCKETS_SPECIAL_PAGES; i < WEB_SOCKETS_TOTAL_PAGES; i++)
                if (!webSocketsRegisterPageHandler(&webSocketsPages[i]._page))
                    break;
            if (i < WEB_SOCKETS_TOTAL_PAGES)
                break;

            // The web server is ready to handle incoming requests
            if (settings.debugWebServer)
                systemPrintf("webSockets successfully started\r\n");
            return true;
        } while (0);

        // Stop the web server
        httpd_stop(webSocketsHandle);
    }

    // Display the failure to start
    if (settings.debugWebServer)
        systemPrintf("ERROR: webSockets failed to start, status: %s!\r\n", esp_err_to_name(status));
    return false;
}

//----------------------------------------
// Stop the web config state machine
//----------------------------------------
void webServerStop()
{
    networkUserRemove(NETCONSUMER_WEB_CONFIG, __FILE__, __LINE__);
    if (webServerState != WEBSERVER_STATE_OFF)
    {
        webServerReleaseResources(); // Release web server resources

        // Stop network
        systemPrintln("Web Server releasing network request");
        networkSoftApConsumerRemove(NETCONSUMER_WEB_CONFIG, __FILE__, __LINE__);
        networkConsumerRemove(NETCONSUMER_WEB_CONFIG, NETWORK_ANY, __FILE__, __LINE__);

        // Stop the machine
        webServerSetState(WEBSERVER_STATE_OFF);
        if (settings.debugWebServer)
            systemPrintln("Web Server: Stopped");

        // Display the heap state
        reportHeapNow(settings.debugWebServer);
    }
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

//----------------------------------------
// State machine to handle the starting/stopping of the web server
//----------------------------------------
void webServerUpdate()
{
    bool connected;

    // Determine if the network is connected
    connected = networkConsumerIsConnected(NETCONSUMER_WEB_CONFIG);

    // Walk the state machine
    switch (webServerState)
    {
    default:
        systemPrintf("ERROR: Unknown Web Server state (%d)\r\n", webServerState);

        // Stop the machine
        webServerStop();
        break;

    case WEBSERVER_STATE_OFF:
        // Wait until webServerStart() is called
        break;

    // Wait for connection to the network
    case WEBSERVER_STATE_WAIT_FOR_NETWORK:
        // Wait until the network is connected to the internet or has WiFi AP
        if (connected || wifiSoftApRunning)
        {
            if (settings.debugWebServer)
                systemPrintln("Web Server connected to network");

            networkUserAdd(NETCONSUMER_WEB_CONFIG, __FILE__, __LINE__);
            webServerSetState(WEBSERVER_STATE_NETWORK_CONNECTED);
        }
        break;

    // Start the web server
    case WEBSERVER_STATE_NETWORK_CONNECTED: {
        // Determine if the network has failed
        if (connected == false && wifiSoftApRunning == false)
        {
            networkUserRemove(NETCONSUMER_WEB_CONFIG, __FILE__, __LINE__);
            webServerSetState(WEBSERVER_STATE_WAIT_FOR_NETWORK);
        }

        // Attempt to start the web server
        else if (webServerAssignResources(settings.httpPort) == true)
            webServerSetState(WEBSERVER_STATE_RUNNING);
    }
    break;

    // Allow web services
    case WEBSERVER_STATE_RUNNING:
        // Determine if the network has failed
        if (connected == false && wifiSoftApRunning == false)
        {
            webServerReleaseResources(); // Release web server resources
            webServerSetState(WEBSERVER_STATE_WAIT_FOR_NETWORK);
        }

        // This state is exited when webServerStop() is called

        break;
    }

    // Display an alive message
    if (PERIODIC_DISPLAY(PD_WEB_SERVER_STATE))
    {
        systemPrintf("Web Server state: %s\r\n", webServerStateNames[webServerState]);
        PERIODIC_CLEAR(PD_WEB_SERVER_STATE);
    }
}

//----------------------------------------
// Verify the web page descriptions
//----------------------------------------
void webServerVerifyTables()
{
    const GET_PAGE_HANDLER * endPage;
    uint32_t index;
    const GET_PAGE_HANDLER * startPage;
    const GET_PAGE_HANDLER * webpage;

    if (webServerStateEntries != WEBSERVER_STATE_MAX)
        reportFatalError("Fix webServerStateNames to match WebServerState");

    // Loop through all of the web page handlers
    startPage = (GET_PAGE_HANDLER *)&webSocketsPages;
    webpage = startPage;
    endPage = &startPage[WEB_SOCKETS_TOTAL_PAGES];
    while (webpage < endPage)
    {
        index = webpage - startPage;
        if ((uintptr_t)webpage->_page.user_ctx != index)
        {
            Serial.printf("Change GET_PAGE for %s from %d to %d\r\n",
                          webpage->_page.uri,
                          (uintptr_t)webpage->_page.user_ctx,
                          index);
            reportFatalError("webSockets: Fix GET_PAGE entry");
        }
        webpage++;
    }
}

#endif // COMPILE_AP
