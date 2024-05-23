#include "mbedtls/ssl.h" //Needed for certificate validation

//----------------------------------------
// Locals - compiled out
//----------------------------------------

// The PointPerfect token is provided at compile time via build flags
#define DEVELOPMENT_TOKEN 0xAA, 0xBB, 0xCC, 0xDD, 0x00, 0x11, 0x22, 0x33, 0x0A, 0x0B, 0x0C, 0x0D, 0x00, 0x01, 0x02, 0x03

#ifndef POINTPERFECT_LBAND_TOKEN
#warning Using the DEVELOPMENT_TOKEN for point perfect!
#define POINTPERFECT_LBAND_TOKEN DEVELOPMENT_TOKEN
#define POINTPERFECT_IP_TOKEN DEVELOPMENT_TOKEN
#define POINTPERFECT_LBAND_IP_TOKEN DEVELOPMENT_TOKEN
#endif // POINTPERFECT_LBAND_TOKEN

static const uint8_t developmentToken[16] = {DEVELOPMENT_TOKEN};         // Token in HEX form
static const uint8_t ppLbandToken[16] = {POINTPERFECT_LBAND_TOKEN};      // Token in HEX form
static const uint8_t ppIpToken[16] = {POINTPERFECT_IP_TOKEN};            // Token in HEX form
static const uint8_t ppLbandIpToken[16] = {POINTPERFECT_LBAND_IP_TOKEN}; // Token in HEX form

#ifdef COMPILE_WIFI
static const char *pointPerfectAPI = "https://api.thingstream.io/ztp/pointperfect/credentials";
MqttClient *menuppMqttClient;
#endif // COMPILE_WIFI

//----------------------------------------
// Forward declarations - compiled out
//----------------------------------------

void checkRXMCOR(UBX_RXM_COR_data_t *ubxDataStruct);

//----------------------------------------
// L-Band Routines - compiled out
//----------------------------------------

void menuPointPerfectKeys()
{
    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: PointPerfect Keys");

        systemPrint("1) Set ThingStream Device Profile Token: ");
        if (strlen(settings.pointPerfectDeviceProfileToken) > 0)
            systemPrintln(settings.pointPerfectDeviceProfileToken);
        else
            systemPrintln("Use SparkFun Token");

        systemPrint("2) Set Current Key: ");
        if (strlen(settings.pointPerfectCurrentKey) > 0)
            systemPrintln(settings.pointPerfectCurrentKey);
        else
            systemPrintln("N/A");

        systemPrint("3) Set Current Key Expiration Date (DD/MM/YYYY): ");
        if (strlen(settings.pointPerfectCurrentKey) > 0 && settings.pointPerfectCurrentKeyStart > 0 &&
            settings.pointPerfectCurrentKeyDuration > 0)
        {
            long long gpsEpoch = thingstreamEpochToGPSEpoch(settings.pointPerfectCurrentKeyStart);

            gpsEpoch += (settings.pointPerfectCurrentKeyDuration / 1000) -
                        1; // Add key duration back to the key start date to get key expiration

            systemPrintf("%s\r\n", printDateFromGPSEpoch(gpsEpoch));

            if (settings.debugCorrections == true)
                systemPrintf("settings.pointPerfectCurrentKeyDuration: %lld (%s)\r\n",
                             settings.pointPerfectCurrentKeyDuration,
                             printDaysFromDuration(settings.pointPerfectCurrentKeyDuration));
        }
        else
            systemPrintln("N/A");

        systemPrint("4) Set Next Key: ");
        if (strlen(settings.pointPerfectNextKey) > 0)
            systemPrintln(settings.pointPerfectNextKey);
        else
            systemPrintln("N/A");

        systemPrint("5) Set Next Key Expiration Date (DD/MM/YYYY): ");
        if (strlen(settings.pointPerfectNextKey) > 0 && settings.pointPerfectNextKeyStart > 0 &&
            settings.pointPerfectNextKeyDuration > 0)
        {
            long long gpsEpoch = thingstreamEpochToGPSEpoch(settings.pointPerfectNextKeyStart);

            gpsEpoch += (settings.pointPerfectNextKeyDuration /
                         1000); // Add key duration back to the key start date to get key expiration

            systemPrintf("%s\r\n", printDateFromGPSEpoch(gpsEpoch));
        }
        else
            systemPrintln("N/A");

        systemPrintln("x) Exit");

        int incoming = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

        if (incoming == 1)
        {
            systemPrint("Enter Device Profile Token: ");
            getUserInputString(settings.pointPerfectDeviceProfileToken,
                               sizeof(settings.pointPerfectDeviceProfileToken));
        }
        else if (incoming == 2)
        {
            systemPrint("Enter Current Key: ");
            getUserInputString(settings.pointPerfectCurrentKey, sizeof(settings.pointPerfectCurrentKey));
        }
        else if (incoming == 3)
        {
            clearBuffer();

            systemPrintln("Enter Current Key Expiration Date: ");
            uint8_t expDay;
            uint8_t expMonth;
            uint16_t expYear;
            while (getDate(expDay, expMonth, expYear) == false)
            {
                systemPrintln("Date invalid. Please try again.");
            }

            dateToKeyStart(expDay, expMonth, expYear, &settings.pointPerfectCurrentKeyStart);

            // The u-blox API reports key durations of 5 weeks, but the web interface reports expiration dates
            // that are 4 weeks.
            // If the user has manually entered a date, force duration down to four weeks
            settings.pointPerfectCurrentKeyDuration = (1000LL * 60 * 60 * 24 * 28);

            // Calculate the next key expiration date
            settings.pointPerfectNextKeyStart = settings.pointPerfectCurrentKeyStart +
                                                settings.pointPerfectCurrentKeyDuration +
                                                1; // Next key starts after current key
            settings.pointPerfectNextKeyDuration = settings.pointPerfectCurrentKeyDuration;

            if (settings.debugCorrections == true)
                pointperfectPrintKeyInformation();
        }
        else if (incoming == 4)
        {
            systemPrint("Enter Next Key: ");
            getUserInputString(settings.pointPerfectNextKey, sizeof(settings.pointPerfectNextKey));
        }
        else if (incoming == 5)
        {
            clearBuffer();

            systemPrintln("Enter Next Key Expiration Date: ");
            uint8_t expDay;
            uint8_t expMonth;
            uint16_t expYear;
            while (getDate(expDay, expMonth, expYear) == false)
            {
                systemPrintln("Date invalid. Please try again.");
            }

            dateToKeyStart(expDay, expMonth, expYear, &settings.pointPerfectNextKeyStart);
        }
        else if (incoming == INPUT_RESPONSE_GETNUMBER_EXIT)
            break;
        else if (incoming == INPUT_RESPONSE_GETNUMBER_TIMEOUT)
            break;
        else
            printUnknown(incoming);
    }

    clearBuffer(); // Empty buffer of any newline chars
}

// Given a GPS Epoch, return a DD/MM/YYYY string
char *printDateFromGPSEpoch(long long gpsEpoch)
{
    uint16_t keyGPSWeek;
    uint32_t keyGPSToW;
    epochToWeekToW(gpsEpoch, &keyGPSWeek, &keyGPSToW);

    long expDay;
    long expMonth;
    long expYear;
    gpsWeekToWToDate(keyGPSWeek, keyGPSToW, &expDay, &expMonth, &expYear);

    char *response = (char *)malloc(strlen("01/01/1010"));

    sprintf(response, "%02ld/%02ld/%ld", expDay, expMonth, expYear);
    return (response);
}

// Given a Unix Epoch, return a DD/MM/YYYY string
// https://www.epochconverter.com/programming/c
char *printDateFromUnixEpoch(long long unixEpoch)
{
    char *buf = (char *)malloc(strlen("01/01/2023") + 1); // Make room for terminator
    time_t rawtime = unixEpoch;

    struct tm ts;
    ts = *localtime(&rawtime);

    // Format time, "dd/mm/yyyy"
    strftime(buf, strlen("01/01/2023") + 1, "%d/%m/%Y", &ts);
    return (buf);
}

