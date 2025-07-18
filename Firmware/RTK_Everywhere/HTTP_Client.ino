/*------------------------------------------------------------------------------
HTTP_Client.ino

  The HTTP client sits on top of the network layer and handles Zero Touch
  Provisioning (ZTP) via a HTTP POST.

  Previously this functionality was in pointperfectProvisionDevice and
  pointperfectTryZtpToken but was problematic as it used a separate instance
  of NetworkClientSecure and had no control over the underlying network.

------------------------------------------------------------------------------*/

ZtpResponse ztpResponse =
    ZTP_NOT_STARTED; // Used in menuPP. This is the overall result of the ZTP process of testing multiple tokens

#ifdef COMPILE_HTTP_CLIENT

//----------------------------------------
// Constants
//----------------------------------------

// Give up connecting after this number of attempts
// Note: pointperfectProvisionDevice implements its own timeout.
// MAX_HTTP_CLIENT_CONNECTION_ATTEMPTS * httpClientConnectionAttemptTimeout should
// be less than the pointperfectProvisionDevice timeout.
static const int MAX_HTTP_CLIENT_CONNECTION_ATTEMPTS = 3;

// Define the HTTP client states
enum HTTPClientState
{
    HTTP_CLIENT_OFF = 0,
    HTTP_CLIENT_NETWORK_STARTED,     // Connecting to WiFi access point or Ethernet
    HTTP_CLIENT_CONNECTION_DELAY,    // Delay before connecting to HTTP server
    HTTP_CLIENT_CONNECTING_2_SERVER, // Connecting to the HTTP server
    HTTP_CLIENT_CONNECTED,           // Connected to the HTTP services
    HTTP_CLIENT_COMPLETE,            // Complete. Can not or do not need to continue
    // Insert new states here
    HTTP_CLIENT_STATE_MAX // Last entry in the state list
};

const char *const httpClientStateName[] = {
    "HTTP_CLIENT_OFF",
    "HTTP_CLIENT_NETWORK_STARTED",
    "HTTP_CLIENT_CONNECTION_DELAY",
    "HTTP_CLIENT_CONNECTING_2_SERVER",
    "HTTP_CLIENT_CONNECTED",
    "HTTP_CLIENT_COMPLETE",
};

const int httpClientStateNameEntries = sizeof(httpClientStateName) / sizeof(httpClientStateName[0]);

//----------------------------------------
// Locals
//----------------------------------------

static HTTPClient *httpClient = nullptr;

JsonDocument *jsonZtp = nullptr;
char *tempHolderPtr = nullptr;

// Throttle the time between connection attempts
static int httpClientConnectionAttempts; // Count the number of connection attempts between restarts
static uint32_t httpClientConnectionAttemptTimeout;
static int httpClientConnectionAttemptsTotal; // Count the number of connection attempts absolutely

static volatile uint32_t httpClientLastDataReceived; // Last time data was received via HTTP

static NetworkClientSecure *httpSecureClient;

static volatile uint8_t httpClientState = HTTP_CLIENT_OFF;

// HTTP client timer usage:
//  * Reconnection delay
//  * Measure the connection response time
//  * Receive HTTP data timeout
static uint32_t httpClientStartTime; // For calculating uptime
static uint32_t httpClientTimer;

//----------------------------------------
// HTTP Client Routines
//----------------------------------------

//----------------------------------------
// Determine if another connection is possible or if the limit has been reached
//----------------------------------------
bool httpClientConnectLimitReached()
{
    bool limitReached;
    int minutes;
    int seconds;

    // Retry the connection a few times
    limitReached = (httpClientConnectionAttempts >= MAX_HTTP_CLIENT_CONNECTION_ATTEMPTS);

    // Restart the HTTP client
    httpClientStop(limitReached || (!httpClientEnabled(nullptr)));

    // Limit to max connection delay
    if (httpClientConnectionAttempts)
        httpClientConnectionAttemptTimeout = (5 * MILLISECONDS_IN_A_SECOND) << (httpClientConnectionAttempts - 1);
    if (httpClientConnectionAttemptTimeout > RTK_MAX_CONNECTION_MSEC)
        httpClientConnectionAttemptTimeout = httpClientConnectionAttemptTimeout;
    else
        httpClientConnectionAttempts++;
    httpClientConnectionAttemptsTotal++;
    if (settings.debugHttpClientState)
        httpClientPrintStatus();

    if (limitReached == false)
    {
        // Display the delay before starting the HTTP client
        if (settings.debugHttpClientState && httpClientConnectionAttemptTimeout)
        {
            seconds = httpClientConnectionAttemptTimeout / MILLISECONDS_IN_A_SECOND;
            minutes = seconds / SECONDS_IN_A_MINUTE;
            seconds -= minutes * SECONDS_IN_A_MINUTE;
            systemPrintf("HTTP Client trying again in %d:%02d seconds.\r\n", minutes, seconds);
        }
    }
    else
        // No more connection attempts
        systemPrintln("HTTP Client connection attempts exceeded!");

    return limitReached;
}

