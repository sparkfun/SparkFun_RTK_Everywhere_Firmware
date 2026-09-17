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

bool RTK_CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC = false; // Needed because of local BT TLS patch

#include "settings.h"

#include "secrets.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

const char *firmwareURL_4_14_10_1 = "/gnss/mosaic-x5/mosaic-X5-4.14.10.1.suf";
const char *firmwareURL_4_15_1 = "/gnss/mosaic-x5/mosaic-X5-4.15.1.suf";

#define OTA_FIRMWARE_GITHUB_RAW "raw.githubusercontent.com"

#include <SparkFun_I2C_Expander_Arduino_Library.h> // Click here to get the library: http://librarymanager/All#SparkFun_I2C_Expander_Arduino_Library
SFE_PCA95XX io(PCA95XX_PCA9534); // Create a PCA9534
SFE_PCA95XX *gpioExpanderSwitches = nullptr;

int pin_SDA = 15;
int pin_SCL = 4;

const int gpioExpanderSwitch_S1 = 0; // Controls U16 switch 1: connect ESP UART0 to CH342 or SW2
const int gpioExpanderSwitch_S2 = 1; // Controls U17 switch 2: connect SW1 to RS232 Output or GNSS UART4
const int gpioExpanderSwitch_S3 = 2; // Controls U18 switch 3: connect ESP UART2 to GNSS UART3 or LoRa UART2
const int gpioExpanderSwitch_S4 = 3; // Controls U19 switch 4: connect GNSS UART2 to 4-pin JST TTL Serial or LoRa UART0
const int gpioExpanderSwitch_LoraEnable = 4; // LoRa_EN
const int gpioExpanderSwitch_GNSS_Reset = 5; // RST_GNSS
const int gpioExpanderSwitch_LoraBoot = 6;   // LoRa_BOOT0 - Used for bootloading the STM32 radio IC
const int gpioExpanderSwitch_S5 = 7;         // Controls U61 switch 5: connect GNSS UART1 to Port A of CH342
const int gpioExpanderNumSwitches = 8;

// Communication Ports
HardwareSerial *serialGNSS = nullptr;  // UART1: FP GNSS or Facet mosaic COM1
HardwareSerial *serial2GNSS = nullptr; // UART2: Facet mosaic COM4

int pin_UART1_TX = 27; // FP
int pin_UART1_RX = 26;
int pin_UART2_TX = 25; // Facet mosaic COM4
int pin_UART2_RX = 4;

// On Facet mosaic, the mosaic-X5 (and OLED) sit behind a switched power rail.
// Confirmed in production Begin.ino: this pin must be driven HIGH (peripheralsOn())
// before the module will respond to anything - without it, COM1/COM4 are silent
// at every baud rate because the X5 itself has no power.
int pin_peripheralPowerControl = 27; // Facet mosaic only. NOTE: shared number with pin_UART1_TX (FP); never both.

// Timer for firmware update duration
unsigned long firmwareUpdateStartTime = 0;
unsigned long firmwareUpdateElapsed = 0;

// Global variables used by firmwareUpdateProgressCallback, called by all firmware update procedures
uint32_t firmwareUpdateBytesToProcess = 0;
uint32_t firmwareUpdateBytesProcessed = 0;
uint8_t firmwareUpdateLastPercent = 0;

HardwareSerial *mosaicCommandSerial();

bool voltageInRange(uint16_t voltage, uint16_t minimum, uint16_t maximum)
{
    return (voltage > minimum) && (voltage < maximum);
}

void setup()
{
    Serial.begin(115200);
    delay(250);

    systemPrintln("=== mosaic-X5 Firmware Updater ===");

    // Use the production board ID resistor first. This does not depend on the
    // display or GPIO expander being present on the I2C bus.
    uint16_t boardIdMillivolts = analogReadMilliVolts(35);
    boardIdMillivolts = analogReadMilliVolts(35);
    systemPrintf("Board ID: %u mV\r\n", boardIdMillivolts);

    if (voltageInRange(boardIdMillivolts, 2618, 2811))
    {
        systemPrintln("Facet mosaic detected by board ID");
        productVariant = RTK_FACET_MOSAIC;
    }
    else if (voltageInRange(boardIdMillivolts, 2071, 2322))
    {
        systemPrintln("FP detected by board ID");
        productVariant = RTK_FACET_FP;
    }
    else
    {
        // Fall back to board-specific I2C devices for older hardware without
        // a usable ADC reading.
        Wire.begin(15, 4); // Facet FP SDA, SCL
        if (i2cIsDevicePresent(0x21))
        {
            systemPrintln("FP detected by I2C");
            productVariant = RTK_FACET_FP;
        }
        else
        {
            Wire.begin(21, 22); // Facet mosaic SDA, SCL
            if (i2cIsDevicePresent(0x3D))
            {
                systemPrintln("Facet mosaic detected by I2C");
                productVariant = RTK_FACET_MOSAIC;
            }
        }
    }

    if (productVariant == RTK_UNKNOWN)
    {
        systemPrintln("Unknown platform. Freezing...");
        while (1)
            ;
    }

    if (productVariant == RTK_FACET_MOSAIC)
    {
        // Turn on power to the mosaic-X5 and OLED. Without this the module is
        // completely unpowered and will never respond on COM1/COM4 at any baud.
        pinMode(pin_peripheralPowerControl, OUTPUT);
        digitalWrite(pin_peripheralPowerControl, HIGH);
        systemPrintf("[%lums] Peripheral power on, waiting 860ms for the rail to stabilize...\r\n", millis());
        delay(860); // Matches production Begin.ino's peripheralsOn() exactly - already validated on real hardware

        Wire.begin(21, 22); // Facet mosaic SDA, SCL
    }
    else
        Wire.begin(15, 4); // Facet FP SDA, SCL

    if (serialGNSS == nullptr)
        serialGNSS = new HardwareSerial(1);

    // Larger RX/TX ring buffers mean fewer stalls waiting on interrupt
    // servicing during the ~22MB .suf transfer. Must be set before begin().
    serialGNSS->setRxBufferSize(1024 * 4);
    serialGNSS->setTxBufferSize(1024 * 4);

    if (productVariant == RTK_FACET_MOSAIC)
    {
        serialGNSS->begin(460800, SERIAL_8N1, 13, 14); // X5 COM1
        serial2GNSS = new HardwareSerial(2);
        serial2GNSS->setRxBufferSize(1024 * 4);
        serial2GNSS->setTxBufferSize(1024 * 4);
        serial2GNSS->begin(115200, SERIAL_8N1, pin_UART2_RX, pin_UART2_TX); // X5 COM4
        serial2GNSS->setRxFIFOFull(50);
        systemPrintf("[%lums] Facet mosaic COM1 and COM4 started\r\n", millis());
    }
    else
    {
        serialGNSS->begin(115200 * 4, SERIAL_8N1, pin_UART1_RX, pin_UART1_TX);
        serialGNSS->setRxFIFOFull(50);
        beginGpioExpanderSwitches();
        gpioExpanderConnectGNSSToESP32(); // Connect Facet FP GNSS UART1 to ESP32 UART1
        systemPrintf("[%lums] Serial GNSS started\r\n", millis());
    }

    printCommPortUsage();

    // Timestamped so a slow/failed boot shows exactly how long the module
    // took (or that it never answered) relative to power-on above, instead
    // of an unexplained gap that looks identical to a hang.
    unsigned long versionCheckStart = millis();
    bool versionOk = mosaicGetVersion(*mosaicCommandSerial());
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

    wifiConnect();

    displayMenu();
}