// Given a duration in ms, print days
char *printDaysFromDuration(long long duration)
{
    float days = duration / (1000.0 * 60 * 60 * 24); // Convert ms to days

    char *response = (char *)malloc(strlen("34.9") + 1); // Make room for terminator
    sprintf(response, "%0.2f", days);
    return (response);
}

// Connect to 'home' WiFi and then ThingStream API. This will attach this unique device to the ThingStream network.
bool pointperfectProvisionDevice()
{
#ifdef COMPILE_WIFI
    bool retVal = false;

    uint8_t provisionAttempt = 0;
    const uint8_t maxProvisionAttempts = 2;

    do
    {
        provisionAttempt++;

        char hardwareID[15];
        snprintf(hardwareID, sizeof(hardwareID), "%02X%02X%02X%02X%02X%02X%02X", btMACAddress[0], btMACAddress[1],
                 btMACAddress[2], btMACAddress[3], btMACAddress[4], btMACAddress[5],
                 productVariant); // Get ready for JSON

#ifdef WHITELISTED_ID
        // Override ID with testing ID
        snprintf(hardwareID, sizeof(hardwareID), "%02X%02X%02X%02X%02X%02X%02X", whitelistID[0], whitelistID[1],
                 whitelistID[2], whitelistID[3], whitelistID[4], whitelistID[5], productVariant);

        systemPrintf("Using whitelist hardware ID: %s\r\n", hardwareID);
#endif // WHITELISTED_ID

        // Given name must be between 1 and 50 characters
        char givenName[100];
        char versionString[9];
        getFirmwareVersion(versionString, sizeof(versionString), false);

        StaticJsonDocument<256> pointPerfectAPIPost;

        char tokenString[37] = "\0";
        char tokenChar;

        // Determine if we use the SparkFun tokens or custom token
        if (strlen(settings.pointPerfectDeviceProfileToken) == 0)
        {
            // Use the built-in SparkFun tokens

            // Depending on how many times we've tried the ZTP interface, change the token
            pointperfectGetToken(tokenString);

            if (memcmp(ppLbandToken, developmentToken, sizeof(developmentToken)) == 0)
                systemPrintln("Warning: Using the development token!");

            if (settings.debugCorrections == true)
            {
                // Don't expose the SparkFun tokens
                tokenChar = tokenString[4];
                tokenString[4] = 0;
                systemPrintf("Using token: %s\r\n", tokenString);
                tokenString[4] = tokenChar;
            }
        }
        else
        {
            // Use the user's custom token
            strncpy(tokenString, settings.pointPerfectDeviceProfileToken, sizeof(tokenString));
            systemPrintf("Using custom token: %s\r\n", tokenString);
        }

        // Build the givenName:   Name vxx.yy AABBCCDD1122
        // Get ready for JSON
        memset(givenName, 0, sizeof(givenName));
        snprintf(givenName, sizeof(givenName), "%s %s - %s", platformProvisionTable[productVariant], versionString,
                 hardwareID);

        // Verify the givenName
        if (strlen(givenName) == 0)
        {
            systemPrint("Error: Unable to build the given name!");
            break;
        }
        if (strlen(givenName) >= 50)
        {
            systemPrintf("Error: GivenName '%s' too long: %d bytes\r\n", givenName, strlen(givenName));
            break;
        }

        pointPerfectAPIPost["token"] = tokenString;
        pointPerfectAPIPost["givenName"] = givenName;
        pointPerfectAPIPost["hardwareId"] = hardwareID; // Appears as 'Sticker Ref' in ThingStream

        // const char *tag;
        // tag = "paidunit";
        // pointPerfectAPIPost["tags"][0] = tag;

        systemPrintf("Connecting to: %s\r\n", pointPerfectAPI);

        if (settings.debugCorrections == true)
        {
            systemPrintln("{");
            tokenChar = tokenString[4];
            tokenString[4] = 0;
            systemPrintf("  token: %s\r\n", tokenString);
            tokenString[4] = tokenChar;
            systemPrintf("  givenName: %s\r\n", givenName);
            systemPrintf("  hardwareId: %s\r\n", hardwareID);
            // systemPrintln("  tags: [");
            // systemPrintf("    %s\r\n", tag);
            // systemPrintln("  ]");
            systemPrintln("}");
        }

        // Using this token and hardwareID, attempt to get keys
        ZtpResponse ztpResponse = pointperfectTryZtpToken(pointPerfectAPIPost);

        if (ztpResponse == ZTP_SUCCESS)
        {
            systemPrintln("Device successfully provisioned. Keys obtained.");

            recordSystemSettings();
            retVal = true;
            break;
        }
        else if (ztpResponse == ZTP_DEACTIVATED)
        {
            char hardwareID[15];
            snprintf(hardwareID, sizeof(hardwareID), "%02X%02X%02X%02X%02X%02X%02X", btMACAddress[0], btMACAddress[1],
                     btMACAddress[2], btMACAddress[3], btMACAddress[4], btMACAddress[5], productVariant);

            char landingPageUrl[200] = "";
            if (productVariant == RTK_TORCH)
                snprintf(landingPageUrl, sizeof(landingPageUrl),
                         "or goto https://www.sparkfun.com/rtk_torch_registration ");
            else
                systemPrintln("pointperfectProvisionDevice() Platform missing landing page");

            systemPrintf("This device has been deactivated. Please contact "
                         "support@sparkfun.com %sto renew the PointPerfect "
                         "subscription. Please reference device ID: %s\r\n",
                         landingPageUrl, hardwareID);

            displayAccountExpired(5000);
            break;
        }
        else if (ztpResponse == ZTP_NOT_WHITELISTED)
        {
            char hardwareID[15];
            snprintf(hardwareID, sizeof(hardwareID), "%02X%02X%02X%02X%02X%02X%02X", btMACAddress[0], btMACAddress[1],
                     btMACAddress[2], btMACAddress[3], btMACAddress[4], btMACAddress[5], productVariant);

            char landingPageUrl[200] = "";
            if (productVariant == RTK_TORCH)
                snprintf(landingPageUrl, sizeof(landingPageUrl),
                         "or goto https://www.sparkfun.com/rtk_torch_registration ");
            else
                systemPrintln("pointperfectProvisionDevice() Platform missing landing page");

            systemPrintf("This device is not whitelisted. Please contact "
                         "support@sparkfun.com %sto get the subscription "
                         "activated. Please reference device ID: %s\r\n",
                         landingPageUrl, hardwareID);

            displayNotListed(5000);
            break;
        }
        else if (ztpResponse == ZTP_ALREADY_REGISTERED)
        {
            // Device is already registered to a different ZTP profile.
            char hardwareID[15];
            snprintf(hardwareID, sizeof(hardwareID), "%02X%02X%02X%02X%02X%02X%02X", btMACAddress[0], btMACAddress[1],
                     btMACAddress[2], btMACAddress[3], btMACAddress[4], btMACAddress[5], productVariant);

            systemPrintf("This device is registered on a different profile. Please contact "
                         "support@sparkfun.com for more assistance. Please reference device ID: %s\r\n",
                         hardwareID);
            break;
        }
        else if (ztpResponse == ZTP_RESPONSE_TIMEOUT)
        {
            // The WiFi failed to connect in a timely manner to the API.
            if (provisionAttempt < maxProvisionAttempts)
                systemPrintf("Provision server response timed out. Trying again.\r\n");
            else
                systemPrintf("Provision server response timed out. \r\n");
        }
        else
        {
            // Unknown error
            if (provisionAttempt < maxProvisionAttempts)
                systemPrintf("Unknown provisioning error. Trying again.\r\n");
            else
                systemPrintf("Unknown provisioning error.\r\n");
        }

    } while (provisionAttempt < maxProvisionAttempts);

    return (retVal);
#else  // COMPILE_WIFI
    return (false);
#endif // COMPILE_WIFI
}

