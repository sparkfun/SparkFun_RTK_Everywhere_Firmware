/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
WebServer.ino

  Start and stop the web-server, provide the form and handle browser input.
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#ifdef COMPILE_AP

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