//----------------------------------------
// Determine if the HTTP client may be enabled
//----------------------------------------
bool httpClientEnabled(const char **line)
{
    bool enableHttpClient;

    do
    {
        enableHttpClient = false;

        // HTTP requires use of point perfect corrections
        if (pointPerfectIsEnabled() == false)
        {
            if (line)
                *line = ", PointPerfect corrections disabled!";
            break;
        }

        // All conditions support running the HTTP client
        enableHttpClient = httpClientModeNeeded;
        if (line && !enableHttpClient)
            *line = ", HTTP Client disabled!";
    } while (0);
    return enableHttpClient;
}

//----------------------------------------
// Print the HTTP client state summary
//----------------------------------------
void httpClientPrintStateSummary()
{
    switch (httpClientState)
    {
    default:
        systemPrintf("Unknown: %d", httpClientState);
        break;
    case HTTP_CLIENT_OFF:
        systemPrint("Off");
        break;

    case HTTP_CLIENT_NETWORK_STARTED:
    case HTTP_CLIENT_CONNECTION_DELAY:
        systemPrint("Disconnected");
        break;

    case HTTP_CLIENT_CONNECTING_2_SERVER:
        systemPrint("Connecting");
        break;

    case HTTP_CLIENT_CONNECTED:
        systemPrint("Connected");
        break;

    case HTTP_CLIENT_COMPLETE:
        systemPrint("Complete");
        break;
    }
}

//----------------------------------------
// Print the HTTP Client status
//----------------------------------------
void httpClientPrintStatus()
{
    systemPrint("HTTP Client ");
    httpClientPrintStateSummary();
    systemPrintln();
}

//----------------------------------------
// Restart the HTTP client
//----------------------------------------
void httpClientRestart()
{
    // Save the previous uptime value
    if (httpClientState == HTTP_CLIENT_CONNECTED)
        httpClientStartTime = httpClientTimer - httpClientStartTime;
    httpClientConnectLimitReached();
}

//----------------------------------------
// Update the state of the HTTP client state machine
//----------------------------------------
void httpClientSetState(uint8_t newState)
{
    if (settings.debugHttpClientState)
    {
        if (httpClientState == newState)
            systemPrint("*");
        else
            systemPrintf("%s --> ", httpClientStateName[httpClientState]);
    }
    httpClientState = newState;
    if (settings.debugHttpClientState)
    {
        if (newState >= HTTP_CLIENT_STATE_MAX)
        {
            systemPrintf("Unknown HTTP Client state: %d\r\n", newState);
            reportFatalError("Unknown HTTP Client state");
        }
        else
            systemPrintln(httpClientStateName[httpClientState]);
    }
}

//----------------------------------------
// Shutdown the HTTP client
//----------------------------------------
void httpClientShutdown()
{
    httpClientStop(true);
}

//----------------------------------------
// Start the HTTP client
//----------------------------------------
void httpClientStart()
{
    // Display the heap state
    reportHeapNow(settings.debugHttpClientState);

    // Start the HTTP client
    systemPrintln("HTTP Client start");
    httpClientStop(false);
}

