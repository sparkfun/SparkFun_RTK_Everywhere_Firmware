/*
    This example shows how to read a firmware file from an array and send chunks to the LG290P.
    The goal is to eventually read from WiFi.

    This was written for FP and TX2 hardware.

    To test: load this sketch onto an FP or TX2.
    Press 'u' to start the update. Allow the update to complete.
    Press 'r' to reset. The GNSS module should boot and respond to commands.

    If the update fails, the LG290P must be hardware reset or power reset, at which time
    it will enter into a state where it can enter bootloader mode again. On the TX2, there is
    hardware reset. On the FP, the user may need to power cycle the device and restart the update.

    All loaders should have similar structure:
    Given the web address of the binary to load,
    Do the WiFi stuff to begin reading the file data
    Put the target into bootload mode and malloc any necessary buffers xxxUpdateFirmwareBegin()
    Grab chunks of bytes over WiFi and throw at xxxUpdateFirmware(*data, length)
    When done, call xxxUpdateFirmwareEnd() to free buffers and exit the bootloader mode or reset the target
*/

#define COMPILE_ALL_FIRMWARE // Comment this out to test with a smaller firmware blob

#include "TheData.h" //Array containing the PKG data

//#define PLATFORM_FP
#define PLATFORM_POSTCARD
// #define PLATFORM_TX2

#include <SparkFun_LG290P_GNSS.h>
LG290P myGnss;

#include <SparkFun_I2C_Expander_Arduino_Library.h> // Click here to get the library: http://librarymanager/All#SparkFun_I2C_Expander_Arduino_Library
SFE_PCA95XX io(PCA95XX_PCA9534); // Create a PCA9534
SFE_PCA95XX *gpioExpanderSwitches = nullptr;

const int gpioExpanderSwitch_S1 = 0; // Controls U16 switch 1: connect ESP UART0 to CH342 or SW2
const int gpioExpanderSwitch_S2 = 1; // Controls U17 switch 2: connect SW1 to RS232 Output or GNSS UART4
const int gpioExpanderSwitch_S3 = 2; // Controls U18 switch 3: connect ESP UART2 to GNSS UART3 or LoRa UART2
const int gpioExpanderSwitch_S4 = 3; // Controls U19 switch 4: connect GNSS UART2 to 4-pin JST TTL Serial or LoRa UART0
const int gpioExpanderSwitch_LoraEnable = 4; // LoRa_EN
const int gpioExpanderSwitch_GNSS_Reset = 5; // RST_GNSS
const int gpioExpanderSwitch_LoraBoot = 6;   // LoRa_BOOT0 - Used for bootloading the STM32 radio IC
const int gpioExpanderSwitch_S5 = 7;         // Controls U61 switch 5: connect GNSS UART1 to Port A of CH342
const int gpioExpanderNumSwitches = 8;

// Communication Port
HardwareSerial SerialGNSS(1); // Use UART1 on the ESP32

#ifdef PLATFORM_FP
int pin_SDA = 15;
int pin_SCL = 4;
int pin_UART1_TX = 27; // FP
int pin_UART1_RX = 26;
#elif defined(PLATFORM_POSTCARD)
int pin_SDA = 7;
int pin_SCL = 20;
int pin_UART1_TX = 22; // TX2
int pin_UART1_RX = 21;
int pin_GNSS_DR_Reset = 33; // Push low to reset GNSS/DR.
#elif defined(PLATFORM_TX2)
int pin_SDA = 15;
int pin_SCL = 4;
int pin_UART1_TX = 17; // TX2
int pin_UART1_RX = 14;
int pin_GNSS_DR_Reset = 22; // Push low to reset GNSS/DR.
#endif

int gnss_baud = 460800; // Baud rate for GNSS module

// External GPIO functions
extern void gpioExpanderConnectGNSSToESP32();

// Timer for firmware update duration
unsigned long firmwareUpdateStartTime = 0;
unsigned long firmwareUpdateElapsed = 0;

// Global variables used by firmwareUpdateProgressCallback, called by all firmware update procedures
uint32_t firmwareUpdateBytesToProcess = 0;
uint32_t firmwareUpdateBytesProcessed = 0;

// To be removed / obtained from JSON file in the future
uint32_t fileSize;
uint32_t crc;

