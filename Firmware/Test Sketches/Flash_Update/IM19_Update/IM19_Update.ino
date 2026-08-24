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
*/

bool RTK_CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC = false; // Needed because of local BT TLS patch

#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "secrets.h"

// v11.4.1
const char * url_11_4_1 = "https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/main/imu/im19/20260522185649_VH2_B2.2_A11.4.1_131b44ecee0bdad5670c7.enc";

// v11.1
const char * url_11_1 = "https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/main/imu/im19/20260302210315_VH2_B2.2_A11.1_6bf04becee0bda310e65d.enc";

// v6.1
const char * url_6_1 = "https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/main/imu/im19/20230419111130_VH2_B2.2_A6.1_2eea4d4c024538bf5ed52.enc";

#define OTA_FIRMWARE_GITHUB_RAW "raw.githubusercontent.com"

#include "settings.h"

// Reports firmware update progress to the shared system callback.
void firmwareUpdateProgressCallback(uint16_t bytesProcessed);

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

HardwareSerial *uart2Serial; // Shared serial port between LoRa and Tilt

#define SerialForLoRa uart2Serial
#define SerialForTilt uart2Serial

int pin_muxA = -1;
int pin_muxB = -1;
int pin_GNSS_DR_Reset = 22; // Torch only. Push low to reset GNSS/DR
int pin_IMU_RX = 14;        // Pins used both on Torch and FP.
int pin_IMU_TX = 17;

// Timer for firmware update duration
unsigned long firmwareUpdateStartTime = 0;
unsigned long firmwareUpdateElapsed = 0;

// Global variables used by firmwareUpdateProgressCallback, called by all firmware update procedures
uint32_t firmwareUpdateBytesToProcess = 0;
uint32_t firmwareUpdateBytesProcessed = 0;
uint8_t firmwareUpdateLastPercent = 0;

char imuVersion[96];

bool otaDebugVerbose;

const char * otaEqualSigns = "==================================================";

#define OTA_DATA_TIMEOUT        (15 * 1000)

void setup()
{
    Serial.begin(115200);
    delay(250);

    systemPrintln("IM19 bootloader test over WiFi");

    Wire.begin(pin_SDA, pin_SCL);

    // Basic test to tell platform
    if (i2cIsDevicePresent(0x21))
    {
        systemPrintln("FP detected");
        productVariant = RTK_FACET_FP;
    }
    else
    {
        systemPrintln("Torch detected");
        productVariant = RTK_TORCH;
    }

    if (productVariant == RTK_TORCH)
    {
        pin_muxA = 18; // Controls U12 switch between ESP UART1 to UM980 UART3 or LoRa UART0
        pin_muxB = 12; // Controls U18 switch between ESP UART0 to LoRa UART2 or UM980 UART1
        pinMode(pin_muxA, OUTPUT);
        pinMode(pin_muxB, OUTPUT);

        pinMode(pin_GNSS_DR_Reset, OUTPUT);
        imuReset();
    }
    else if (productVariant == RTK_FACET_FP)
    {
        beginGpioExpanderSwitches();

        gpioExpanderSelectImu(); // On FP, confirm SW3 is in the correct position
    }
    else
    {
        Serial.println("Unknown product variant. Freezing...");
        while (true)
            delay(1000);
    }

    beginUart2Serial(); // Init the UART that communicates between the ESP32 and the IM19.

    systemPrint("Checking IM19 version: ");
    if (im19GetVersionString(imuVersion, sizeof(imuVersion)))
        systemPrintln(imuVersion);
    else
        systemPrintln("Version query failed.");

    wifiConnect();

    displayMenu();
}

void displayMenu()
{
    systemPrintln();
    systemPrintln("Menu:");
    systemPrintln("r) Reset");
    systemPrintln("u) Update Firmware");
    systemPrintf("d) Debug: %s\r\n", settings.debugFirmwareUpdate ? "Enabled" : "Disabled");
    systemPrintf("v) Verbose output: %s\r\n", otaDebugVerbose ? "Enabled" : "Disabled");
    systemPrint("Make selection: ");
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
        else if (incoming == 'd')
        {
            settings.debugFirmwareUpdate ^= 1;
            otaDebugVerbose = false;
        }
        else if (incoming == 'u')
        {
            // Start timer before erase
            firmwareUpdateStartTime = millis();

            // Attempt to update the firmware
            if (im19FirmwareUpdate((char *)url_11_4_1) == true)
            {
                firmwareUpdateElapsed = millis() - firmwareUpdateStartTime;
                systemPrintf("IM19 firmware update complete in %0.2f s.\r\n", firmwareUpdateElapsed / 1000.0);

                systemPrint("Checking IM19 version: ");
                if (im19GetVersionString(imuVersion, sizeof(imuVersion)))
                    systemPrintln(imuVersion);
                else
                    systemPrintln("Version query failed.");
            }
        }
        else if (incoming == 'v')
            otaDebugVerbose ^= 1;
        displayMenu();
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
