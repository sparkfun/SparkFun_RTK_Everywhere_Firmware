/*
    This example shows how to read a firmware file from microSD and send chunks to the IM19.

    Ported from the reference implementation in upgrade.c: a 268-byte framed
    protocol (0xAA55 header, 256-byte payload, uint32 checksum) used to push
    a firmware image to the IM19 module and confirm it booted the new image.

    This example is written for the ESP32 Thing Plus C:
    https://www.sparkfun.com/sparkfun-thing-plus-esp32-wroom-usb-c.html
    Connected to a IM19 Breakout:
    https://www.sparkfun.com/sparkfun-9dof-imu-breakout.html

    To test: load this sketch onto the ESP32 Thing Plus C.
    Place multiple IM19 firmware binaries in the root directory of the SD card.
    https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/tree/main/imu/im19
    E.g.:
    20230419111130_VH2_B2.2_A6.1_2eea4d4c024538bf5ed52.enc
    20260302210315_VH2_B2.2_A11.1_6bf04becee0bda310e65d.enc
    20260522185649_VH2_B2.2_A11.4.1_131b44ecee0bdad5670c7.enc

    Hardware Connections:
    Pull the IM19 Breakout CH342_EN pad low, to disconnect the CH342
    Connect TX1 of the IM19 to pin 16 of the ESP32
    Connect RX1 of the IM19 to pin 17 of the ESP32
    Connect IM19 RST to pin 4 of the ESP32
*/

#include "Tilt.h"

#include "SdFat.h"

// Adjust these values according to your configuration
//------------------------------------------------------------------------------
// https://www.sparkfun.com/sparkfun-thing-plus-esp32-wroom-usb-c.html
int pin_IMU_TX = 17;
int pin_IMU_RX = 16;
int pin_GNSS_DR_Reset = 4; // The Free pin
int pin_SCK = 18;
int pin_PICO = 23; // microSD SDI
int pin_POCI = 19; // microSD SDO
int pin_microSD_CS = 5;
const char * platform = "SparkFun ESP32 Thing Plus C";

HardwareSerial *uart2Serial;

#define SerialForTilt uart2Serial

#define SD_CONFIG SdSpiConfig(pin_microSD_CS, SHARED_SPI, SD_SCK_MHZ(16))
//#define SD_CONFIG SdSpiConfig(pin_microSD_CS, DEDICATED_SPI, SD_SCK_MHZ(16))

// SD_FAT_TYPE = 0 for SdFat/File as defined in SdFatConfig.h,
// 1 for FAT16/FAT32, 2 for exFAT, 3 for FAT16/FAT32 and exFAT.
#define SD_FAT_TYPE 3
#if SD_FAT_TYPE == 0
SdFat sd;
File dir;
File file;
#elif SD_FAT_TYPE == 1
SdFat32 sd;
File32 dir;
File32 file;
#elif SD_FAT_TYPE == 2
SdExFat sd;
ExFile dir;
ExFile file;
#elif SD_FAT_TYPE == 3
SdFs sd;
FsFile dir;
FsFile file;
#else  // SD_FAT_TYPE
#error invalid SD_FAT_TYPE
#endif  // SD_FAT_TYPE

// Reports firmware update progress to the shared system callback.
void firmwareUpdateProgressCallback(uint16_t bytesProcessed);

// Timer for firmware update duration
unsigned long firmwareUpdateStartTime = 0;
unsigned long firmwareUpdateElapsed = 0;

// Global variables used by firmwareUpdateProgressCallback, called by all firmware update procedures
uint32_t firmwareUpdateBytesToProcess = 0;
uint32_t firmwareUpdateBytesProcessed = 0;

char imuVersion[96];

char fileName[strlen("20260522185649_VH2_B2.2_A11.4.1_131b44ecee0bdad5670c7.enc") + 10];

void setup()
{
    pinMode(pin_GNSS_DR_Reset, OUTPUT);
    digitalWrite(pin_GNSS_DR_Reset, HIGH); // Keep GNSS/DR out of reset

    delay(1000);

    Serial.begin(115200);

    systemPrintln("IM19 firmware update test");
    systemFlush();

    SPI.begin(pin_SCK, pin_POCI, pin_PICO);

    if (sd.begin(SD_CONFIG) == false)
    {
        systemPrintln("Failed to start SD. Freezing...");
        while (1)
            ;
    }
    systemPrintln("SD started");

    beginUart2Serial(); // Init the UART that communicates between the ESP32 and the IM19.

    systemPrint("Checking IM19 version: ");
    if (im19GetVersionString(imuVersion, sizeof(imuVersion)))
        systemPrintln(imuVersion);
    else
        systemPrintln("Version query failed.");

    printMenu();
}

void loop()
{
    if (systemAvailable())
    {
        byte incoming = systemRead();
        if (incoming == 'r')
        {
            ESP.restart();
        }
        else if (incoming == 'u')
        {
            if (selectFirmwareFile())
            {
                systemPrintln("Starting IM19 firmware update...");

                firmwareUpdateBytesToProcess = 0;
                firmwareUpdateBytesProcessed = 0;
                firmwareUpdateStartTime = millis();

                if (im19StreamFirmwareFromFile((const char *)fileName) == true)
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

            printMenu();
        }
    }
}
