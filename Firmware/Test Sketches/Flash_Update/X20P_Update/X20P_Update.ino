/*
    This example shows how to read a firmware file from an array and send chunks to the ZED-X20P.
    The goal is to eventually read from WiFi.

    This was written for FP hardware.

    To test: load this sketch onto an FP.
    Press 'u' to start the update. Allow the update to complete.
    Press 'g' to reset the GNSS in case it gets partially loaded or frozen.
    Press 'r' to reset. The GNSS module should boot and respond to commands.

    If the update fails, the ZED-X20P must be hardware reset or power reset, at which time
    it will enter into a state where it can enter bootloader mode again.

    All loaders should have similar structure:
    Given the web address of the binary to load,
    Do the WiFi stuff to begin reading the file data
    Put the target into bootload mode and malloc any necessary buffers xxxUpdateFirmwareBegin()
    Grab chunks of bytes over WiFi and throw at xxxUpdateFirmware(*data, length)
    When done, call xxxUpdateFirmwareEnd() to free buffers and exit the bootloader mode or reset the target

    Test procedure commands:
    1) a    ?.?? --> 2.02   Verify 'a' command and array
    2) e    2.02 --> 2.10   Verify 'e' command and HTTP, connect to somewhere
                            other than raw.githubusercontent.com using http://
    3) e    2.10 --> 2.02   Verify HTTPS, connect to somewhere other than
                            raw.githubusercontent.com using https://
    4) L                    Verify 'L' command and directory listing
       0    2.02 --> 2.10   Verify HTTPS
    5) a    2.10 --> 2.02   Verify 'o' command and URL
    6) u    2.02 --> 2.10   Verify 'u' command and URL
    7) p    2.10 --> 2.02   Verify 'p' command and URL
    8) u    2.02 --> 2.10   Leave at highest revision
    9) g                    Verify 'g' command, displays the current version (2.10)

    Test procedure commands (Verifies HTTP, HTTPS and array):
    1) a    ?.?? --> 2.02   Verify array
    2) L                    Verify directory listing
       0    2.02 --> 2.10   Verify HTTPS
    2) e    2.10 --> 2.02   Verify HTTP, connect to somewhere other than
                            raw.githubusercontent.com using http://
    4) u    2.02 --> 2.10   Leave at highest revision
*/

//----------------------------------------
// Common declarations
//----------------------------------------

bool RTK_CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC = false; // Needed because of local BT TLS patch

#include <arpa/inet.h>
#include <HTTPClient.h>
#include <netdb.h>
#include <Network.h>
#include <NetworkClientSecure.h>
#include <sys/socket.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#ifndef ENABLE_DEVELOPER
#define ENABLE_DEVELOPER            true
#endif   // ENABLE_DEVELOPER
#define DMW_if if (0)

const uint8_t logoSparkFun[] = {0};
#define logoSparkFun_Height         1
#define logoSparkFun_Width          1

const uint8_t logoSparkPNT[] = {0};
#define logoSparkPNT_Height         1
#define logoSparkPNT_Width          1

#include "Firmware_Data_Stream.h"
#include "secrets.h"
#include "settings.h"
#define COMPILE_ALL_FIRMWARE
#include "TheData.h"

#define rtkMalloc(bytes, description)       malloc(bytes)
#define rtkFree(buffer, description)        free(buffer)

//----------------------------------------
// Test specific declarations
//----------------------------------------

Firmware_Data_Stream dataArray(firmwareData, sizeof(firmwareData));

const char * subsystem = "GNSS";
const char * chip = "X20P";

uint8_t rxBuffer[2048];

const char * urlDirectory = "https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/tree/main/gnss/zed-x20p";

const char * ulrFileServer = "https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/main/gnss/zed-x20p/";

// 2.02
const char * url_2_02 = "https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/main/gnss/zed-x20p/UBX_20_HPG_202_ZED_F20P.329facb56ce18631d607fe15177834dc.bin";

// 2.10
const char * url_2_10 = "https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/main/gnss/zed-x20p/UBX_20_HPG_210_ZED_X20P-01B.512369040097ce18fd3475e71e7c627f.bin";

// ==================================================================
//  RECEIVE BUFFER
//  ACK / response payloads are tiny (2–5 bytes), but UBX-MON-VER's
//  swVersion (30 bytes) + hwVersion (10 bytes) needs 40.  Only the
//  first X20P_RX_PAYLOAD_MAX bytes of any incoming payload are stored.
// ==================================================================

#define X20P_RX_PAYLOAD_MAX 40u

struct UbxMsg
{
    uint8_t cls;
    uint8_t id;
    uint16_t len;
    uint8_t payload[X20P_RX_PAYLOAD_MAX];
};

//----------------------------------------
// Connects to the configured SSID and blocks until connected or the attempt times out.
//----------------------------------------
bool wifiConnect()
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSSID, wifiPassword);
    return wifiWaitUntilConnected();
}

//----------------------------------------
// Wait for the WiFi connection
//----------------------------------------
bool wifiWaitUntilConnected()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        systemPrint("Connecting to WiFi SSID: ");
        systemPrintln(wifiSSID);

        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED)
        {
            if ((millis() - start) > 20000)
            {
                systemPrintln("WiFi connection timed out.");
                return false;
            }
            delay(250);
            systemPrint(".");
        }

        systemPrint("WiFi connected, IP address: ");
        systemPrintln(WiFi.localIP());
    }
    return true;
}

