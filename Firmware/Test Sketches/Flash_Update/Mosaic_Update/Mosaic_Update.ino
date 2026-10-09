/*
    This example downloads a mosaic-X5 firmware (.suf) file from GitHub over WiFi and streams it
    to the module to perform a firmware update.

    This was written for FP hardware.

    The SUF file is large - ~22MB. At 460800bps, this takes ~8 minutes. This sketch demonstrates
    an update at 4MB/s (~1.3min update).

    To test: load this sketch onto an FP.
    Press '1' or '2' to start the update for a given firmware. Allow the update to complete.
    Press 'g' to reset the GNSS in case it gets partially loaded or frozen.
    Press 'r' to reset. The GNSS module should boot and respond to commands.
    Press 'b' to test different interface rates.

    All loaders should have similar structure:
    Given the web address of the binary to load,
    Do the WiFi stuff to begin reading the file data
    Put the target into bootload mode and malloc any necessary buffers xxxUpdateFirmwareBegin()
    Grab chunks of bytes over WiFi and throw at xxxUpdateFirmware(*data, length)
    When done, call xxxUpdateFirmwareEnd() to free buffers and exit the bootloader mode or reset the target
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

#include "secrets.h"
#include "settings.h"

#define rtkMalloc(bytes, description)       malloc(bytes)
#define rtkFree(buffer, description)        free(buffer)

//----------------------------------------
// Test specific declarations
//----------------------------------------

const char * subsystem = "GNSS";
const char * chip = "Mosaic-X5";

uint8_t rxBuffer[256];

const char * urlDirectory = "https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/tree/main/gnss/mosaic-x5";

const char * ulrFileServer = "https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/main/gnss/mosaic-x5/";

// 4.14.10.1
const char * url_4_14_10_1 = "https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/main/gnss/mosaic-x5/mosaic-X5-4.14.10.1.suf";

// 4.15.1
const char * url_4_15_1 = "https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/main/gnss/mosaic-x5/mosaic-X5-4.15.1.suf";

HardwareSerial * mosaicSerial;

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
    systemPrintln("=== mosaic-X5 Firmware Updater ===");
    if (productVariant == RTK_FACET_FP)
    {
        if (serialGNSS == nullptr)
            serialGNSS = new HardwareSerial(1);
        mosaicSerial = serialGNSS;

        // Larger RX/TX ring buffers mean fewer stalls waiting on interrupt
        // servicing during the ~22MB .suf transfer. Must be set before begin().
        serialGNSS->setRxBufferSize(1024 * 4);
        serialGNSS->setTxBufferSize(1024 * 4);

        serialGNSS->begin(115200 * 4, SERIAL_8N1, pin_GnssUart_RX, pin_GnssUart_TX);
        serialGNSS->setRxFIFOFull(50);
        beginGpioExpanderSwitches();
        gpioExpanderConnectGNSSToESP32(); // Connect Facet FP GNSS UART1 to ESP32 UART1
        systemPrintf("[%lums] Serial GNSS started\r\n", millis());
    }
    else if (productVariant == RTK_FACET_MOSAIC)
    {
        mosaicSerial = serial2GNSS;
        systemPrintln("mosaic-X5 firmware update over WiFi is not yet supported on Facet mosaic - see\r\n"
                     "mosaicFirmwareUpdatePort()'s comment. Use Test Sketches/Flash_Update/Mosaic_Update\r\n"
                     "connected directly to this unit, or the mosaic-X5's own web page over USB-C\r\n"
                     "(docs/firmware_update_mosaicX5.md), instead.");
        reportFatalError("Mosaic-X5 firmware update is not suspported on this platform");
    }
    else
        reportFatalError("This product's GNSS receiver is not a Mosaic-x5");

    printCommPortUsage();

    // Timestamped so a slow/failed boot shows exactly how long the module
    // took (or that it never answered) relative to power-on above, instead
    // of an unexplained gap that looks identical to a hang.
    unsigned long versionCheckStart = millis();
    bool versionOk = mosaicGetVersion(*mosaicSerial);
    systemPrintf("[%lums] Initial version check %s (took %lums)\r\n", millis(), versionOk ? "succeeded" : "FAILED",
                 millis() - versionCheckStart);

    // While COM4 is fresh off a proven-good connection, use it to pin down
    // COM1's baud too (see mosaicSyncUpdatePortBaud()) - avoids COM1's baud
    // being rediscovered by blind scanning later, when starting an update.
    // Only meaningful if COM4 itself is confirmed working, and only exists
    // as a separate port on Facet mosaic (FP's COM1 IS its command port, so
    // mosaicGetVersion() above already pinned its baud directly).
    if (versionOk && productVariant == RTK_FACET_MOSAIC)
    {
        unsigned long syncStart = millis();
        bool syncOk = mosaicSyncUpdatePortBaud(115200);
        systemPrintf("[%lums] COM1 baud pre-sync %s (took %lums)\r\n", millis(), syncOk ? "succeeded" : "failed",
                     millis() - syncStart);
    }

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
    systemPrintln("p) Update IM19 to 11.1");
    systemPrintln("u) Update IM19 to 11.4.1");
    systemPrintln("e) Enter URL");
    systemPrintln("L) List all versions");
    systemPrintf("b) Find max %s baud rate\r\n", mosaicUpdatePortName());
    systemPrintf("f) Get mosaic firmware version\r\n");
    systemPrintf("g) Reset GNSS\r\n");

    // Common menu items
    systemPrintln("r) Reboot system");
    systemPrintln("h) Display the heap");
    systemPrintf("d) Debug: %s\r\n", settings.debugFirmwareUpdate ? "Enabled" : "Disabled");
    systemPrintf("v) Verbose output: %s\r\n", otaDebugVerbose ? "Enabled" : "Disabled");

    // Discard any type ahead
    while (Serial.available())
        Serial.read();

    // Request user input
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
        else if (incoming == 'h')
            reportHeapNow(true);
        else if (incoming == 'v')
            otaDebugVerbose ^= 1;

        // Test specific menu items
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
            systemPrintln("Getting the list of files");

            // Get the SparkFun directory page
            urlString = serverSelectFileNameFromDirectoryListing(urlDirectory,
                                                                 otaFileTree,
                                                                 otaListEnd,
                                                                 otaItems,
                                                                 otaName,
                                                                 otaNameEnd,
                                                                 "mosaic-X5",
                                                                 ".suf",
                                                                 ulrFileServer);
            if (urlString.length() != 0)
            {
                wifiWaitUntilConnected();
                flashUpdate(urlString.c_str());
            }
        }
        else if (incoming == 'p')
            flashUpdate(url_4_14_10_1);
        else if (incoming == 'u')
            flashUpdate(url_4_15_1);
        else if (incoming == 'b')
        {
            mosaicFindMaxBaudRate(*mosaicUpdateSerial());
        }
        else if (incoming == 'f')
        {
            mosaicGetVersion(*mosaicSerial);
        }
        else if (incoming == 'g')
        {
            systemPrintln("Resetting GNSS");
            if (productVariant == RTK_FACET_FP)
            {
                gpioExpanderGnssReset();
                delay(250);
                gpioExpanderGnssBoot();
                delay(250);
            }
            else if (productVariant == RTK_FACET_MOSAIC)
            {
                // No separate reset line to the X5 - power-cycle the shared
                // peripheral rail instead (see pin_peripheralPowerControl in setup()).
                digitalWrite(pin_peripheralPowerControl, LOW);
                delay(500);
                digitalWrite(pin_peripheralPowerControl, HIGH);
                delay(1000);
                mosaicForceRescan(); // The module is booting from scratch; discard any cached baud
            }
        }

        // Display the menu again
        displayMenu();
    }
}

// Name of the physical mosaic COM port returned by mosaicCommandSerial().
// On Facet mosaic that's COM4; on FP it's COM1 (FP has no separate command
// channel - COM1 does everything).
const char *mosaicCommandPortName()
{
    return (productVariant == RTK_FACET_MOSAIC) ? "COM4" : "COM1";
}

// COM1 on both platforms: the mosaic-X5's dedicated high-throughput data
// port, and the ONLY one confirmed (on real hardware) to sustain baud rates
// above the 115200 default. Used for the entire firmware-update sequence
// (raise baud, trigger upgrade mode, stream the .suf file) - see mosaic.ino's
// mosaicEnterBootloaderMode()/mosaicStreamFirmware()/mosaicFinishUpdate().
//
// Raising COM4's baud instead (the previous design) produced pure line
// noise even at 921600 - different garbage on every identical retry, the
// signature of a real signal-integrity problem, not a protocol rejection.
// COM4 is wired as a low-speed ASCII console channel only; it was never
// meant to carry a 22MB binary transfer at multi-Mbps.
HardwareSerial *mosaicUpdateSerial()
{
    return serialGNSS;
}

const char *mosaicUpdatePortName()
{
    return "COM1";
}

// Print the actual port usage explicitly so the config/update split isn't
// hidden/assumed - and so it's obvious if that ever changes.
void printCommPortUsage()
{
    systemPrintln("COM port usage:");
    if (productVariant == RTK_FACET_MOSAIC)
    {
        systemPrintf("  COM1 (serialGNSS,  ESP32 RX%d/TX%d) @ %lu baud - used for firmware update (bootload trigger "
                     "+ SUF stream)\r\n",
                     13, 14, 460800UL);
        systemPrintf("  COM4 (serial2GNSS, ESP32 RX%d/TX%d) @ %lu baud - used for config commands (version check)\r\n",
                     pin_GnssUart2_RX, pin_GnssUart2_TX, 115200UL);
        systemPrintf("Config command port: %s\r\n", mosaicCommandPortName());
        systemPrintf("Firmware update port: %s (different from config - COM4 can't sustain the update's raised "
                     "baud rates; see mosaicUpdateSerial())\r\n",
                     mosaicUpdatePortName());
    }
    else
    {
        systemPrintf("  COM1 (serialGNSS, ESP32 RX%d/TX%d) @ %lu baud - used for config commands AND firmware "
                     "update\r\n",
                     pin_GnssUart_RX, pin_GnssUart_TX, (unsigned long)(115200 * 4));
        systemPrintf("Config command port: %s\r\n", mosaicCommandPortName());
        systemPrintf("Firmware update port: %s (same port as config - FP has only one)\r\n", mosaicUpdatePortName());
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
    if ((url != nullptr) && (mosaicFirmwareUpdate(subsystem,
                                                  chip,
                                                  url,
                                                  rxBuffer,
                                                  sizeof(rxBuffer)) == true))
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
}

bool voltageInRange(uint16_t voltage, uint16_t minimum, uint16_t maximum)
{
    return (voltage > minimum) && (voltage < maximum);
}