void setup()
{
    Serial.begin(115200);

    Serial.println("LG290P bootloader test");

    delay(250);

    Wire.begin(pin_SDA, pin_SCL);

    SerialGNSS.begin(gnss_baud, SERIAL_8N1, pin_UART1_RX, pin_UART1_TX);
    Serial.println("Serial GNSS started");

    Serial.printf("Starting connection to GNSS module at %d baud...\n\r", gnss_baud);

#ifdef PLATFORM_FP
    beginGpioExpanderSwitches();

    // Connect Facet FP GNSS receiver UART1 to ESP32 UART1 for normal comms
    gpioExpanderConnectGNSSToESP32();
#elif defined(PLATFORM_POSTCARD)
    pinMode(pin_GNSS_DR_Reset, OUTPUT);
#elif defined(PLATFORM_TX2)
    pinMode(pin_GNSS_DR_Reset, OUTPUT);
#endif

    gnssBoot();

    delay(1000);

    myGnss.enableDebugging(Serial); // Enable debugging to get more info during the update process
    if (myGnss.begin(SerialGNSS, "LG290P") == true)
        Serial.println("GNSS module found");
    else
        Serial.println(
            "Failed to find GNSS module. It may have been damaged by a previous failed update attempt. Proceeding.");

    // In the future, file size and CRC will come from the JSON file in the repo
    // For now, compute the CRC manually

    fileSize = sizeof(lg290_firmware);
    firmwareUpdateBytesToProcess = fileSize;
    firmwareUpdateBytesProcessed = 0;

    Serial.printf("Firmware file: %u bytes\r\n", fileSize);

    Serial.println("Calculating CRC32 of firmware file...");

    crc = LG290P::initFirmwareCrc32(fileSize);
    uint8_t chunk[512];
    size_t offset = 0;
    while (offset < sizeof(lg290_firmware))
    {
        size_t n = sizeof(lg290_firmware) - offset;
        if (n > sizeof(chunk))
            n = sizeof(chunk);

        memcpy(chunk, lg290_firmware + offset, n);
        crc = LG290P::computeFirmwareCrc32(crc, chunk, n);
        offset += n;
    }
    Serial.printf("CRC32: 0x%08X\r\n", crc);
    displayMenu();
}

void displayMenu()
{
    Serial.println();
    Serial.println("r) Restart the ESP32");
    Serial.println("u) Update the LG290P firmware");
    Serial.print("Selection: ");
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
        else if (incoming == 'u')
        {
            Serial.println("Starting LG290P firmware update...");

            // Start timer before erase
            firmwareUpdateStartTime = millis();

            if (lg290pFirmwareUpdateBegin() == true)
                Serial.println("Device is in bootloader mode.");
            else
            {
                Serial.println("Failed to enter bootloader mode.");
                displayMenu();
                return;
            }

            // We will be given bytes over WiFi so we need to parse the incoming
            // stream to look for starting HEX lines and extract the data from those lines to send to

            uint32_t blobIndex = 0;
            while (blobIndex < sizeof(lg290_firmware))
            {
                uint8_t c = lg290_firmware[blobIndex++];

                if (lg290pFirmwareUpdate(&c, 1, false) == false)
                {
                    Serial.println("Firmware update failed during data upload.");
                    displayMenu();
                    return;
                }
            }

            Serial.print("Sending last packet. Device will then take up to 30 seconds to verify and reboot... ");
            if (lg290pFirmwareUpdate(NULL, 0, true) == true) // Send a final call to flush any remaining buffered data
                Serial.println("OK");
            else
            {
                Serial.println("FAILED");
                displayMenu();
                return;
            }

            Serial.print("Waiting up to 25 seconds for device to boot. updateFirmwareIsFinished: ");
            if (lg290pFirmwareUpdateEnd() == true) // Clean up and reset
                Serial.println("LG290 updated successfully.");
            else
            {
                Serial.println("LG290P update failed.");
                displayMenu();
                return;
            }

            // Stop timer and print elapsed time
            firmwareUpdateElapsed = millis() - firmwareUpdateStartTime;
            Serial.print("Firmware update time: ");
            Serial.print(firmwareUpdateElapsed / 1000.0, 3);
            Serial.println(" seconds");
            displayMenu();
        }
    }
}
