#include "mbedtls/ctr_drbg.h"
#include "mbedtls/ssl.h" //Needed for certificate validation

//----------------------------------------
// Locals - compiled out
//----------------------------------------

// The PointPerfect token is provided at compile time via build flags
#define DEVELOPMENT_TOKEN 0xAA, 0xBB, 0xCC, 0xDD, 0x00, 0x11, 0x22, 0x33, 0x0A, 0x0B, 0x0C, 0x0D, 0x00, 0x01, 0x02, 0x03

#ifndef POINTPERFECT_LBAND_TOKEN
#warning Using the DEVELOPMENT_TOKEN for PointPerfect!
#define POINTPERFECT_LBAND_TOKEN DEVELOPMENT_TOKEN
#define POINTPERFECT_IP_TOKEN DEVELOPMENT_TOKEN
#define POINTPERFECT_LBAND_IP_TOKEN DEVELOPMENT_TOKEN
#endif // POINTPERFECT_LBAND_TOKEN

static const uint8_t developmentToken[16] = {DEVELOPMENT_TOKEN};         // Token in HEX form
static const uint8_t ppLbandToken[16] = {POINTPERFECT_LBAND_TOKEN};      // Token in HEX form
static const uint8_t ppIpToken[16] = {POINTPERFECT_IP_TOKEN};            // Token in HEX form
static const uint8_t ppLbandIpToken[16] = {POINTPERFECT_LBAND_IP_TOKEN}; // Token in HEX form
static unsigned long provisioningStartTime_millis;
static bool provisioningRunning;

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

            gpsEpoch += (settings.pointPerfectCurrentKeyDuration / MILLISECONDS_IN_A_SECOND) -
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
                         MILLISECONDS_IN_A_SECOND); // Add key duration back to the key start date to get key expiration

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
            settings.pointPerfectCurrentKeyDuration = 28LL * MILLISECONDS_IN_A_DAY;

            // Calculate the next key expiration date
            settings.pointPerfectNextKeyStart = settings.pointPerfectCurrentKeyStart +
                                                settings.pointPerfectCurrentKeyDuration +
                                                1; // Next key starts after current key
            settings.pointPerfectNextKeyDuration = settings.pointPerfectCurrentKeyDuration;

            if (settings.debugCorrections == true)
                pointperfectPrintKeyInformation("Menu PP");
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
const char *printDateFromGPSEpoch(long long gpsEpoch)
{
    static char response[strlen("01/01/1010") + 1]; // Make room for terminator

    uint16_t keyGPSWeek;
    uint32_t keyGPSToW;
    epochToWeekToW(gpsEpoch, &keyGPSWeek, &keyGPSToW);

    long expDay;
    long expMonth;
    long expYear;
    gpsWeekToWToDate(keyGPSWeek, keyGPSToW, &expDay, &expMonth, &expYear);

    sprintf(response, "%02ld/%02ld/%ld", expDay, expMonth, expYear);
    return ((const char *)response);
}

// Given a Unix Epoch, return a DD/MM/YYYY string
// https://www.epochconverter.com/programming/c
const char *printDateFromUnixEpoch(long long unixEpoch)
{
    static char response[strlen("01/01/2023") + 1]; // Make room for terminator

    time_t rawtime = unixEpoch;

    struct tm ts;
    ts = *localtime(&rawtime);

    // Format time, "dd/mm/yyyy"
    strftime(response, strlen("01/01/2023") + 1, "%d/%m/%Y", &ts);
    return ((const char *)response);
}

// Given a duration in ms, print days
const char *printDaysFromDuration(long long duration)
{
    static char response[strlen("99.99") + 1]; // Make room for terminator

    float days = duration / MILLISECONDS_IN_A_DAY; // Convert ms to days

    sprintf(response, "%0.2f", days);
    return ((const char *)response);
}