// Given a token buffer and an attempt number, decide which token to use
// Decide which token to use for ZTP
// There are three lists:
//   L-Band
//   IP
//   L-Band+IP
void pointperfectGetToken(char *tokenString)
{
    // Convert uint8_t array into string with dashes in spots
    // We must assume u-blox will not change the position of their dashes or length of their token

    if (productVariant == RTK_EVK)
    {
        pointperfectCreateTokenString(tokenString, (uint8_t *)ppLbandIpToken, sizeof(ppLbandIpToken));
    }
    else if (present.gnss_mosaicX5 == false && present.lband_neo == false)
    {
        // If the hardware lacks L-Band capability, use IP token
        pointperfectCreateTokenString(tokenString, (uint8_t *)ppIpToken, sizeof(ppIpToken));
    }
    else if (present.gnss_mosaicX5 == true || present.lband_neo == true)
    {
        // If the hardware is L-Band capable, use L-Band token
        pointperfectCreateTokenString(tokenString, (uint8_t *)ppLbandToken, sizeof(ppLbandToken));
    }
    else
    {
        systemPrintln("Unknown hardware for GetToken");
        return;
    }
}

// Given a prepared JSON blob, pass to the PointPerfect API
// If it passes, keys/values are recorded to settings, ZTP_SUCCESS is returned
// If we fail, a ZTP response is returned
ZtpResponse pointperfectTryZtpToken(StaticJsonDocument<256> &apiPost)
{
#ifdef COMPILE_WIFI
    DynamicJsonDocument *jsonZtp = nullptr;
    char *tempHolderPtr = nullptr;

    WiFiClientSecure client;
    client.setCACert(AWS_PUBLIC_CERT);

    String json;
    serializeJson(apiPost, json);
    if (settings.debugPpCertificate)
        systemPrintf("Sending JSON, %d bytes\r\n", strlen(json.c_str()));

    HTTPClient http;
    http.begin(client, pointPerfectAPI);
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.POST(json);

    String response = http.getString();
    if (settings.debugPpCertificate)
        systemPrintf("Response: %d bytes\r\n", strlen(response.c_str()));
    http.end();

    ZtpResponse ztpResponse = ZTP_UNKNOWN_ERROR;

    do
    {
        if (httpResponseCode != 200)
        {
            if (settings.debugCorrections == true)
            {
                systemPrintf("HTTP response error %d: ", httpResponseCode);
                systemPrintln(response);
            }

            // "HTTP response error -11:  "
            if (httpResponseCode == -11)
            {
                if (settings.debugCorrections == true)
                    systemPrintln("API failed to respond in time.");

                ztpResponse = ZTP_RESPONSE_TIMEOUT;
                break;
            }

            // If a device has already been registered on a different ZTP profile, response will be:
            // "HTTP response error 403: Device already registered"
            else if (response.indexOf("Device already registered") >= 0)
            {
                if (settings.debugCorrections == true)
                    systemPrintln("Device already registered to different profile. Trying next profile.");

                ztpResponse = ZTP_ALREADY_REGISTERED;
                break;
            }
            // If a device has been deactivated, response will be: "HTTP response error 403: No plan for device
            // device:9f19e97f-e6a7-4808-8d58-ac7ecac90e23"
            else if (response.indexOf("No plan for device") >= 0)
            {
                ztpResponse = ZTP_DEACTIVATED;
                break;
            }
            // If a device is not whitelisted, response will be: "HTTP response error 403: Device hardware code not
            // whitelisted"
            else if (response.indexOf("not whitelisted") >= 0)
            {
                ztpResponse = ZTP_NOT_WHITELISTED;
                break;
            }
            else
            {
                systemPrintf("HTTP response error %d: ", httpResponseCode);
                systemPrintln(response);
                ztpResponse = ZTP_UNKNOWN_ERROR;
                break;
            }
        }
        else
        {
            // Device is now active with ThingStream
            // Pull pertinent values from response
            jsonZtp = new DynamicJsonDocument(8192);
            if (!jsonZtp)
            {
                systemPrintln("ERROR - Failed to allocate jsonZtp!\r\n");
                break;
            }

            DeserializationError error = deserializeJson(*jsonZtp, response);
            if (DeserializationError::Ok != error)
            {
                systemPrintln("JSON error");
                break;
            }
            else
            {
                if (online.psram == true)
                    tempHolderPtr = (char *)ps_malloc(MQTT_CERT_SIZE);
                else
                    tempHolderPtr = (char *)malloc(MQTT_CERT_SIZE);

                if (!tempHolderPtr)
                {
                    systemPrintln("ERROR - Failed to allocate tempHolderPtr buffer!\r\n");
                    break;
                }
                strncpy(tempHolderPtr, (const char *)((*jsonZtp)["certificate"]), MQTT_CERT_SIZE - 1);
                recordFile("certificate", tempHolderPtr, strlen(tempHolderPtr));

                strncpy(tempHolderPtr, (const char *)((*jsonZtp)["privateKey"]), MQTT_CERT_SIZE - 1);
                recordFile("privateKey", tempHolderPtr, strlen(tempHolderPtr));

                // Validate the keys
                if (!checkCertificates())
                {
                    systemPrintln("ERROR - Failed to validate the Point Perfect certificates!");
                }
                else
                {
                    if (settings.debugPpCertificate)
                        systemPrintln("Certificates recorded successfully.");

                    strncpy(settings.pointPerfectClientID, (const char *)((*jsonZtp)["clientId"]),
                            sizeof(settings.pointPerfectClientID));
                    strncpy(settings.pointPerfectBrokerHost, (const char *)((*jsonZtp)["brokerHost"]),
                            sizeof(settings.pointPerfectBrokerHost));

                    // Note: from the ZTP documentation:
                    // ["subscriptions"][0] will contain the key distribution topic
                    // But, assuming the key distribution topic is always ["subscriptions"][0] is potentially brittle
                    // It is safer to check the "description" contains "key distribution topic"
                    // If we are on an IP-only plan, the path will be /pp/ubx/0236/ip
                    // If we are on a L-Band-only or L-Band+IP plan, the path will be /pp/ubx/0236/Lb
                    // These 0236 key distribution topics provide the keys in UBX format, ready to be pushed to a ZED.
                    // There are also /pp/key/ip and /pp/key/Lb topics which provide the keys in JSON format - but we
                    // don't use those.
                    int subscription =
                        findZtpJSONEntry("subscriptions", "description", "key distribution topic", jsonZtp);
                    if (subscription >= 0)
                        strncpy(settings.pointPerfectKeyDistributionTopic,
                                (const char *)((*jsonZtp)["subscriptions"][subscription]["path"]),
                                sizeof(settings.pointPerfectKeyDistributionTopic));

                    // "subscriptions" will also contain the correction topics for all available regional areas - for
                    // IP-only or L-Band+IP We should store those too, and then allow the user to select the one for
                    // their regional area
                    for (int r = 0; r < numRegionalAreas; r++)
                    {
                        char findMe[40];
                        snprintf(findMe, sizeof(findMe), "correction topic for %s",
                                 Regional_Information_Table[r].name); // Search for "US" etc.
                        subscription = findZtpJSONEntry("subscriptions", "description", (const char *)findMe, jsonZtp);
                        if (subscription >= 0)
                            strncpy(settings.regionalCorrectionTopics[r],
                                    (const char *)((*jsonZtp)["subscriptions"][subscription]["path"]),
                                    sizeof(settings.regionalCorrectionTopics[0]));
                        else
                            settings.regionalCorrectionTopics[r][0] =
                                0; // Erase any invalid (non-plan) correction topics. Just in case the plan has changed.
                    }

                    // "subscriptions" also contains the geographic area definition topic for each region for localized
                    // distribution. We can cheat by appending "/gad" to the correction topic. TODO: think about doing
                    // this properly.

                    // Now we extract the current and next key pair
                    strncpy(settings.pointPerfectCurrentKey,
                            (const char *)((*jsonZtp)["dynamickeys"]["current"]["value"]),
                            sizeof(settings.pointPerfectCurrentKey));
                    settings.pointPerfectCurrentKeyDuration = (*jsonZtp)["dynamickeys"]["current"]["duration"];
                    settings.pointPerfectCurrentKeyStart = (*jsonZtp)["dynamickeys"]["current"]["start"];

                    strncpy(settings.pointPerfectNextKey, (const char *)((*jsonZtp)["dynamickeys"]["next"]["value"]),
                            sizeof(settings.pointPerfectNextKey));
                    settings.pointPerfectNextKeyDuration = (*jsonZtp)["dynamickeys"]["next"]["duration"];
                    settings.pointPerfectNextKeyStart = (*jsonZtp)["dynamickeys"]["next"]["start"];

                    if (settings.debugCorrections == true)
                        pointperfectPrintKeyInformation();

                    ztpResponse = ZTP_SUCCESS;
                }
            } // JSON Derialized correctly
        } // HTTP Response was 200
    } while (0);

    // Free the allocated buffers
    if (jsonZtp)
        delete jsonZtp;
    if (tempHolderPtr)
        free(tempHolderPtr);

    return (ztpResponse);

#else  // COMPILE_WIFI
    return (ZTP_UNKNOWN_ERROR);
#endif // COMPILE_WIFI
}

