/*
    This example shows how to update the IM19 over WiFi.

    This was written for Torch and FP hardware.

    Ported from the reference implementation in upgrade.c: a 268-byte framed
    protocol (0xAA55 header, 256-byte payload, uint32 checksum) used to push
    a firmware image to the IM19 module and confirm it booted the new image.

    To test: load this sketch onto a Torch or FP.
    Press 'u' to start the update. Allow the update to complete.
    Press 'r' to reset. The GNSS module should boot and respond to commands.

    All loaders should have similar structure:
    Given the web address of the binary to load,
    Do the WiFi stuff to begin reading the file data
    Put the target into bootload mode and malloc any necessary buffers xxxUpdateFirmwareBegin()
    Grab chunks of bytes over WiFi and throw at xxxUpdateFirmware(*data, length)
    When done, call xxxUpdateFirmwareEnd() to free buffers and exit the bootloader mode or reset the target

    Test procedure commands:
    1) o    ?.? --> 6.1
    2) a    6.1 --> 11.1
    3) e    11.1 --> 6.1    Connect to somewhere other than
                            raw.githubusercontent.com using http://
    4) e    6.1 --> 11.1    Connect to somewhere other than
                            raw.githubusercontent.com using https://
    5) L
       0    11.1 --> 11.4.1
    6) o    11.4.1 --> 6.1
    7) p    6.1 --> 11.1
    8) u    11.1 --> 11.4.1
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

// v11.4.1
const char * url_11_4_1 = "https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/main/imu/im19/20260522185649_VH2_B2.2_A11.4.1_131b44ecee0bdad5670c7.enc";

// v11.1
const char * url_11_1 = "https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/main/imu/im19/20260302210315_VH2_B2.2_A11.1_6bf04becee0bda310e65d.enc";

// v6.1
const char * url_6_1 = "https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/main/imu/im19/20230419111130_VH2_B2.2_A6.1_2eea4d4c024538bf5ed52.enc";

const char * ulrFileServer = "https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/main/imu/im19/";

const char * urlDirectory = "https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/tree/main/imu/im19";

char imuVersion[96];

static uint8_t rxBuffer[256];

uint32_t badBlocks1[] = {5, 15, 16, 17, 18, 19, 20, 36, 40, 89};
uint32_t badBlocks2[] = {5, 36, 40};
uint32_t * badBlocks;
uint32_t * badBlocksEnd;
uint32_t * nextBadBlocks;
uint32_t * nextBadBlocksEnd;
uint32_t * previousBadBlocks;
uint32_t * previousBadBlocksEnd;

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
    systemPrintln("IM19 firmware update example");

    // Configure the product
    if (productVariant == RTK_TORCH)
        imuReset();
    else if (productVariant == RTK_FACET_FP)
    {
        beginGpioExpanderSwitches();
        gpioExpanderSelectImu(); // On FP, confirm SW3 is in the correct position
    }
    else if (present.imu_im19)
        reportFatalError("Please add missing product configuration");
    else
        reportFatalError("An IM19 is not in this product");

    // Display the current firmware version
    im19GetVersionString();

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
    systemPrintln("a) Update IM19 to 11.1 from array");
    systemPrintln("o) Update IM19 to 6.1");
    systemPrintln("p) Update IM19 to 11.1");
    systemPrintln("u) Update IM19 to 11.4.1");
    systemPrintln("e) Enter URL");
    systemPrintln("L) List all versions");

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
                                                                 "_VH",
                                                                 ".enc",
                                                                 ulrFileServer);
            if (urlString.length() != 0)
            {
                wifiWaitUntilConnected();
                flashUpdate(urlString.c_str());
            }
        }
        else if (incoming == 'o')
            flashUpdate(url_6_1);
        else if (incoming == 'p')
            flashUpdate(url_11_1);
        else if (incoming == 'u')
            flashUpdate(url_11_4_1);

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

    // Test the retry mechanism
    badBlocks = &badBlocks1[0];
    badBlocksEnd = &badBlocks[sizeof(badBlocks1) / sizeof(badBlocks1[0])];
    nextBadBlocks = &badBlocks2[0];
    nextBadBlocksEnd = &badBlocks[sizeof(badBlocks2) / sizeof(badBlocks2[0])];

    // Attempt to update the firmware
    dataArray.init(0);
    if (((url != nullptr) && (im19FirmwareUpdate("IM19", url, rxBuffer, sizeof(rxBuffer)) == true))
        || ((url == nullptr) && im19ArrayFlashUpdate("IM19",
                                                     (NetworkClient *)&dataArray,
                                                     dataArray.available(),
                                                     rxBuffer,
                                                     sizeof(rxBuffer))))
    {
        // Stop timer and print elapsed time
        uint32_t flashUpdateElapsed = millis() - flashUpdateStartTime;
        systemPrint("Firmware update time: ");
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