// Create a ZTP request to be sent to thingstream JSON API
void createZtpRequest(String &str)
{
    // Assume failure
    str = "";

    // Get the hardware ID
    char hardwareID[15];
    snprintf(hardwareID, sizeof(hardwareID), "%02X%02X%02X%02X%02X%02X%02X", btMACAddress[0], btMACAddress[1],
             btMACAddress[2], btMACAddress[3], btMACAddress[4], btMACAddress[5], productVariant);

    // Get the firmware version string
    char versionString[9];
    firmwareVersionGet(versionString, sizeof(versionString), false);

    // Build the givenName:   Name vxx.yy - HardwareID
    char givenName[100];
    memset(givenName, 0, sizeof(givenName));
    snprintf(givenName, sizeof(givenName), "%s %s - %s", platformProvisionTable[productVariant], versionString,
             hardwareID);
    if (strlen(givenName) >= 50)
    {
        systemPrintf("Error: GivenName '%s' too long: %d bytes\r\n", givenName, strlen(givenName));
        return;
    }

    // Get the token
    char tokenString[37] = "\0";
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
            char tokenChar = tokenString[4];
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

    // Build the JSON request
    JsonDocument json;
    json["tags"][0] = "ztp";
    json["token"] = tokenString;
    json["hardwareId"] = hardwareID;
    json["givenName"] = givenName;

    // Debug the request
    if (settings.debugCorrections == true)
    {
        char tokenChar;
        systemPrintln("{");
        tokenChar = tokenString[4];
        tokenString[4] = 0;
        systemPrintf("  token: %s\r\n", tokenString);
        tokenString[4] = tokenChar;
        systemPrintf("  givenName: %s\r\n", givenName);
        systemPrintf("  hardwareId: %s\r\n", hardwareID);
        systemPrintln("}");
    }

    // Convert the JSON to a string
    serializeJson(json, str);
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

    if (productVariant == RTK_EVK) // EVK
    {
        pointperfectCreateTokenString(tokenString, (uint8_t *)ppLbandIpToken, sizeof(ppLbandIpToken));
    }
    else if (present.gnss_mosaicX5 == false && present.lband_neo == false) // Torch, Facet v2
    {
        // If the hardware lacks L-Band capability, use IP token
        pointperfectCreateTokenString(tokenString, (uint8_t *)ppIpToken, sizeof(ppIpToken));
    }
    else if (present.gnss_mosaicX5 == true || present.lband_neo == true) // Facet mosaic, Facet v2 L-Band
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

// Find thing3 in (*jsonZtp)[thing1][n][thing2]. Return n on success. Return -1 on error / not found.
int findZtpJSONEntry(const char *thing1, const char *thing2, const char *thing3, JsonDocument *jsonZtp)
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
    certificateContents = (char *)rtkMalloc(MQTT_CERT_SIZE, "Certificate buffer (certificateContents)");
    keyContents = (char *)rtkMalloc(MQTT_CERT_SIZE, "Certificate buffer (keyContents)");
    if ((!certificateContents) || (!keyContents))
    {
        if (certificateContents)
            rtkFree(certificateContents, "Certificate buffer (certificateContents)");
        if (keyContents)
            rtkFree(keyContents, "Certificate buffer (keyContents)");
        systemPrintln("Failed to allocate content buffers!");
        return (false);
    }

    // Load the certificate
    memset(certificateContents, 0, MQTT_CERT_SIZE);
    if (!loadFile("certificate", certificateContents, settings.debugPpCertificate))
    {
        systemPrintf("ERROR: Failed to open the certificate file\r\n");
        validCertificates = false;
    }
    else if (checkCertificateValidity(certificateContents, strlen(certificateContents)) == false)
    {
        if (settings.debugPpCertificate)
            systemPrintln("Certificate is corrupt.");
        validCertificates = false;
    }

    // Load the private key
    memset(keyContents, 0, MQTT_CERT_SIZE);
    if (!loadFile("privateKey", keyContents, settings.debugPpCertificate))
    {
        systemPrintf("ERROR: Failed to open the private key file\r\n");
        validCertificates = false;
    }
    else if (checkPrivateKeyValidity(keyContents, strlen(keyContents)) == false)
    {
        if (settings.debugPpCertificate)
            systemPrintln("PrivateKey is corrupt.");
        validCertificates = false;
    }

    // Free the content buffers
    if (certificateContents)
        rtkFree(certificateContents, "Certificate buffer (certificateContents)");
    if (keyContents)
        rtkFree(keyContents, "Certificate buffer (keyContents)");

    if (settings.debugPpCertificate)
    {
        systemPrintf("Stored certificates are %svalid\r\n", validCertificates ? "" : "NOT ");
    }

    // Enable MQTT once the certificates are available and valid
    if (validCertificates)
        mqttClientStartEnabled();
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

    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ctr_drbg_init(&ctr_drbg);

    int result_code = mbedtls_pk_parse_key(&pk, (unsigned char *)privateKey, privateKeySize + 1, nullptr, 0,
                                           mbedtls_ctr_drbg_random, &ctr_drbg);
    mbedtls_pk_free(&pk);
    mbedtls_ctr_drbg_free(&ctr_drbg);

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

// Given the key's starting epoch time, and the key's duration
// Convert from ms to s
// Add leap seconds (the API reports start times with GPS leap seconds removed)
// Convert from unix epoch (the API reports unix epoch time) to GPS epoch (the NED-D9S expects)
// Note: I believe the Thingstream API is reporting startEpoch 18 seconds less than actual
long long thingstreamEpochToGPSEpoch(long long startEpoch)
{
    long long epoch = startEpoch;
    epoch /= MILLISECONDS_IN_A_SECOND; // Convert PointPerfect ms Epoch to s

    // Convert Unix Epoch time from PointPerfect to GPS Time Of Week needed for UBX message
    long long gpsEpoch = epoch - 315964800 + gnss->getLeapSeconds(); // Shift to GPS Epoch.
    return (gpsEpoch);
}

//----------------------------------------
// Global L-Band Routines
//----------------------------------------

// Begin any L-Band hardware
// Check if NEO-D9S is connected. Configure if available.
// If GNSS is mosaic-X5, configure LBandBeam1
void beginLBand()
{

#ifdef COMPILE_L_BAND
    if (present.lband_neo)
    {
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

        gnss->update();

        uint32_t LBandFreq;
        uint8_t fixType = gnss->getFixType();
        double latitude = gnss->getLatitude();
        double longitude = gnss->getLongitude();
        // If we have a fix, check which frequency to use
        if (fixType >= 2 && fixType <= 5) // 2D, 3D, 3D+DR, or Time
        {
            int r = 0; // Step through each geographic region
            for (; r < numRegionalAreas; r++)
            {
                if ((longitude >= Regional_Information_Table[r].area.lonWest) &&
                    (longitude <= Regional_Information_Table[r].area.lonEast) &&
                    (latitude >= Regional_Information_Table[r].area.latSouth) &&
                    (latitude <= Regional_Information_Table[r].area.latNorth))
                {
                    LBandFreq = Regional_Information_Table[r].frequency;
                    if (settings.debugCorrections == true)
                        systemPrintf("Setting L-Band frequency to %s (%dHz)\r\n", Regional_Information_Table[r].name,
                                     LBandFreq);
                    break;
                }
            }
            if (r == numRegionalAreas) // Geographic region not found
            {
                LBandFreq = Regional_Information_Table[settings.geographicRegion].frequency;
                if (settings.debugCorrections == true)
                    systemPrintf("Error: Unknown L-Band geographic region. Using %s (%dHz)\r\n",
                                 Regional_Information_Table[settings.geographicRegion].name, LBandFreq);
            }
        }
        else
        {
            LBandFreq = Regional_Information_Table[settings.geographicRegion].frequency;
            if (settings.debugCorrections == true)
                systemPrintf("No fix available for L-Band geographic region determination. Using %s (%dHz)\r\n",
                             Regional_Information_Table[settings.geographicRegion].name, LBandFreq);
        }

        bool response = true;
        response &= i2cLBand.newCfgValset();
        response &= i2cLBand.addCfgValset(UBLOX_CFG_PMP_CENTER_FREQUENCY, LBandFreq); // Default 1539812500 Hz
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

        GNSS_ZED *zed = (GNSS_ZED *)gnss;
        response &= zed->lBandCommunicationEnable();

        if (response == false)
            systemPrintln("L-Band failed to configure");

        i2cLBand.softwareResetGNSSOnly(); // Do a restart

        if (settings.debugCorrections == true)
            systemPrintln("L-Band online");

        gnss->applyPointPerfectKeys(); // Apply keys now, if we have them. This sets online.lbandCorrections

        online.lband_neo = true;
    }
#endif // COMPILE_L_BAND
#ifdef COMPILE_MOSAICX5
    if (present.gnss_mosaicX5 && settings.enablePointPerfectCorrections)
    {
        uint32_t LBandFreq;
        uint8_t fixType = gnss->getFixType();
        double latitude = gnss->getLatitude();
        double longitude = gnss->getLongitude();
        // If we have a fix, check which frequency to use
        if (fixType >= 1) // Stand-Alone PVT or better
        {
            int r = 0; // Step through each geographic region
            for (; r < numRegionalAreas; r++)
            {
                if ((longitude >= Regional_Information_Table[r].area.lonWest) &&
                    (longitude <= Regional_Information_Table[r].area.lonEast) &&
                    (latitude >= Regional_Information_Table[r].area.latSouth) &&
                    (latitude <= Regional_Information_Table[r].area.latNorth))
                {
                    LBandFreq = Regional_Information_Table[r].frequency;
                    if (settings.debugCorrections == true)
                        systemPrintf("Setting L-Band frequency to %s (%dHz)\r\n", Regional_Information_Table[r].name,
                                     LBandFreq);
                    break;
                }
            }
            if (r == numRegionalAreas) // Geographic region not found
            {
                LBandFreq = Regional_Information_Table[settings.geographicRegion].frequency;
                if (settings.debugCorrections == true)
                    systemPrintf("Error: Unknown L-Band geographic region. Using %s (%dHz)\r\n",
                                 Regional_Information_Table[settings.geographicRegion].name, LBandFreq);
            }
        }
        else
        {
            LBandFreq = Regional_Information_Table[settings.geographicRegion].frequency;
            if (settings.debugCorrections == true)
                systemPrintf("No fix available for L-Band geographic region determination. Using %s (%dHz)\r\n",
                             Regional_Information_Table[settings.geographicRegion].name, LBandFreq);
        }

        bool result = true;

        // If no SPARTN data is received, the L-Band may need a 'kick'. Turn L-Band off and back on again!
        GNSS_MOSAIC *mosaic = (GNSS_MOSAIC *)gnss;

        result &= mosaic->configureGNSSCOM(true); // Ensure LBandBeam1 is enabled on COM1

        result &= mosaic->configureLBand(true, LBandFreq); // Start L-Band

        if (result == false)
            systemPrintln("mosaic-X5 L-Band failed to configure");
        else if (settings.debugCorrections == true)
            systemPrintln("mosaic-X5 L-Band online");

        online.lband_gnss = result;
    }
#endif // /COMPILE_MOSAICX5
}

// Provision device on ThingStream
// Download keys
void menuPointPerfect()
{
    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: PointPerfect Corrections");

        if (settings.debugCorrections == true)
            systemPrintf("Time to first RTK Fix: %ds Restarts: %d\r\n",
                         rtkTimeToFixMs / MILLISECONDS_IN_A_SECOND, floatLockRestarts);

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

        if ((settings.useLocalizedDistribution) && (localizedDistributionTileTopic.length() > 0))
        {
            systemPrint("Most recent localized distribution topic: ");
            systemPrintln(localizedDistributionTileTopic.c_str()); // From MQTT_Client.ino
        }

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
#ifdef COMPILE_NETWORK
            systemPrint("2) Toggle Auto Key Renewal: ");
            if (settings.autoKeyRenewal == true)
                systemPrintln("Enabled");
            else
                systemPrintln("Disabled");
            systemPrint("3) Request Key Update: ");
            if (settings.requestKeyUpdate == true)
                systemPrintln("Requested");
            else
                systemPrintln("Not requested");
            systemPrint("4) Use localized distribution: ");
            if (settings.useLocalizedDistribution == true)
                systemPrintln("Enabled");
            else
                systemPrintln("Disabled");
            if (settings.useLocalizedDistribution)
            {
                systemPrint("5) Localized distribution tile level: ");
                systemPrint(settings.localizedDistributionTileLevel);
                systemPrint(" (");
                systemPrint(localizedDistributionTileLevelNames[settings.localizedDistributionTileLevel]);
                systemPrintln(")");
            }
            if (productVariant != RTK_FACET_MOSAIC)
            {
                systemPrint("a) Use AssistNow: ");
                if (settings.useAssistNow == true)
                    systemPrintln("Enabled");
                else
                    systemPrintln("Disabled");
            }
#endif // COMPILE_NETWORK

            systemPrintln("c) Clear the Keys");

            systemPrintln("i) Show device ID");

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
            settings.requestKeyUpdate = settings.enablePointPerfectCorrections; // Force a key update - or don't
        }

#ifdef COMPILE_NETWORK
        else if (incoming == 2 && pointPerfectIsEnabled())
        {
            settings.autoKeyRenewal ^= 1;
            settings.requestKeyUpdate = settings.autoKeyRenewal; // Force a key update - or don't
        }
        else if (incoming == 3 && pointPerfectIsEnabled())
        {
            settings.requestKeyUpdate ^= 1;
        }
        else if (incoming == 4 && pointPerfectIsEnabled())
        {
            settings.useLocalizedDistribution ^= 1;
        }
        else if (incoming == 5 && pointPerfectIsEnabled() && settings.useLocalizedDistribution)
        {
            settings.localizedDistributionTileLevel++;
            if (settings.localizedDistributionTileLevel >= LOCALIZED_DISTRIBUTION_TILE_LEVELS)
                settings.localizedDistributionTileLevel = 0;
        }
        else if (incoming == 'a' && pointPerfectIsEnabled() && (productVariant != RTK_FACET_MOSAIC))
        {
            settings.useAssistNow ^= 1;
        }
#endif // COMPILE_NETWORK
        else if (incoming == 'c' && pointPerfectIsEnabled())
        {
            settings.pointPerfectCurrentKey[0] = 0;
            settings.pointPerfectNextKey[0] = 0;
        }
        else if (incoming == 'i' && pointPerfectIsEnabled())
        {
            char hardwareID[15];
            snprintf(hardwareID, sizeof(hardwareID), "%02X%02X%02X%02X%02X%02X%02X", btMACAddress[0], btMACAddress[1],
                     btMACAddress[2], btMACAddress[3], btMACAddress[4], btMACAddress[5], productVariant);
            systemPrintf("Device ID: %s\r\n", hardwareID);
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
        gnss->applyPointPerfectKeys();
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

#ifdef COMPILE_L_BAND
    if (online.lbandCorrections == true)
    {
        i2cLBand.checkUblox();     // Check for the arrival of new PMP data and process it.
        i2cLBand.checkCallbacks(); // Check if any L-Band callbacks are waiting to be processed.

        // If a certain amount of time has elapsed between last decryption, turn off L-Band icon
        if (lbandCorrectionsReceived == true && millis() - lastLBandDecryption > (5 * MILLISECONDS_IN_A_SECOND))
            lbandCorrectionsReceived = false;

        // If we don't get an L-Band fix within Timeout, hot-start ZED-F9x
        if (gnss->isRTKFloat())
        {
            if (lbandTimeFloatStarted == 0)
                lbandTimeFloatStarted = millis();

            if (millis() - lbandLastReport > MILLISECONDS_IN_A_SECOND)
            {
                lbandLastReport = millis();

                if (settings.debugCorrections == true)
                    systemPrintf("ZED restarts: %d Time remaining before Float lock forced restart: %ds\r\n",
                                 floatLockRestarts,
                                 settings.lbandFixTimeout_seconds - ((millis() - lbandTimeFloatStarted) / MILLISECONDS_IN_A_SECOND));
            }

            if (settings.lbandFixTimeout_seconds > 0)
            {
                if ((millis() - lbandTimeFloatStarted) > (settings.lbandFixTimeout_seconds * MILLISECONDS_IN_A_SECOND))
                {
                    floatLockRestarts++;

                    lbandTimeFloatStarted =
                        millis(); // Restart timer for L-Band. Don't immediately reset ZED to achieve fix.

                    // Hotstart GNSS to try to get RTK lock
                    gnss->softwareReset();

                    if (settings.debugCorrections == true)
                        systemPrintf("Restarting ZED. Number of Float lock restarts: %d\r\n", floatLockRestarts);
                }
            }
        }
        else if (gnss->isRTKFix() && rtkTimeToFixMs == 0)
        {
            lbandTimeFloatStarted = 0; // Restart timer in case we drop from RTK Fix

            rtkTimeToFixMs = millis();
            if (settings.debugCorrections == true)
                systemPrintf("Time to first RTK Fix: %ds\r\n", rtkTimeToFixMs / MILLISECONDS_IN_A_SECOND);
        }
        else
        {
            // We are not in float or fix, so restart timer
            lbandTimeFloatStarted = 0;
        }
    }

#endif // COMPILE_L_BAND
}

enum ProvisioningStates
{
    PROVISIONING_OFF = 0,
    PROVISIONING_CHECK_REMAINING,
    PROVISIONING_WAIT_FOR_NETWORK,
    PROVISIONING_STARTED,
    PROVISIONING_KEYS_REMAINING,
    PROVISIONING_STATE_MAX,
};
static volatile uint8_t provisioningState = PROVISIONING_OFF;

const char *const provisioningStateName[] = {"PROVISIONING_OFF",
                                             "PROVISIONING_CHECK_REMAINING",
                                             "PROVISIONING_WAIT_FOR_NETWORK",
                                             "PROVISIONING_STARTED",
                                             "PROVISIONING_KEYS_REMAINING"};

const int provisioningStateNameEntries = sizeof(provisioningStateName) / sizeof(provisioningStateName[0]);

void provisioningVerifyTables()
{
    // Verify the table length
    if (provisioningStateNameEntries != PROVISIONING_STATE_MAX)
        reportFatalError("Please fix provisioningStateName table to match ProvisioningStates");
}

void provisioningSetState(uint8_t newState)
{
    if (settings.debugPpCertificate)
    {
        if (provisioningState == newState)
            systemPrint("Provisioning: *");
        else
            systemPrintf("Provisioning: %s --> ", provisioningStateName[provisioningState]);
    }
    provisioningState = newState;
    if (settings.debugPpCertificate)
    {
        if (newState >= PROVISIONING_STATE_MAX)
        {
            systemPrintf("Unknown provisioning state: %d\r\n", newState);
            reportFatalError("Unknown provisioning state");
        }
        else
            systemPrintln(provisioningStateName[provisioningState]);
    }
}

// Determine if provisioning is enabled
bool provisioningEnabled(const char ** line)
{
    bool enabled;

    do
    {
        // Provisioning requires PointPerfect corrections
        enabled = settings.enablePointPerfectCorrections;
        if (enabled == false)
        {
            *line = ", PointPerfect corrections disabled!";
            break;
        }

        // Keep running until provisioning attempt is complete
        if (provisioningRunning)
            break;

        // Determine if provisioning should start
        provisioningRunning = settings.requestKeyUpdate // Manual update
            || (provisioningStartTime_millis == 0) // Update keys at boot
            || (settings.autoKeyRenewal && // Auto renewal time (24 hours expired)
                ((millis() - provisioningStartTime_millis) > MILLISECONDS_IN_A_DAY));

        // Determine if key provisioning is enabled
        enabled = provisioningRunning;
        if (settings.autoKeyRenewal)
            *line = ", Key not requested and auto key renewal running later!";
        else
            *line = ", Key not requested and auto key renewal is disabled!";
    } while (0);
    return enabled;
}

// Determine if the keys are needed
bool provisioningKeysNeeded()
{
    bool keysNeeded;

    do
    {
        keysNeeded = true;

        // If we don't have certs or keys, begin zero touch provisioning
        if (!checkCertificates() || strlen(settings.pointPerfectCurrentKey) == 0 ||
            strlen(settings.pointPerfectNextKey) == 0)
        {
            if (settings.debugPpCertificate)
                systemPrintln("Invalid certificates or keys.");
            break;
        }

        // If requestKeyUpdate is true, begin provisioning
        if (settings.requestKeyUpdate)
        {
            if (settings.debugPpCertificate)
                systemPrintln("requestKeyUpdate is true.");
            break;
        }

        // Determine if RTC is online
        if (!online.rtc)
        {
            if (settings.debugPpCertificate)
                systemPrintln("No RTC.");
            break;
        }

        // RTC is online. Determine days until next key expires
        int daysRemaining =
            daysFromEpoch(settings.pointPerfectNextKeyStart + settings.pointPerfectNextKeyDuration + 1);

        if (settings.debugPpCertificate)
            systemPrintf("Days until keys expire: %d\r\n", daysRemaining);

        // PointPerfect returns keys that expire at midnight so the primary key
        // is still available with 0 days left, and a Next Key that has 28 days left
        // If there are 28 days remaining, PointPerfect won't have new keys.
        if (daysRemaining < 28)
        {
            // When did we last try to get keys? Attempt every 24 hours - or always for DEVELOPER
            // if (rtc.getEpoch() - settings.lastKeyAttempt > ( ENABLE_DEVELOPER ? 0 : SECONDS_IN_A_DAY))
            // When did we last try to get keys? Attempt every 24 hours
            if (rtc.getEpoch() - settings.lastKeyAttempt > SECONDS_IN_A_DAY)
            {
                settings.lastKeyAttempt = rtc.getEpoch(); // Mark it
                recordSystemSettings();                   // Record these settings to unit
                break;
            }

            if (settings.debugPpCertificate)
                systemPrintln("Already tried to obtain keys for today");
        }

        // Don't need new keys
        keysNeeded = false;
    } while (0);
    if (keysNeeded && settings.debugPpCertificate)
        systemPrintln(" Starting provisioning");
    return keysNeeded;
}

void provisioningStop(const char * file, uint32_t line)
{
    // Done with this request attempt
    settings.requestKeyUpdate = false;
    provisioningRunning = false;

    // Record the time so we can restart after 24 hours
    provisioningStartTime_millis = millis();

    // Done with the network
    networkConsumerRemove(NETCONSUMER_PPL_KEY_UPDATE, NETWORK_ANY, file, line);
    provisioningSetState(PROVISIONING_OFF);
}

void provisioningUpdate()
{
    bool enabled;
    const char * line = "";
    const unsigned long provisioningTimeout_ms = 2 * MILLISECONDS_IN_A_MINUTE;
    static bool rtcOnline;

    // Determine if key provisioning is enabled
    DMW_st(provisioningSetState, provisioningState);
    enabled = provisioningEnabled(&line);

    // Determine if the RTC was properly initialized
    if (rtcOnline == false)
        rtcOnline = online.rtc || settings.requestKeyUpdate
                  || (millis() > provisioningTimeout_ms);

    switch (provisioningState)
    {
    default:
    case PROVISIONING_OFF: {
        // If RTC is not online after provisioningTimeout_ms, try to provision anyway
        if (enabled && rtcOnline)
            provisioningSetState(PROVISIONING_CHECK_REMAINING);
    }
    break;
    case PROVISIONING_CHECK_REMAINING: {
        if (provisioningKeysNeeded() == false)
            provisioningSetState(PROVISIONING_KEYS_REMAINING);
        else
        {
            // Request the network for PointPerfect key provisioning
            networkConsumerAdd(NETCONSUMER_PPL_KEY_UPDATE, NETWORK_ANY, __FILE__, __LINE__);
            provisioningSetState(PROVISIONING_WAIT_FOR_NETWORK);
        }
    }
    break;

    // Wait for connection to the network
    case PROVISIONING_WAIT_FOR_NETWORK: {
        // Stop waiting if PointPerfect has been disabled
        if (enabled == false)
            provisioningStop(__FILE__, __LINE__);

        // Wait until the network is available
        else if (networkConsumerIsConnected(NETCONSUMER_PPL_KEY_UPDATE))
        {
            if (settings.debugPpCertificate)
                systemPrintln("PointPerfect key update connected to network");

            // Go get latest keys
            ztpResponse = ZTP_NOT_STARTED;           // HTTP_Client will update this
            httpClientModeNeeded = true;             // This will start the HTTP_Client
            provisioningStartTime_millis = millis(); // Record the start time so we can timeout
            paintGettingKeys();
            networkUserAdd(NETCONSUMER_PPL_KEY_UPDATE, __FILE__, __LINE__);
            provisioningSetState(PROVISIONING_STARTED);
        }
    }
    break;

    case PROVISIONING_STARTED: {
        // Only leave this state if we timeout or ZTP is complete
        if (millis() > (provisioningStartTime_millis + provisioningTimeout_ms))
        {
            httpClientModeNeeded = false; // Tell HTTP_Client to give up. (But it probably already has...)
            paintKeyUpdateFail(5 * MILLISECONDS_IN_A_SECOND);
            provisioningSetState(PROVISIONING_KEYS_REMAINING);
        }
        else if (ztpResponse == ZTP_SUCCESS)
        {
            httpClientModeNeeded = false; // Tell HTTP_Client to give up
            recordSystemSettings();       // Make sure the new cert and keys are recorded
            systemPrintln("Keys successfully updated!");
            provisioningSetState(PROVISIONING_KEYS_REMAINING);
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
            else if (productVariant == RTK_EVK)
                snprintf(landingPageUrl, sizeof(landingPageUrl),
                         "or goto https://www.sparkfun.com/rtk_evk_registration ");
            else if (productVariant == RTK_POSTCARD)
                snprintf(landingPageUrl, sizeof(landingPageUrl),
                         "or goto https://www.sparkfun.com/rtk_postcard_registration ");
            else
                systemPrintln("pointperfectProvisionDevice(): Platform missing landing page");

            systemPrintf("This device has been deactivated. Please contact "
                         "support@sparkfun.com %sto renew the PointPerfect "
                         "subscription. Please reference device ID: %s\r\n",
                         landingPageUrl, hardwareID);

            httpClientModeNeeded = false; // Tell HTTP_Client to give up.
            displayAccountExpired(5 * MILLISECONDS_IN_A_SECOND);

            provisioningSetState(PROVISIONING_KEYS_REMAINING);
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
            else if (productVariant == RTK_EVK)
                snprintf(landingPageUrl, sizeof(landingPageUrl),
                         "or goto https://www.sparkfun.com/rtk_evk_registration ");
            else if (productVariant == RTK_POSTCARD)
                snprintf(landingPageUrl, sizeof(landingPageUrl),
                         "or goto https://www.sparkfun.com/rtk_postcard_registration ");
            else
                systemPrintln("pointperfectProvisionDevice(): Platform missing landing page");

            systemPrintf("This device is not whitelisted. Please contact "
                         "support@sparkfun.com %sto get the subscription "
                         "activated. Please reference device ID: %s\r\n",
                         landingPageUrl, hardwareID);

            httpClientModeNeeded = false; // Tell HTTP_Client to give up.
            displayNotListed(5 * MILLISECONDS_IN_A_SECOND);

            provisioningSetState(PROVISIONING_KEYS_REMAINING);
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

            httpClientModeNeeded = false; // Tell HTTP_Client to give up.
            displayAlreadyRegistered(5 * MILLISECONDS_IN_A_SECOND);

            provisioningSetState(PROVISIONING_KEYS_REMAINING);
        }
        else if (ztpResponse == ZTP_UNKNOWN_ERROR)
        {
            systemPrintln("updateProvisioning: ZTP_UNKNOWN_ERROR");

            httpClientModeNeeded = false; // Tell HTTP_Client to give up.

            provisioningSetState(PROVISIONING_KEYS_REMAINING);
        }
    }
    break;
    case PROVISIONING_KEYS_REMAINING: {
        if (online.rtc == true)
        {
            if (settings.pointPerfectNextKeyStart > 0)
            {
                int daysRemaining =
                    daysFromEpoch(settings.pointPerfectNextKeyStart + settings.pointPerfectNextKeyDuration + 1);
                systemPrintf("Days until PointPerfect keys expire: %d\r\n", daysRemaining);
                if (daysRemaining >= 0)
                {
                    paintKeyDaysRemaining(daysRemaining, 2 * MILLISECONDS_IN_A_SECOND);
                }
                else
                {
                    paintKeysExpired();
                }
            }
        }
        paintLBandConfigure();

        // Be sure we ignore any external RTCM sources
        gnss->rtcmOnGnssDisable();

        gnss->applyPointPerfectKeys(); // Send current keys, if available, to GNSS

        recordSystemSettings();            // Record these settings to unit

        // Done with the network
        provisioningStop(__FILE__, __LINE__);
    }
    break;
    }

    // Periodically display the provisioning state
    if (PERIODIC_DISPLAY(PD_PROVISIONING_STATE))
    {
        systemPrintf("Provisioning state: %s%s\r\n",
                     provisioningStateName[provisioningState], line);
        PERIODIC_CLEAR(PD_PROVISIONING_STATE);
    }
}