// Find thing3 in (*jsonZtp)[thing1][n][thing2]. Return n on success. Return -1 on error / not found.
int findZtpJSONEntry(const char *thing1, const char *thing2, const char *thing3, DynamicJsonDocument *jsonZtp)
{
    if (!jsonZtp)
        return (-1);

    int i = (*jsonZtp)[thing1].size();

    if (i == 0)
        return (-1);

    for (int j = 0; j < i; j++)
        if (strstr((const char *)(*jsonZtp)[thing1][j][thing2], thing3) != nullptr)
        {
            return j;
        }

    return (-1);
}

// Given a token array, format it in the proper way and store it in the buffer
void pointperfectCreateTokenString(char *tokenBuffer, uint8_t *tokenArray, int tokenArrayLength)
{
    tokenBuffer[0] = '\0'; // Clear anything in the buffer

    for (int x = 0; x < tokenArrayLength; x++)
    {
        char temp[3];
        snprintf(temp, sizeof(temp), "%02x", tokenArray[x]);
        strcat(tokenBuffer, temp);
        if (x == 3 || x == 5 || x == 7 || x == 9)
            strcat(tokenBuffer, "-");
    }
}

// Check certificate and privatekey for valid formatting
// Return false if improperly formatted
bool checkCertificates()
{
    bool validCertificates = true;
    char *certificateContents = nullptr; // Holds the contents of the keys prior to MQTT connection
    char *keyContents = nullptr;

    // Allocate the buffers
    if (online.psram == true)
    {
        certificateContents = (char *)ps_malloc(MQTT_CERT_SIZE);
        keyContents = (char *)ps_malloc(MQTT_CERT_SIZE);
    }
    else
    {
        certificateContents = (char *)malloc(MQTT_CERT_SIZE);
        keyContents = (char *)malloc(MQTT_CERT_SIZE);
    }

    if ((!certificateContents) || (!keyContents))
    {
        if (certificateContents)
            free(certificateContents);
        if (keyContents)
            free(keyContents);
        systemPrintln("Failed to allocate content buffers!");
        return (false);
    }

    // Load the certificate
    memset(certificateContents, 0, MQTT_CERT_SIZE);
    loadFile("certificate", certificateContents, settings.debugPpCertificate);

    if (checkCertificateValidity(certificateContents, strlen(certificateContents)) == false)
    {
        if (settings.debugPpCertificate)
            systemPrintln("Certificate is corrupt.");
        validCertificates = false;
    }

    // Load the private key
    memset(keyContents, 0, MQTT_CERT_SIZE);
    loadFile("privateKey", keyContents, settings.debugPpCertificate);

    if (checkPrivateKeyValidity(keyContents, strlen(keyContents)) == false)
    {
        if (settings.debugPpCertificate)
            systemPrintln("PrivateKey is corrupt.");
        validCertificates = false;
    }

    // Free the content buffers
    if (certificateContents)
        free(certificateContents);
    if (keyContents)
        free(keyContents);

    if (settings.debugPpCertificate)
        systemPrintln("Stored certificates are valid!");
    return (validCertificates);
}

// Check if a given certificate is in a valid format
// This was created to detect corrupt or invalid certificates caused by bugs in v3.0 to and including v3.3.
bool checkCertificateValidity(char *certificateContent, int certificateContentSize)
{
    // Check for valid format of certificate
    // From ssl_client.cpp
    // https://stackoverflow.com/questions/70670070/mbedtls-cannot-parse-valid-x509-certificate
    mbedtls_x509_crt certificate;
    mbedtls_x509_crt_init(&certificate);

    int result_code =
        mbedtls_x509_crt_parse(&certificate, (unsigned char *)certificateContent, certificateContentSize + 1);

    mbedtls_x509_crt_free(&certificate);

    if (result_code < 0)
    {
        if (settings.debugPpCertificate)
            systemPrintln("ERROR - Invalid certificate format!");
        return (false);
    }

    return (true);
}

// Check if a given private key is in a valid format
// This was created to detect corrupt or invalid private keys caused by bugs in v3.0 to and including v3.3.
// See https://github.com/Mbed-TLS/mbedtls/blob/development/library/pkparse.c
bool checkPrivateKeyValidity(char *privateKey, int privateKeySize)
{
    // Check for valid format of private key
    // From ssl_client.cpp
    // https://stackoverflow.com/questions/70670070/mbedtls-cannot-parse-valid-x509-certificate
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);

    int result_code = mbedtls_pk_parse_key(&pk, (unsigned char *)privateKey, privateKeySize + 1, nullptr, 0);
    mbedtls_pk_free(&pk);
    if (result_code < 0)
    {
        if (settings.debugPpCertificate)
            systemPrintln("ERROR - Invalid private key format!");
        return (false);
    }
    return (true);
}

// When called, removes the files used for SSL to PointPerfect obtained during provisioning
// Also deletes keys so the user can immediately re-provision
void erasePointperfectCredentials()
{
    char fileName[80];

    snprintf(fileName, sizeof(fileName), "/%s_%s_%d.txt", platformFilePrefix, "certificate", profileNumber);
    LittleFS.remove(fileName);

    snprintf(fileName, sizeof(fileName), "/%s_%s_%d.txt", platformFilePrefix, "privateKey", profileNumber);
    LittleFS.remove(fileName);
    strcpy(settings.pointPerfectCurrentKey, ""); // Clear contents
    strcpy(settings.pointPerfectNextKey, "");    // Clear contents
}