//----------------------------------------
// Shutdown or restart the HTTP client
//----------------------------------------
void httpClientStop(bool shutdown)
{
    // Free the httpClient resources
    if (httpClient)
    {
        if (settings.debugHttpClientState)
            systemPrintln("Freeing httpClient");

        delete httpClient;
        httpClient = nullptr;
        reportHeapNow(settings.debugHttpClientState);
    }

    // Free the httpSecureClient resources
    if (httpSecureClient)
    {
        if (settings.debugHttpClientState)
            systemPrintln("Freeing httpSecureClient");
        // delete httpSecureClient; // Don't. This causes issue #335
        httpSecureClient = nullptr;
        reportHeapNow(settings.debugHttpClientState);
    }

    // Increase timeouts if we started the network
    if (httpClientState > HTTP_CLIENT_NETWORK_STARTED)
        // Mark the Client stop so that we don't immediately attempt re-connect to Caster
        httpClientTimer = millis();

    // Determine the next HTTP client state
    online.httpClient = false;
    networkConsumerOffline(NETCONSUMER_HTTP_CLIENT);
    if (shutdown)
    {
        networkConsumerRemove(NETCONSUMER_HTTP_CLIENT, NETWORK_ANY, __FILE__, __LINE__);
        httpClientModeNeeded = false;
        httpClientConnectionAttempts = 0;
        httpClientConnectionAttemptTimeout = 0;
        httpClientSetState(HTTP_CLIENT_OFF);
        systemPrintln("HTTP Client stopped");
    }
    else
        httpClientSetState(HTTP_CLIENT_NETWORK_STARTED);
}