//----------------------------------------
// Test entry point
//----------------------------------------
void setup()
{
    // Common setup
    Serial.begin(115200);
    delay(250);

    identifyBoard(); // Determine what hardware platform we are running on.
    beginBoard();    // Set all pin numbers and pin initial states
    beginMux();      // Must come before I2C activity to avoid external
                     // devices from corrupting the bus. See issue 474
                     //  https://github.com/sparkfun/SparkFun_RTK_Firmware/issues/474
    peripheralsOn(); // Enable power for the display, SD, etc
    beginI2C();      // Requires settings and peripheral power (if applicable).
    if (wifiConnect() == false)
        reportFatalError("WiFi network not found!");

    // Test specific setup
    systemPrintln("ZED-X20P firmware update example");

    // Configure the product
    if (productVariant == RTK_FACET_FP)
    {
        beginGpioExpanderSwitches();
        gpioExpanderConnectGNSSToESP32(); // Connect GNSS receiver UART1 to ESP32 UART1 for normal comms
    }
    else if (present.gnss_zedx20p)
        reportFatalError("Please add missing product configuration");
    else
        reportFatalError("An X20P is not in this product");

    serialGNSS = new HardwareSerial(1);
    systemPrintln("Serial GNSS started");

    // Display the current firmware version
    x20pDisplayVersion(subsystem, chip);

    displayMenu();
}

//----------------------------------------
// Test serial menu
//----------------------------------------
void displayMenu()
{
    systemPrintln();
    systemPrintln("Menu:");

    // Test specific menu items
    systemPrintln("a) Update GNSS to 2.02 from array");
    systemPrintln("p) Update GNSS to 2.02");
    systemPrintln("u) Update GNSS to 2.10");
    systemPrintln("e) Enter URL");
    systemPrintln("L) List all versions");
    systemPrintln("g) Reset GNSS");

    // Common menu items
    systemPrintln("r) Reboot system");
    systemPrintf("d) Debug: %s\r\n", settings.debugFirmwareUpdate ? "Enabled" : "Disabled");
    systemPrintf("v) Verbose output: %s\r\n", otaDebugVerbose ? "Enabled" : "Disabled");
    systemPrint("Make selection: ");
}

//----------------------------------------
// Process user input
//----------------------------------------
void loop()
{
    String urlString;

    // Loop common code
    wifiWaitUntilConnected();
    if (Serial.available())
    {
        // Get and echo the user input
        byte incoming = Serial.read();
        Serial.printf("%c\r\n", incoming);

        // Process the menu item
        if (incoming == 'r')
            ESP.restart();
        else if (incoming == 'd')
        {
            settings.debugFirmwareUpdate ^= 1;
            otaDebugVerbose = false;
        }
        else if (incoming == 'v')
            otaDebugVerbose ^= 1;

        // Test specific menu items
        else if (incoming == 'a')
            flashUpdate(nullptr);
        else if (incoming == 'g')
            x20pDisplayVersion(subsystem, chip);
        else if (incoming == 'e')
        {
            // Get the URL
            systemPrint("Enter URL: ");
            urlString = systemGetStringFromUser();
            if (urlString.length())
                flashUpdate(urlString.c_str());
        }
        else if (incoming == 'L')
        {
            // Get the SparkFun directory page
            urlString = serverSelectFileNameFromDirectoryListing(urlDirectory,
                                                                 otaFileTree,
                                                                 otaListEnd,
                                                                 otaItems,
                                                                 otaName,
                                                                 otaNameEnd,
                                                                 "UBX_",
                                                                 ".bin",
                                                                 ulrFileServer);
            if (urlString.length() != 0)
            {
                wifiWaitUntilConnected();
                flashUpdate(urlString.c_str());
            }
        }
        else if (incoming == 'p')
            flashUpdate(url_2_02);
        else if (incoming == 'u')
            flashUpdate(url_2_10);

        // Display the menu again
        displayMenu();
    }
}

//----------------------------------------
// Perform the flash update and display duration
//----------------------------------------
void flashUpdate(const char * url)
{
    // Start timer before erase
    uint32_t flashUpdateStartTime = millis();

    // Attempt to update the firmware
    dataArray.init(0);
    if (((url != nullptr) && (x20pFirmwareUpdate(subsystem,
                                                 chip,
                                                 url,
                                                 rxBuffer,
                                                 sizeof(rxBuffer)) == true))
        || ((url == nullptr) && x20pArrayFlashUpdate(subsystem,
                                                     chip,
                                                     rxBuffer,
                                                     sizeof(rxBuffer))))
    {
        // Stop timer and print elapsed time
        uint32_t flashUpdateElapsed = millis() - flashUpdateStartTime;
        systemPrintf("%s (%s) firmware update time: ", chip, subsystem);
        systemPrint(flashUpdateElapsed / 1000.0, 3);
        systemPrint(" seconds, ");
        systemPrint(otaFileBytes);
        systemPrint(" bytes, ");
        systemPrint((int)(otaFileBytes / ((flashUpdateElapsed + 500) / 1000)));
        systemPrintln(" bytes/second");
    }

    // Always reboot the system
    ESP.restart();
}