// Subscribe to MQTT channel, grab keys, then stop
bool pointperfectUpdateKeys()
{
#ifdef COMPILE_WIFI
    char *certificateContents = nullptr; // Holds the contents of the keys prior to MQTT connection
    char *keyContents = nullptr;
    WiFiClientSecure secureClient;
    bool gotKeys = false;
    menuppMqttClient = nullptr;

    do
    {
        // Allocate the buffers
        if (online.psram == true)
        {
            certificateContents = (char *)ps_malloc(MQTT_CERT_SIZE);
            keyContents = (char *)ps_malloc(MQTT_CERT_SIZE);
        }
        else
        {
            certificateContents = (char *)malloc(MQTT_CERT_SIZE);
            keyContents = (char *)malloc(MQTT_CERT_SIZE);
        }

        if ((!certificateContents) || (!keyContents))
        {
            if (certificateContents)
                free(certificateContents);
            systemPrintln("Failed to allocate content buffers!");
            break;
        }

        // Get the certificate
        memset(certificateContents, 0, MQTT_CERT_SIZE);
        loadFile("certificate", certificateContents, false);
        secureClient.setCertificate(certificateContents);

        // Get the private key
        memset(keyContents, 0, MQTT_CERT_SIZE);
        loadFile("privateKey", keyContents, false);
        secureClient.setPrivateKey(keyContents);

        secureClient.setCACert(AWS_PUBLIC_CERT);

        // Allocate the MQTT client
        menuppMqttClient = new MqttClient(secureClient);
        if (!menuppMqttClient)
        {
            systemPrintln("Failed to allocate the MQTT client structure!");
            break;
        }

        // Configure the MQTT client
        menuppMqttClient->setId(settings.pointPerfectClientID);
        menuppMqttClient->onMessage(mqttCallback);

        // Attempt to the MQTT broker
        systemPrintf("Attempting to connect to MQTT broker: %s\r\n", settings.pointPerfectBrokerHost);
        if (!menuppMqttClient->connect(settings.pointPerfectBrokerHost, 8883))
        {
            systemPrintln("Failed to connect to MQTT Broker");

            // MQTT does not provide good error reporting.
            // Throw out everything and attempt to provision the device to get better error checking.
            pointperfectProvisionDevice();
            break; // Skip the remaining MQTT checking, release resources
        }

        // Successful connection
        systemPrintln("MQTT connected");

        // pointPerfectKeyDistributionTopic is /pp/ubx/0236/ip or /pp/ubx/0236/Lb.
        // It is provided during ZTP provisioning.
        // The topic contains the keys in UBX format, ready to be pushed to a ZED.
        // These need to be unpicked into JSON format and stored in settings - by mqttCallback below.
        mqttMessageReceived = false;
        if (!menuppMqttClient->subscribe(settings.pointPerfectKeyDistributionTopic))
        {
            systemPrintf("Failed to subscribe to %s!\r\n", settings.pointPerfectKeyDistributionTopic);
            pointperfectProvisionDevice();
            break;
        }

        systemPrint("Waiting for keys");

        // Wait for callback
        startTime = millis();
        while (1)
        {
            menuppMqttClient->poll();
            if (mqttMessageReceived == true)
                break;
            if (menuppMqttClient->connected() == false)
            {
                if (settings.debugCorrections == true)
                    systemPrintln("Client disconnected");
                break;
            }
            if (millis() - startTime > 8000)
            {
                if (settings.debugCorrections == true)
                    systemPrintln("Channel failed to respond");
                break;
            }

            // Continue waiting for the keys
            delay(100);
            systemPrint(".");
        }
        systemPrintln();

        // Determine if the keys were updated
        if (mqttMessageReceived)
        {
            systemPrintln("Keys successfully updated");
            gotKeys = true;
        }

        // Done with the MQTT client
        menuppMqttClient->unsubscribe(settings.pointPerfectKeyDistributionTopic);
    } while (0);

    // Stop and delete the MQTT client
    if (menuppMqttClient)
    {
        menuppMqttClient->stop();
        delete menuppMqttClient;
        menuppMqttClient = nullptr;
    }

    // Free the content buffers
    if (keyContents)
        free(keyContents);
    if (certificateContents)
        free(certificateContents);

    // Return the key status
    return (gotKeys);
#else  // COMPILE_WIFI
    return (false);
#endif // COMPILE_WIFI
}

// Called when a subscribed to message arrives
void mqttCallback(int messageSize)
{
#ifdef COMPILE_WIFI
    static uint32_t messageLength;
    static byte *message;

    do
    {
        // Determine if this is a message that should be processed
        if (menuppMqttClient->messageTopic() != settings.pointPerfectKeyDistributionTopic)
            break;

        // Allocate the message buffer
        if (messageLength < messageSize)
        {
            // Free the previous message buffer
            if (message)
            {
                free(message);
                message = nullptr;
            }

            // Allocate the new message buffer
            messageLength = (messageSize + 512) & (~511);
            message = (byte *)malloc(messageLength);
            if (!message)
            {
                messageLength = 0;
                systemPrintln("Failed to allocate MQTT message buffer!");
                break;
            }
        }

        // Get the message data
        menuppMqttClient->read(message, messageSize);
        if (settings.debugCorrections)
            systemPrintf("\r\nReceived %d bytes\r\n", messageSize);

        // Separate the UBX message into its constituent Key/ToW/Week parts
        // Obtained from SparkFun u-blox Arduino library - setDynamicSPARTNKeys()
        byte *payLoad = &message[6];
        uint8_t currentKeyLength = payLoad[5];
        uint16_t currentWeek = (payLoad[7] << 8) | payLoad[6];
        uint32_t currentToW =
            (payLoad[11] << 8 * 3) | (payLoad[10] << 8 * 2) | (payLoad[9] << 8 * 1) | (payLoad[8] << 8 * 0);

        char currentKey[currentKeyLength];
        memcpy(&currentKey, &payLoad[20], currentKeyLength);

        uint8_t nextKeyLength = payLoad[13];
        uint16_t nextWeek = (payLoad[15] << 8) | payLoad[14];
        uint32_t nextToW =
            (payLoad[19] << 8 * 3) | (payLoad[18] << 8 * 2) | (payLoad[17] << 8 * 1) | (payLoad[16] << 8 * 0);

        char nextKey[nextKeyLength];
        memcpy(&nextKey, &payLoad[20 + currentKeyLength], nextKeyLength);

        // Convert byte array to HEX character array
        strcpy(settings.pointPerfectCurrentKey, ""); // Clear contents
        strcpy(settings.pointPerfectNextKey, "");    // Clear contents
        for (int x = 0; x < 16; x++)                 // Force length to max of 32 bytes
        {
            char temp[3];
            snprintf(temp, sizeof(temp), "%02X", currentKey[x]);
            strcat(settings.pointPerfectCurrentKey, temp);

            snprintf(temp, sizeof(temp), "%02X", nextKey[x]);
            strcat(settings.pointPerfectNextKey, temp);
        }

        // Convert from ToW and Week to key duration and key start
        WeekToWToUnixEpoch(&settings.pointPerfectCurrentKeyStart, currentWeek, currentToW);
        WeekToWToUnixEpoch(&settings.pointPerfectNextKeyStart, nextWeek, nextToW);

        settings.pointPerfectCurrentKeyStart -= gnssGetLeapSeconds(); // Remove GPS leap seconds to align with u-blox
        settings.pointPerfectNextKeyStart -= gnssGetLeapSeconds();

        settings.pointPerfectCurrentKeyStart *= 1000; // Convert to ms
        settings.pointPerfectNextKeyStart *= 1000;

        settings.pointPerfectCurrentKeyDuration =
            settings.pointPerfectNextKeyStart - settings.pointPerfectCurrentKeyStart - 1;
        // settings.pointPerfectNextKeyDuration =
        //     settings.pointPerfectCurrentKeyDuration; // We assume next key duration is the same as current key
        //     duration because we have to

        settings.pointPerfectNextKeyDuration = (1000LL * 60 * 60 * 24 * 28) - 1; // Assume next key duration is 28 days

        if (settings.debugCorrections == true)
            pointperfectPrintKeyInformation();

        mqttMessageReceived = true;
    } while (0);
#endif // COMPILE_WIFI
}

// Get a date from a user
// Return true if validated
// https://www.includehelp.com/c-programs/validate-date.aspx
bool getDate(uint8_t &dd, uint8_t &mm, uint16_t &yy)
{
    systemPrint("Enter Day: ");
    dd = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

    systemPrint("Enter Month: ");
    mm = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

    systemPrint("Enter Year (YYYY): ");
    yy = getUserInputNumber(); // Returns EXIT, TIMEOUT, or long

    // check year
    if (yy >= 2022 && yy <= 9999)
    {
        // check month
        if (mm >= 1 && mm <= 12)
        {
            // check days
            if ((dd >= 1 && dd <= 31) && (mm == 1 || mm == 3 || mm == 5 || mm == 7 || mm == 8 || mm == 10 || mm == 12))
                return (true);
            else if ((dd >= 1 && dd <= 30) && (mm == 4 || mm == 6 || mm == 9 || mm == 11))
                return (true);
            else if ((dd >= 1 && dd <= 28) && (mm == 2))
                return (true);
            else if (dd == 29 && mm == 2 && (yy % 400 == 0 || (yy % 4 == 0 && yy % 100 != 0)))
                return (true);
            else
            {
                printf("Day is invalid.\n");
                return (false);
            }
        }
        else
        {
            printf("Month is not valid.\n");
            return (false);
        }
    }

    printf("Year is not valid.\n");
    return (false);
}