//----------------------------------------
// Update the state of the HTTP client
//----------------------------------------
void httpClientUpdate()
{
    bool connected;
    bool enabled;

    // Shutdown the HTTP client when the mode or setting changes
    DMW_st(httpClientSetState, httpClientState);
    connected = networkConsumerIsConnected(NETCONSUMER_HTTP_CLIENT);
    enabled = httpClientEnabled(nullptr);
    if ((enabled == false) && (httpClientState > HTTP_CLIENT_OFF))
        httpClientShutdown();

    // Determine if the network has failed
    else if ((httpClientState > HTTP_CLIENT_NETWORK_STARTED) && !connected)
        httpClientRestart();

    // Enable the network and the HTTP client if requested
    switch (httpClientState)
    {
    default:
    case HTTP_CLIENT_OFF: {
        if (enabled)
        {
            httpClientStart();
            networkConsumerAdd(NETCONSUMER_HTTP_CLIENT, NETWORK_ANY, __FILE__, __LINE__);
        }
        break;
    }

    // Wait for a network media connection
    case HTTP_CLIENT_NETWORK_STARTED: {
        // Wait until the network is connected to the media
        if (connected)
        {
            // Reset the timeout when the network changes
            if (networkChanged(NETCONSUMER_HTTP_CLIENT))
                httpClientConnectionAttemptTimeout = 0;
            networkUserAdd(NETCONSUMER_HTTP_CLIENT, __FILE__, __LINE__);
            httpClientSetState(HTTP_CLIENT_CONNECTION_DELAY);
        }
        break;
    }

    // Delay before connecting to HTTP server
    case HTTP_CLIENT_CONNECTION_DELAY: {
        if ((millis() - httpClientTimer) > httpClientConnectionAttemptTimeout)
        {
            httpClientSetState(HTTP_CLIENT_CONNECTING_2_SERVER);
        }
        break;
    }

    // Connect to the HTTP server
    case HTTP_CLIENT_CONNECTING_2_SERVER: {
        // Allocate the httpSecureClient structure
        networkUseDefaultInterface();
        httpSecureClient = new NetworkClientSecure();
        if (!httpSecureClient)
        {
            systemPrintln("ERROR: Failed to allocate the httpSecureClient structure!");
            httpClientShutdown();
            break;
        }

        // Set the Amazon Web Services public certificate
        httpSecureClient->setCACert(AWS_PUBLIC_CERT);

        // Connect to the server
        if (!httpSecureClient->connect(THINGSTREAM_SERVER, HTTPS_PORT))
        {
            // Failed to connect to the server
            systemPrintln("ERROR: Failed to connect to the Thingstream server!");
            httpClientRestart(); // I _think_ we want to restart here - i.e. retry after the timeout?
            break;
        }

        // Allocate the httpClient structure
        httpClient = new HTTPClient();
        if (!httpClient)
        {
            // Failed to allocate the httpClient structure
            systemPrintln("ERROR: Failed to allocate the httpClient structure!");
            httpClientShutdown();
            break;
        }

        if (settings.debugHttpClientState)
            systemPrintf("HTTP client connecting to %s\r\n", THINGSTREAM_ZTPURL);

        // Begin the HTTP client
        if (!httpClient->begin(*httpSecureClient, THINGSTREAM_ZTPURL))
        {
            systemPrintln("ERROR: Failed to start httpClient!\r\n");
            httpClientRestart(); // I _think_ we want to restart here - i.e. retry after the timeout?
            break;
        }

        reportHeapNow(settings.debugHttpClientState);
        online.httpClient = true;
        httpClientSetState(HTTP_CLIENT_CONNECTED);

        break;
    }

    case HTTP_CLIENT_CONNECTED: {
        String ztpRequest;
        createZtpRequest(ztpRequest);

        if (settings.debugCorrections || settings.debugHttpClientData)
        {
            systemPrintf("Sending JSON, %d bytes\r\n", ztpRequest.length());
            // systemPrintln(ztpRequest.c_str());
        }

        httpClient->addHeader(F("Content-Type"), F("application/json"));
        int httpResponseCode = httpClient->POST(ztpRequest.c_str());
        String response = httpClient->getString();
        if (settings.debugCorrections || settings.debugHttpClientData)
        {
            systemPrint("HTTP response: ");
            systemPrintln(response.c_str());
        }

        if (httpResponseCode != 200) // Connection failed
        {
            if (settings.debugCorrections || settings.debugHttpClientData)
            {
                systemPrintf("HTTP response error %d: ", httpResponseCode);
                systemPrintln(response.c_str());
            }

            // "HTTP response error -11 " = 411 which is length required
            // https://stackoverflow.com/questions/19227142/http-status-code-411-length-required
            if (httpResponseCode == -11)
            {
                if (settings.debugCorrections || settings.debugHttpClientData)
                {
                    systemPrintln("HTTP response error 411: Length Required. Retrying...");
                    systemPrintln(response.c_str());
                }

                httpClientRestart();
                break;
            }

            // If a device has already been registered on a different ZTP profile, response will be:
            // "HTTP response error 403: Device already registered"
            else if (response.indexOf("Device already registered") >= 0)
            {
                if (settings.debugCorrections || settings.debugHttpClientData)
                    systemPrintln("Device already registered to different profile");
                ztpResponse = ZTP_ALREADY_REGISTERED;
                httpClientSetState(HTTP_CLIENT_COMPLETE);
                break;
            }
            // If a device has been deactivated, response will be: "HTTP response error 403: No plan for device
            // device:9f19e97f-e6a7-4808-8d58-ac7ecac90e23"
            else if (response.indexOf("No plan for device") >= 0)
            {
                if (settings.debugCorrections || settings.debugHttpClientData)
                    systemPrintln("Device has been deactivated.");

                ztpResponse = ZTP_DEACTIVATED;
                httpClientSetState(HTTP_CLIENT_COMPLETE);
                break;
            }
            // If a device is not whitelisted, response will be: "HTTP response error 403: Device hardware code not
            // whitelisted"
            else if (response.indexOf("not whitelisted") >= 0)
            {
                if (settings.debugCorrections || settings.debugHttpClientData)
                    systemPrintln("Device not whitelisted.");
                ztpResponse = ZTP_NOT_WHITELISTED;
                httpClientSetState(HTTP_CLIENT_COMPLETE);
                break;
            }

            // Unknown. Report and shut down.
            else
            {
                systemPrintf("HTTP response error %d: ", httpResponseCode);
                systemPrintln(response.c_str());
                ztpResponse = ZTP_UNKNOWN_ERROR;
                httpClientSetState(HTTP_CLIENT_COMPLETE);
                break;
            }
        }

        else
        {
            // Device is now active with ThingStream
            // Pull pertinent values from response
            jsonZtp = new JsonDocument;
            if (!jsonZtp)
            {
                systemPrintln("ERROR - Failed to allocate jsonZtp!\r\n");
                httpClientShutdown(); // Try again?
                break;
            }

            DeserializationError error = deserializeJson(*jsonZtp, response);
            if (DeserializationError::Ok != error)
            {
                systemPrintln("JSON error");
                httpClientShutdown(); // Try again
                break;
            }
            else
            {
                // Depending on the service selected, we will get different responses
                if (pointPerfectServiceUsesKeys() == true)
                {
                    // Handle a PointPerfect 'keys' response
                    tempHolderPtr = (char *)rtkMalloc(MQTT_CERT_SIZE, "Certificate buffer (tempHolderPtr)");

                    if (!tempHolderPtr)
                    {
                        systemPrintln("ERROR - Failed to allocate tempHolderPtr buffer!\r\n");
                        httpClientShutdown(); // Try again?
                        break;
                    }

                    // Check if the JSON blob contains a certificate and private key
                    // If this is the first time this device has connected to ThingStream, they
                    // will not be present and we need to retry
                    // httpClientConnectLimitReached will prevent excessive retries
                    if (((const char *)((*jsonZtp)["certificate"]) == nullptr) ||
                        ((const char *)((*jsonZtp)["privateKey"]) == nullptr))
                    {
                        systemPrintln("ERROR - certificate or privateKey not found!\r\n");
                        httpClientRestart(); // Try again - allow time for ThingStream to finish activating the device
                                             // on plan
                        break;
                    }

                    strncpy(tempHolderPtr, (const char *)((*jsonZtp)["certificate"]), MQTT_CERT_SIZE - 1);
                    recordFile("certificate", tempHolderPtr, strlen(tempHolderPtr));

                    strncpy(tempHolderPtr, (const char *)((*jsonZtp)["privateKey"]), MQTT_CERT_SIZE - 1);
                    recordFile("privateKey", tempHolderPtr, strlen(tempHolderPtr));

                    rtkFree(tempHolderPtr, "Certificate buffer (tempHolderPtr)"); // Clean up. Done with tempHolderPtr

                    // Validate the keys
                    if (!checkCertificates())
                    {
                        systemPrintln("ERROR - Failed to validate the Point Perfect certificates!");
                    }
                    else
                    {
                        // Pull out MQTT settings and keys from remaining JSON
                        if (settings.debugPpCertificate || settings.debugHttpClientData)
                            systemPrintln("Certificates recorded successfully.");

                        strncpy(settings.pointPerfectClientID, (const char *)((*jsonZtp)["clientId"]),
                                sizeof(settings.pointPerfectClientID));
                        strncpy(settings.pointPerfectBrokerHost, (const char *)((*jsonZtp)["brokerHost"]),
                                sizeof(settings.pointPerfectBrokerHost));

                        // Note: from the ZTP documentation:
                        // ["subscriptions"][0] will contain the key distribution topic
                        // But, assuming the key distribution topic is always ["subscriptions"][0] is potentially
                        // brittle. It is safer to check the "description" contains "key distribution topic". If we are
                        // on an IP-only plan, the path will be /pp/ubx/0236/ip If we are on a L-Band-only or L-Band+IP
                        // plan, the path will be /pp/ubx/0236/Lb These 0236 key distribution topics provide the keys in
                        // UBX format, ready to be pushed to a ZED. There are also /pp/key/ip and /pp/key/Lb topics
                        // which provide the keys in JSON format - but we don't use those.
                        int subscription =
                            findZtpJSONEntryTnT("subscriptions", "description", "key distribution topic", jsonZtp);
                        if (subscription >= 0)
                            strncpy(settings.pointPerfectKeyDistributionTopic,
                                    (const char *)((*jsonZtp)["subscriptions"][subscription]["path"]),
                                    sizeof(settings.pointPerfectKeyDistributionTopic));

                        // "subscriptions" will also contain the correction topics for all available regional areas -
                        // for IP-only or L-Band+IP We should store those too, and then allow the user to select the one
                        // for their regional area
                        for (int r = 0; r < numRegionalAreas; r++)
                        {
                            char findMe[40];
                            snprintf(findMe, sizeof(findMe), "correction topic for %s",
                                     Regional_Information_Table[r].name); // Search for "US" etc.
                            subscription =
                                findZtpJSONEntryTnT("subscriptions", "description", (const char *)findMe, jsonZtp);
                            if (subscription >= 0)
                                strncpy(settings.regionalCorrectionTopics[r],
                                        (const char *)((*jsonZtp)["subscriptions"][subscription]["path"]),
                                        sizeof(settings.regionalCorrectionTopics[0]));
                            else
                                settings.regionalCorrectionTopics[r][0] =
                                    0; // Erase any invalid (non-plan) correction topics. Just in case the plan has
                                       // changed.
                        }

                        // "subscriptions" also contains the geographic area definition topic for each region for
                        // localized distribution. We can cheat by appending "/gad" to the correction topic. TODO: think
                        // about doing this properly.

                        // Now we extract the current and next key pair
                        strncpy(settings.pointPerfectCurrentKey,
                                (const char *)((*jsonZtp)["dynamickeys"]["current"]["value"]),
                                sizeof(settings.pointPerfectCurrentKey));
                        settings.pointPerfectCurrentKeyDuration = (*jsonZtp)["dynamickeys"]["current"]["duration"];
                        settings.pointPerfectCurrentKeyStart = (*jsonZtp)["dynamickeys"]["current"]["start"];

                        strncpy(settings.pointPerfectNextKey,
                                (const char *)((*jsonZtp)["dynamickeys"]["next"]["value"]),
                                sizeof(settings.pointPerfectNextKey));
                        settings.pointPerfectNextKeyDuration = (*jsonZtp)["dynamickeys"]["next"]["duration"];
                        settings.pointPerfectNextKeyStart = (*jsonZtp)["dynamickeys"]["next"]["start"];

                        if (settings.debugCorrections || settings.debugHttpClientData)
                            pointperfectPrintKeyInformation("HTTP Client");

                        // displayKeysUpdated();

                        ztpResponse = ZTP_SUCCESS; // Report success to provisioningUpdate()

                        httpClientSetState(HTTP_CLIENT_COMPLETE);
                    } // Valid certificates
                } // End handle keys type response
                else if (pointPerfectNtripNeeded() == true)
                {
                    // We received a JSON blob containing NTRIP credentials
                    if (settings.debugPpCertificate || settings.debugHttpClientData)
                        systemPrintf("PointPerfect response: %s\r\n", response.c_str());

                    // Check if the JSON blob contains rtcmCredentials
                    // If this is the first time this device has connected to ThingStream, rtcmCredentials
                    // endpoint will not be present and we need to retry
                    // httpClientConnectLimitReached will prevent excessive retries
                    if ((const char *)((*jsonZtp)["rtcmCredentials"]["endpoint"]) == nullptr)
                    {
                        systemPrintln("ERROR - rtcmCredentials not found!\r\n");
                        httpClientRestart(); // Try again - allow time for ThingStream to finish activating the device
                                             // on plan
                        break;
                    }

                    strncpy(settings.ntripClient_CasterHost, (const char *)((*jsonZtp)["rtcmCredentials"]["endpoint"]),
                            sizeof(settings.ntripClient_CasterHost));
                    settings.ntripClient_CasterPort = (*jsonZtp)["rtcmCredentials"]["httpPort"];

                    // If region is determined, override NTRIP Settings
                    settings.enableNtripClient = true;
                    settings.ntripClient_TransmitGGA = true;

                    strncpy(settings.ntripClient_CasterUser, (const char *)((*jsonZtp)["rtcmCredentials"]["userName"]),
                            sizeof(settings.ntripClient_CasterUser));
                    strncpy(settings.ntripClient_CasterUserPW,
                            (const char *)((*jsonZtp)["rtcmCredentials"]["password"]),
                            sizeof(settings.ntripClient_CasterUserPW));
                    strncpy(settings.ntripClient_MountPoint,
                            (const char *)((*jsonZtp)["rtcmCredentials"]["mountPoint"]),
                            sizeof(settings.ntripClient_MountPoint));

                    if (settings.debugCorrections || settings.debugHttpClientData)
                        pointperfectPrintNtripInformation("HTTP Client");

                    ztpResponse = ZTP_SUCCESS; // Report success to provisioningUpdate()

                    httpClientSetState(HTTP_CLIENT_COMPLETE);
                } // End handle NTRIP/RTCM response
                else
                {
                    systemPrintf("Error: Unhandled PointPerfect service response");
                    delay(2000);
                }

            } // JSON Deserialized correctly
        } // HTTP Response was 200
        break;
    }

    // The ZTP HTTP POST is complete. We either can not or do not want to continue.
    // Hang here until httpClientModeNeeded is set to false by updateProvisioning
    case HTTP_CLIENT_COMPLETE:
        break;
    }

    // Periodically display the HTTP client state
    if (PERIODIC_DISPLAY(PD_HTTP_CLIENT_STATE))
    {
        const char *line = "";
        httpClientEnabled(&line);
        systemPrintf("HTTP Client state: %s%s\r\n", httpClientStateName[httpClientState], line);
        PERIODIC_CLEAR(PD_HTTP_CLIENT_STATE);
    }
}

//----------------------------------------
// Verify the HTTP client tables
//----------------------------------------
void httpClientValidateTables()
{
    if (httpClientStateNameEntries != HTTP_CLIENT_STATE_MAX)
        reportFatalError("Fix httpClientStateNameEntries to match HTTPClientState");
}

#endif // COMPILE_HTTP_CLIENT
