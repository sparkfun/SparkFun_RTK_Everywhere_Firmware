/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
WebServer.ino

  Start and stop the web-server, provide the form and handle browser input.
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#ifdef COMPILE_AP

//----------------------------------------
// Locals
//----------------------------------------

void stopWebServer()
{
    if (incomingSettings != nullptr)
    {
        rtkFree(incomingSettings, "Settings buffer (incomingSettings)");
        incomingSettings = nullptr;
    }
}

//----------------------------------------
// Create the web server
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

        if (settings.mdnsEnable == true)
            MDNS.addService("http", "tcp", settings.httpPort); // Add service to MDNS

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

    online.webServer = false;

    webSocketsStop(); // Release socket resources

    if (incomingSettings != nullptr)
    {
        rtkFree(incomingSettings, "Settings buffer (incomingSettings)");
        incomingSettings = nullptr;
    }
}

#endif // COMPILE_AP