// Given an epoch in ms, return the number of days from given Epoch and now
int daysFromEpoch(long long endEpoch)
{
    long delta = secondsFromEpoch(endEpoch); // number of s between dates

    if (delta == -1)
        return (-1);

    delta /= (60 * 60); // hours

    delta /= 24; // days
    return ((int)delta);
}

// Given an epoch in ms, return the number of seconds from given Epoch and now
long secondsFromEpoch(long long endEpoch)
{
    if (online.rtc == false)
    {
        // If we don't have RTC we can't calculate days to expire
        if (settings.debugCorrections == true)
            systemPrintln("No RTC available");
        return (-1);
    }

    endEpoch /= 1000; // Convert PointPerfect ms Epoch to s

    long currentEpoch = rtc.getEpoch();

    long delta = endEpoch - currentEpoch; // number of s between dates
    return (delta);
}

// Given the key's starting epoch time, and the key's duration
// Convert from ms to s
// Add leap seconds (the API reports start times with GPS leap seconds removed)
// Convert from unix epoch (the API reports unix epoch time) to GPS epoch (the NED-D9S expects)
// Note: I believe the Thingstream API is reporting startEpoch 18 seconds less than actual
long long thingstreamEpochToGPSEpoch(long long startEpoch)
{
    long long epoch = startEpoch;
    epoch /= 1000; // Convert PointPerfect ms Epoch to s

    // Convert Unix Epoch time from PointPerfect to GPS Time Of Week needed for UBX message
    long long gpsEpoch = epoch - 315964800 + gnssGetLeapSeconds(); // Shift to GPS Epoch.
    return (gpsEpoch);
}

// Covert a given key's expiration date to a GPS Epoch, so that we can calculate GPS Week and ToW
// Add a millisecond to roll over from 11:59UTC to midnight of the following day
// Convert from unix epoch (time lib outputs unix) to GPS epoch (the NED-D9S expects)
long long dateToGPSEpoch(uint8_t day, uint8_t month, uint16_t year)
{
    long long unixEpoch = dateToUnixEpoch(day, month, year); // Returns Unix Epoch

    // Convert Unix Epoch time from PP to GPS Time Of Week needed for UBX message
    long long gpsEpoch = unixEpoch - 315964800; // Shift to GPS Epoch.

    return (gpsEpoch);
}

// Given an epoch, set the GPSWeek and GPSToW
void epochToWeekToW(long long epoch, uint16_t *GPSWeek, uint32_t *GPSToW)
{
    *GPSWeek = (uint16_t)(epoch / (7 * 24 * 60 * 60));
    *GPSToW = (uint32_t)(epoch % (7 * 24 * 60 * 60));
}

// Given a GPSWeek and GPSToW, set the epoch
void WeekToWToUnixEpoch(uint64_t *unixEpoch, uint16_t GPSWeek, uint32_t GPSToW)
{
    *unixEpoch = GPSWeek * (7 * 24 * 60 * 60); // 2192
    *unixEpoch += GPSToW;                      // 518400
    *unixEpoch += 315964800;
}

// Given a GPS Week and ToW, convert to an expiration date
void gpsWeekToWToDate(uint16_t keyGPSWeek, uint32_t keyGPSToW, long *expDay, long *expMonth, long *expYear)
{
    long gpsDays = gpsToMjd(0, (long)keyGPSWeek, (long)keyGPSToW); // Covert ToW and Week to # of days since Jan 6, 1980
    mjdToDate(gpsDays, expYear, expMonth, expDay);
}

// Given a date, convert into epoch
// https://www.epochconverter.com/programming/c
long dateToUnixEpoch(uint8_t day, uint8_t month, uint16_t year)
{
    struct tm t;
    time_t t_of_day;

    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;

    t.tm_hour = 0;
    t.tm_min = 0;
    t.tm_sec = 0;
    t.tm_isdst = -1; // Is DST on? 1 = yes, 0 = no, -1 = unknown

    t_of_day = mktime(&t);

    return (t_of_day);
}

// Given a date, calculate and return the key start in unixEpoch
void dateToKeyStart(uint8_t expDay, uint8_t expMonth, uint16_t expYear, uint64_t *settingsKeyStart)
{
    long long expireUnixEpoch = dateToUnixEpoch(expDay, expMonth, expYear);

    // Thingstream lists the date that a key expires at midnight
    // So if a user types in March 7th, 2022 as exp date the key's Week and ToW need to be
    // calculated from (March 7th - 27 days).
    long long startUnixEpoch = expireUnixEpoch - (27 * 24 * 60 * 60); // Move back 27 days

    // Additionally, Thingstream seems to be reporting Epochs that do not have leap seconds
    startUnixEpoch -= gnssGetLeapSeconds(); // Modify our Epoch to match Point Perfect

    // PointPerfect uses/reports unix epochs in milliseconds
    *settingsKeyStart = startUnixEpoch * 1000L; // Convert to ms

    uint16_t keyGPSWeek;
    uint32_t keyGPSToW;
    long long gpsEpoch = thingstreamEpochToGPSEpoch(*settingsKeyStart);

    epochToWeekToW(gpsEpoch, &keyGPSWeek, &keyGPSToW);

    // Print ToW and Week for debugging
    if (settings.debugCorrections == true)
    {
        systemPrintf("  expireUnixEpoch: %lld - %s\r\n", expireUnixEpoch, printDateFromUnixEpoch(expireUnixEpoch));
        systemPrintf("  startUnixEpoch: %lld - %s\r\n", startUnixEpoch, printDateFromUnixEpoch(startUnixEpoch));
        systemPrintf("  gpsEpoch: %lld - %s\r\n", gpsEpoch, printDateFromGPSEpoch(gpsEpoch));
        systemPrintf("  KeyStart: %lld - %s\r\n", *settingsKeyStart, printDateFromUnixEpoch(*settingsKeyStart));
        systemPrintf("  keyGPSWeek: %d\r\n", keyGPSWeek);
        systemPrintf("  keyGPSToW: %d\r\n", keyGPSToW);
    }
}

/*
   http://www.leapsecond.com/tools/gpsdate.c
   Return Modified Julian Day given calendar year,
   month (1-12), and day (1-31).
   - Valid for Gregorian dates from 17-Nov-1858.
   - Adapted from sci.astro FAQ.
*/
long dateToMjd(long Year, long Month, long Day)
{
    return 367 * Year - 7 * (Year + (Month + 9) / 12) / 4 - 3 * ((Year + (Month - 9) / 7) / 100 + 1) / 4 +
           275 * Month / 9 + Day + 1721028 - 2400000;
}

/*
   Convert Modified Julian Day to calendar date.
   - Assumes Gregorian calendar.
   - Adapted from Fliegel/van Flandern ACM 11/#10 p 657 Oct 1968.
*/
void mjdToDate(long Mjd, long *Year, long *Month, long *Day)
{
    long J, C, Y, M;

    J = Mjd + 2400001 + 68569;
    C = 4 * J / 146097;
    J = J - (146097 * C + 3) / 4;
    Y = 4000 * (J + 1) / 1461001;
    J = J - 1461 * Y / 4 + 31;
    M = 80 * J / 2447;
    *Day = J - 2447 * M / 80;
    J = M / 11;
    *Month = M + 2 - (12 * J);
    *Year = 100 * (C - 49) + Y + J;
}

/*
   Convert GPS Week and Seconds to Modified Julian Day.
   - Ignores UTC leap seconds.
*/
long gpsToMjd(long GpsCycle, long GpsWeek, long GpsSeconds)
{
    long GpsDays = ((GpsCycle * 1024) + GpsWeek) * 7 + (GpsSeconds / 86400);
    // GpsDays -= 1; //Correction
    return dateToMjd(1980, 1, 6) + GpsDays;
}