void loop()
{
    if (Serial.available())
    {
        byte incoming = Serial.read();
        Serial.printf("%c\r\n", incoming);
        if (incoming == 'r')
        {
            ESP.restart();
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
        else if (incoming == '1' || incoming == '2')
        {
            const char *firmwareURL = firmwareURL_4_14_10_1;
            const char *firmwareVersion = "4.14.10.1";
            if (incoming == '2')
            {
                firmwareURL = firmwareURL_4_15_1;
                firmwareVersion = "4.15.1";
            }

            updateFirmware(firmwareURL, firmwareVersion);
        }
        else if (incoming == 'v')
        {
            mosaicGetVersion(*mosaicCommandSerial());
        }
        else if (incoming == 'b')
        {
            mosaicFindMaxBaudRate(*mosaicUpdateSerial());
        }
        displayMenu();
    }
}

void displayMenu()
{
    systemPrintln();
    systemPrintln("Menu:");
    systemPrintf("g) Reset GNSS\r\n");
    systemPrintf("1) Load mosaic-X5 v4.14.10.1\r\n");
    systemPrintf("2) Load mosaic-X5 v4.15.1\r\n");
    systemPrintf("v) Get mosaic firmware version\r\n");
    systemPrintf("b) Find max %s baud rate\r\n", mosaicUpdatePortName());
    systemPrintf("r) Reboot system\r\n");
    systemPrint("Selection: ");
}

void updateFirmware(const char *firmwareURL, const char *firmwareVersion)
{
    systemPrintf("Loading mosaic-X5 v%s...\r\n", firmwareVersion);

    // Start timer before erase
    firmwareUpdateStartTime = millis();

    if (mosaicStreamFirmware(firmwareURL) == true)
        systemPrintln("mosaic-X5 updated successfully.");
    else
        systemPrintln("mosaic-X5 update failed.");

    // Stop timer and print elapsed time
    firmwareUpdateElapsed = millis() - firmwareUpdateStartTime;
    systemPrint("Firmware update time: ");
    systemPrint(firmwareUpdateElapsed / 1000.0, 3);
    systemPrintln(" seconds");

    // Bootload always ends with a reboot into the module's normal operating
    // baud rate, regardless of whether the update itself succeeded.
    mosaicFinishUpdate(*mosaicUpdateSerial());
}

// COM4 on Facet mosaic (mosaicCommandSerial() below): a low-speed ASCII
// console channel used for a quick, reliable initial version check.
HardwareSerial *mosaicCommandSerial()
{
    return (productVariant == RTK_FACET_MOSAIC) ? serial2GNSS : serialGNSS;
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
                     pin_UART2_RX, pin_UART2_TX, 115200UL);
        systemPrintf("Config command port: %s\r\n", mosaicCommandPortName());
        systemPrintf("Firmware update port: %s (different from config - COM4 can't sustain the update's raised "
                     "baud rates; see mosaicUpdateSerial())\r\n",
                     mosaicUpdatePortName());
    }
    else
    {
        systemPrintf("  COM1 (serialGNSS, ESP32 RX%d/TX%d) @ %lu baud - used for config commands AND firmware "
                     "update\r\n",
                     pin_UART1_RX, pin_UART1_TX, (unsigned long)(115200 * 4));
        systemPrintf("Config command port: %s\r\n", mosaicCommandPortName());
        systemPrintf("Firmware update port: %s (same port as config - FP has only one)\r\n", mosaicUpdatePortName());
    }
}

// Connects to the configured SSID and blocks until connected or the attempt times out.
bool wifiConnect()
{
    systemPrint("Connecting to WiFi SSID: ");
    systemPrintln(wifiSSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSSID, wifiPassword);

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

    return true;
}
