/*
    This example shows how to read a raw firmware binary from an array and send chunks to the STM32WL.
    The goal is to eventually read from WiFi so we intentionally treat the data as an opaque byte
    stream rather than a HEX file - the array is written to Flash starting at 0x08000000, contiguously.

    This was written for FP hardware but should be adaptable to the Torch.

    To test: load this sketch onto an FP.
    Press 'u' to start the update. Allow the update to complete.
    Load RTK Everywhere and put the device into STM32 passthrough mode.
    Use STM32CubeProgrammer to read the flash and compare it against the contents of 'SparkPNT_LoRa_3.0.1.bin'.
    Files should be identical.

    All loaders should have similar structure:
    Given the web address of the binary to load,
    Do the WiFi stuff to begin reading the file data
    Put the target into bootload mode and malloc any necessary buffers xxxUpdateFirmwareBegin()
    Grab chunks of bytes over WiFi and throw at xxxUpdateFirmware(*data, length)
    When done, call xxxUpdateFirmwareEnd() to free buffers and exit the bootloader mode or reset the target
*/

#define COMPILE_ALL_FIRMWARE // Comment this out to test with a smaller firmware blob

#include "TheData.h" //Array containing the raw binary firmware image

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

// Communication Port
HardwareSerial SerialForLoRa(2);
#define pin_IMU_TX 17
#define pin_IMU_RX 14
const int loraBaud = 115200; // Increasing the baud rate does not decrease the programming time. Programming time is
                             // likely limited by STM32's internal flash write time.

// External GPIO functions (provided by your hardware abstraction)
extern void gpioExpanderLoraBootEnable();
extern void gpioExpanderLoraEnable();
extern void gpioExpanderLoraDisable();

// Timer for firmware update duration
unsigned long firmwareUpdateStartTime = 0;
unsigned long firmwareUpdateElapsed = 0;

// Global variables used by firmwareUpdateProgressCallback, called by all firmware update procedures
uint32_t firmwareUpdateBytesToProcess = 0;
uint32_t firmwareUpdateBytesProcessed = 0;

void setup()
{
    Serial.begin(115200);
    delay(250);

    systemPrintln("STM32 bootloader test");

    Wire.begin(pin_SDA, pin_SCL);

    beginGpioExpanderSwitches();

    SerialForLoRa.begin(loraBaud, SERIAL_8E1, pin_IMU_RX, pin_IMU_TX); // STM32 bootloader requires Even parity

    // Connect ESP32 UART2 to LoRa UART2 via SW3 for configuration and bootloading/firmware updates
    gpioExpanderSelectLoraConfigure();

    systemPrintln("Serial LoRa started");
    displayMenu();
}

void displayMenu()
{
    systemPrintln();
    systemPrintln("Menu:");
    systemPrintln("r) Reset");
    systemPrintln("u) Update Firmware");
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
        else if (incoming == 'u')
        {
            systemPrintln("Starting firmware update...");

            // Start timer before erase
            firmwareUpdateStartTime = millis();

            stm32UpdateFirmwareBegin();

            systemPrintln("Loading new firmware...");

            // We will eventually be given bytes over WiFi, so feed the update state machine
            // in fixed-size chunks rather than requiring the whole blob at once.
            const uint16_t chunkSize = 256;
            uint32_t blobIndex = 0;

            while (blobIndex < sizeof(lora_firmware_3_0_1))
            {
                uint32_t bytesRemaining = sizeof(lora_firmware_3_0_1) - blobIndex;
                uint16_t bytesToSend = (bytesRemaining < chunkSize) ? bytesRemaining : chunkSize;

                stm32UpdateFirmware((uint8_t *)&lora_firmware_3_0_1[blobIndex], bytesToSend);

                blobIndex += bytesToSend;
            }

            if (stm32UpdateFirmwareEnd())
                systemPrintln("LoRa/STM32 updated successfully.");
            else
                systemPrintln("LoRa/STM32 update failed.");

            // Stop timer and print elapsed time
            firmwareUpdateElapsed = millis() - firmwareUpdateStartTime;
            systemPrint("Firmware update time: ");
            systemPrint(firmwareUpdateElapsed / 1000.0, 3);
            systemPrintln(" seconds");
        }
        displayMenu();
    }
}