// When new PMP message arrives from NEO-D9S push it back to ZED-F9P
void pushRXMPMP(UBX_RXM_PMP_message_data_t *pmpData)
{
    uint16_t payloadLen = ((uint16_t)pmpData->lengthMSB << 8) | (uint16_t)pmpData->lengthLSB;

    updateCorrectionsLastSeen(CORR_LBAND); // This will (re)register the correction source if needed

    if (isHighestRegisteredCorrectionsSource(CORR_LBAND))
    {
        updateZEDCorrectionsSource(1); // Set SOURCE to 1 (L-Band) if needed

        if (settings.debugCorrections == true && !inMainMenu)
            systemPrintf("Pushing %d bytes of RXM-PMP data to GNSS\r\n", payloadLen);

        gnssPushRawData(&pmpData->sync1, (size_t)payloadLen + 6); // Push the sync chars, class, ID, length and payload
        gnssPushRawData(&pmpData->checksumA, (size_t)2);          // Push the checksum bytes
    }
    else
    {
        if (settings.debugCorrections == true && !inMainMenu)
            systemPrintf("NOT pushing %d bytes of RXM-PMP data to GNSS due to priority\r\n", payloadLen);
    }
}

// Check if the PMP data is being decrypted successfully
void checkRXMCOR(UBX_RXM_COR_data_t *ubxDataStruct)
{
    if (settings.debugCorrections == true && !inMainMenu && zedCorrectionsSource == 1) // Only print for L-Band
        systemPrintf("L-Band Eb/N0[dB] (>9 is good): %0.2f\r\n", ubxDataStruct->ebno * pow(2, -3));

    lBandEBNO = ubxDataStruct->ebno * pow(2, -3);

    if (ubxDataStruct->statusInfo.bits.msgDecrypted == 2) // Successfully decrypted
    {
        lbandCorrectionsReceived = true;
        lastLBandDecryption = millis();
    }
    else
    {
        if (settings.debugCorrections == true && !inMainMenu)
            systemPrintln("PMP decryption failed");
    }
}

//----------------------------------------
// Global L-Band Routines
//----------------------------------------

// Check if NEO-D9S is connected. Configure if available.
void beginLBand()
{
    // Skip if going into configure-via-ethernet mode
    if (configureViaEthernet)
    {
        if (settings.debugCorrections == true)
            systemPrintln("configureViaEthernet: skipping beginLBand");
        return;
    }

#ifdef COMPILE_L_BAND
    if (present.lband_neo == false)
        return;

    if (i2cLBand.begin(*i2c_0, 0x43) ==
        false) // Connect to the u-blox NEO-D9S using Wire port. The D9S default I2C address is 0x43 (not 0x42)
    {
        if (settings.debugCorrections == true)
            systemPrintln("L-Band not detected");
        return;
    }

    // Check the firmware version of the NEO-D9S. Based on Example21_ModuleInfo.
    if (i2cLBand.getModuleInfo(1100) == true) // Try to get the module info
    {
        // Reconstruct the firmware version
        snprintf(neoFirmwareVersion, sizeof(neoFirmwareVersion), "%s %d.%02d", i2cLBand.getFirmwareType(),
                 i2cLBand.getFirmwareVersionHigh(), i2cLBand.getFirmwareVersionLow());

        printNEOInfo(); // Print module firmware version
    }

    gnssUpdate();

    // Previously the L-Band frequency was set here based on gnssGetLongitude and gnssGetLatitude
    // if gnssIsFixed was true. beginLBand is called early during setup and I worry that the
    // GNSS may not always be fixed... I think it is far safer to set the frequency based on the
    // selected geographical region...

    uint32_t lBandFreq = Regional_Information_Table[settings.geographicRegion].frequency;
    if (lBandFreq > 0)
    {
        if (settings.debugCorrections == true)
            systemPrintf("L-Band frequency (Hz): %d\r\n", lBandFreq);
    }
    else
    {
        lBandFreq = Regional_Information_Table[0].frequency;
        if (settings.debugCorrections == true)
            systemPrintf("Geographic region has no L-Band frequency. Defaulting to (Hz): %d\r\n", lBandFreq);
    }

    bool response = true;
    response &= i2cLBand.newCfgValset();
    response &= i2cLBand.addCfgValset(UBLOX_CFG_PMP_CENTER_FREQUENCY, lBandFreq); // Default 1539812500 Hz
    response &= i2cLBand.addCfgValset(UBLOX_CFG_PMP_SEARCH_WINDOW, 2200);         // Default 2200 Hz
    response &= i2cLBand.addCfgValset(UBLOX_CFG_PMP_USE_SERVICE_ID, 0);           // Default 1
    response &= i2cLBand.addCfgValset(UBLOX_CFG_PMP_SERVICE_ID, 21845);           // Default 50821
    response &= i2cLBand.addCfgValset(UBLOX_CFG_PMP_DATA_RATE, 2400);             // Default 2400 bps
    response &= i2cLBand.addCfgValset(UBLOX_CFG_PMP_USE_DESCRAMBLER, 1);          // Default 1
    response &= i2cLBand.addCfgValset(UBLOX_CFG_PMP_DESCRAMBLER_INIT, 26969);     // Default 23560
    response &= i2cLBand.addCfgValset(UBLOX_CFG_PMP_USE_PRESCRAMBLING, 0);        // Default 0
    response &= i2cLBand.addCfgValset(UBLOX_CFG_PMP_UNIQUE_WORD, 16238547128276412563ull);
    response &=
        i2cLBand.addCfgValset(UBLOX_CFG_MSGOUT_UBX_RXM_PMP_UART1, 0); // Diasable UBX-RXM-PMP on UART1. Not used.

    response &= i2cLBand.sendCfgValset();

    response &= zedEnableLBandCommunication();

    if (response == false)
        systemPrintln("L-Band failed to configure");

    i2cLBand.softwareResetGNSSOnly(); // Do a restart

    if (settings.debugCorrections == true)
        systemPrintln("L-Band online");

    online.lband = true;
#endif // COMPILE_L_BAND
}

// Set 'home' WiFi credentials
// Provision device on ThingStream
// Download keys
void menuPointPerfect()
{
    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: PointPerfect Corrections");

        if (settings.debugCorrections == true)
            systemPrintf("Time to first RTK Fix: %ds Restarts: %d\r\n", rtkTimeToFixMs / 1000, floatLockRestarts);

        if (settings.debugCorrections == true)
            systemPrintf("settings.pointPerfectKeyDistributionTopic: %s\r\n",
                         settings.pointPerfectKeyDistributionTopic);

        systemPrint("Days until keys expire: ");
        if (strlen(settings.pointPerfectCurrentKey) > 0)
        {
            if (online.rtc == false)
            {
                // If we don't have RTC we can't calculate days to expire
                systemPrintln("No RTC");
            }
            else
            {
                int daysRemaining =
                    daysFromEpoch(settings.pointPerfectNextKeyStart + settings.pointPerfectNextKeyDuration + 1);

                if (daysRemaining < 0)
                    systemPrintln("Expired");
                else
                    systemPrintln(daysRemaining);
            }
        }
        else
            systemPrintln("No keys");

        // How this works:
        //   There are three PointPerfect corrections plans: IP-only, L-Band-only, L-Band+IP
        //   For IP-only - e.g. Torch:
        //     During ZTP Provisioning, we receive the UBX-format key distribution topic /pp/ubx/0236/ip
        //     We also receive the full list of regional correction topics: /pp/ip/us , /pp/ip/eu , etc.
        //     We need to subscribe to our regional correction topic and push the data to the PPL
        //     RTCM from the PPL is pushed to the UM980
        //   For L-Band-only - e.g. Facet mosaic or Facet v2 L-Band
        //     During ZTP Provisioning, we receive the UBX-format key distribution topic /pp/ubx/0236/Lb
        //     There are no regional correction topics for L-Band-only
        //     Facet v2 L-Band pushes the keys to the ZED and pushes PMP from the NEO to the ZED
        //     Facet mosaic pushes the current key and raw L-Band to the PPL, then pushes RTCM to the X5
        //   For L-Band+IP - e.g. EVK:
        //     During ZTP Provisioning, we receive the UBX-format key distribution topic /pp/ubx/0236/Lb
        //     We also receive the full list of regional correction topics: /pp/Lb/us , /pp/Lb/eu , etc.
        //     We can subscribe to the topic and push IP data to the ZED - using UBLOX_CFG_SPARTN_USE_SOURCE 0
        //     Or we can push PMP data from the NEO to the ZED - using UBLOX_CFG_SPARTN_USE_SOURCE 1
        //   We do not need the user to tell us which pointPerfectCorrectionsSource to use.
        //   We can figure it out from the key distribution topic:
        //     IP-only gets /pp/ubx/0236/ip.
        //     L-Band-only and L-Band+IP get /pp/ubx/0236/Lb.
        //   And from the regional correction topics:
        //     IP-only gets /pp/ip/us , /pp/ip/eu , etc.
        //     L-Band-only gets none
        //     L-Band+IP gets /pp/Lb/us , /pp/Lb/eu , etc.

        systemPrint("1) PointPerfect Corrections: ");
        if (settings.enablePointPerfectCorrections)
            systemPrintln("Enabled");
        else
            systemPrintln("Disabled");

        if (pointPerfectIsEnabled())
        {
            systemPrint("2) Toggle Auto Key Renewal: ");
            if (settings.autoKeyRenewal == true)
                systemPrintln("Enabled");
            else
                systemPrintln("Disabled");

            if (strlen(settings.pointPerfectCurrentKey) == 0 || strlen(settings.pointPerfectKeyDistributionTopic) == 0)
                systemPrintln("3) Provision Device");
            else
                systemPrintln("3) Update Keys");

            systemPrintln("4) Show device ID");

            systemPrintln("c) Clear the Keys");

            systemPrintln("k) Manual Key Entry");
        }

        systemPrint("g) Geographic Region: ");
        systemPrintln(Regional_Information_Table[settings.geographicRegion].name);

        systemPrintln("x) Exit");

        byte incoming = getUserInputCharacterNumber();

        if (incoming == 1)
        {
            settings.enablePointPerfectCorrections ^= 1;
            restartRover = true; // Require a rover restart to enable / disable RTCM for PPL
        }

        else if (incoming == 2 && pointPerfectIsEnabled())
        {
            settings.autoKeyRenewal ^= 1;
        }
        else if (incoming == 3 && pointPerfectIsEnabled())
        {
            if (wifiNetworkCount() == 0)
            {
                systemPrintln("Error: Please enter at least one SSID before getting keys");
            }
            else
            {
                if (wifiConnect(10000) == true)
                {
                    // Check if we have certificates
                    char fileName[80];
                    snprintf(fileName, sizeof(fileName), "/%s_%s_%d.txt", platformFilePrefix, "certificate",
                             profileNumber);
                    if (LittleFS.exists(fileName) == false)
                    {
                        pointperfectProvisionDevice(); // Connect to ThingStream API and get keys
                    }
                    else if (strlen(settings.pointPerfectCurrentKey) == 0 ||
                             strlen(settings.pointPerfectKeyDistributionTopic) == 0)
                    {
                        pointperfectProvisionDevice(); // Connect to ThingStream API and get keys
                    }
                    else // We have certs and keys
                    {
                        // Check that the certs are valid
                        if (checkCertificates() == true)
                        {
                            // Update the keys
                            pointperfectUpdateKeys();
                        }
                        else
                        {
                            // Erase keys
                            erasePointperfectCredentials();

                            // Provision device
                            pointperfectProvisionDevice(); // Connect to ThingStream API and get keys
                        }
                    }
                }
                else
                {
                    systemPrintln("Error: No WiFi available to get keys");
                }
            }

            WIFI_STOP();
        }
        else if (incoming == 4 && pointPerfectIsEnabled())
        {
            char hardwareID[15];
            snprintf(hardwareID, sizeof(hardwareID), "%02X%02X%02X%02X%02X%02X%02X", btMACAddress[0], btMACAddress[1],
                     btMACAddress[2], btMACAddress[3], btMACAddress[4], btMACAddress[5], productVariant);
            systemPrintf("Device ID: %s\r\n", hardwareID);
        }
        else if (incoming == 'c' && pointPerfectIsEnabled())
        {
            settings.pointPerfectCurrentKey[0] = 0;
            settings.pointPerfectNextKey[0] = 0;
        }
        else if (incoming == 'k' && pointPerfectIsEnabled())
        {
            menuPointPerfectKeys();
        }
        else if (incoming == 'g')
        {
            settings.geographicRegion++;
            if (settings.geographicRegion >= numRegionalAreas)
                settings.geographicRegion = 0;
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

    if (strlen(settings.pointPerfectClientID) > 0)
    {
        gnssApplyPointPerfectKeys();
    }

    clearBuffer(); // Empty buffer of any newline chars
}

bool pointPerfectIsEnabled()
{
    return (settings.enablePointPerfectCorrections);
}

// Process any new L-Band from I2C
void updateLBand()
{
    static unsigned long lbandLastReport;
    static unsigned long lbandTimeFloatStarted; // Monitors the ZED during L-Band reception if a fix takes too long

    // Skip if in configure-via-ethernet mode
    if (configureViaEthernet)
    {
        // if (settings.debugCorrections == true)
        //     systemPrintln("configureViaEthernet: skipping updateLBand");
        return;
    }

#ifdef COMPILE_L_BAND
    if (online.lbandCorrections == true)
    {
        i2cLBand.checkUblox();     // Check for the arrival of new PMP data and process it.
        i2cLBand.checkCallbacks(); // Check if any L-Band callbacks are waiting to be processed.

        // If a certain amount of time has elapsed between last decryption, turn off L-Band icon
        if (lbandCorrectionsReceived == true && millis() - lastLBandDecryption > 5000)
            lbandCorrectionsReceived = false;

        // If we don't get an L-Band fix within Timeout, hot-start ZED-F9x
        if (gnssIsRTKFloat())
        {
            if (lbandTimeFloatStarted == 0)
                lbandTimeFloatStarted = millis();

            if (millis() - lbandLastReport > 1000)
            {
                lbandLastReport = millis();

                if (settings.debugCorrections == true)
                    systemPrintf("ZED restarts: %d Time remaining before Float lock forced restart: %ds\r\n",
                                 floatLockRestarts,
                                 settings.lbandFixTimeout_seconds - ((millis() - lbandTimeFloatStarted) / 1000));
            }

            if (settings.lbandFixTimeout_seconds > 0)
            {
                if ((millis() - lbandTimeFloatStarted) > (settings.lbandFixTimeout_seconds * 1000L))
                {
                    floatLockRestarts++;

                    lbandTimeFloatStarted =
                        millis(); // Restart timer for L-Band. Don't immediately reset ZED to achieve fix.

                    // Hotstart ZED to try to get RTK lock
                    theGNSS->softwareResetGNSSOnly();

                    if (settings.debugCorrections == true)
                        systemPrintf("Restarting ZED. Number of Float lock restarts: %d\r\n", floatLockRestarts);
                }
            }
        }
        else if (gnssIsRTKFix() && rtkTimeToFixMs == 0)
        {
            lbandTimeFloatStarted = 0; // Restart timer in case we drop from RTK Fix

            rtkTimeToFixMs = millis();
            if (settings.debugCorrections == true)
                systemPrintf("Time to first RTK Fix: %ds\r\n", rtkTimeToFixMs / 1000);
        }
        else
        {
            // We are not in float or fix, so restart timer
            lbandTimeFloatStarted = 0;
        }
    }

#endif // COMPILE_L_BAND
}
