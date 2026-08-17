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
*/
#include "Tilt.h"

#include "SdFat.h"

// Adjust these values according to your configuration
//------------------------------------------------------------------------------
// https://www.sparkfun.com/sparkfun-thing-plus-esp32-wroom-usb-c.html
int pin_UART1_TX = 17;
int pin_UART1_RX = 16;
int pin_GNSS_DR_Reset = 4; // The Free pin
int pin_SCK = 18;
int pin_PICO = 23; // microSD SDI
int pin_POCI = 19; // microSD SDO
int pin_microSD_CS = 5;
const char * platform = "SparkFun ESP32 Thing Plus C";

HardwareSerial IMU_SERIAL(1); // Use UART1 on the ESP32

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

void printMenu()
{
    Serial.println();
    Serial.println("r) Reset ESP32");
    Serial.println("u) Update firmware with *.enc from the SD card root folder");
}

void setup()
{
    pinMode(pin_GNSS_DR_Reset, OUTPUT);
    digitalWrite(pin_GNSS_DR_Reset, HIGH); // Keep GNSS/DR out of reset

    delay(1000);

    Serial.begin(115200);

    Serial.println("IM19 firmware update test");
    Serial.flush();

    SPI.begin(pin_SCK, pin_POCI, pin_PICO);

    if (sd.begin(SD_CONFIG) == false)
    {
        Serial.println("Failed to start SD. Freezing...");
        while (1)
            ;
    }
    Serial.println("SD started");

    IMU_SERIAL.begin(115200, SERIAL_8N1, pin_UART1_RX, pin_UART1_TX);

    systemPrint("Checking IM19 version: ");
    if (im19GetVersionString(imuVersion, sizeof(imuVersion)))
        systemPrintln(imuVersion);
    else
        systemPrintln("Version query failed.");

    printMenu();
}

void loop()
{
    if (Serial.available())
    {
        byte incoming = Serial.read();
        if (incoming == 'r')
        {
            ESP.restart();
        }
        else if (incoming == 'u')
        {
            Serial.println("Select firmware file:");

            // Open root directory
            if (!dir.open("/")) {
                Serial.println("ERROR: dir.open failed");
                return;
            }

            // Open next file in root.
            // Warning, openNext starts at the current position of dir so a
            // rewind may be necessary in your application.
            bool fileSelected = false;
            while (file.openNext(&dir, O_RDONLY)) {
                if (!file.isDir()) {
                    char fileName[strlen("20260522185649_VH2_B2.2_A11.4.1_131b44ecee0bdad5670c7.enc") + 10];
                    file.getName(fileName, sizeof(fileName));
                    size_t fileNameLen = strlen(fileName);
                    //Serial.println(fileName);
                    if (fileNameLen > 10) {
                        if ((fileName[fileNameLen - 4] == '.') &&
                            ((fileName[fileNameLen - 3] == 'E') || (fileName[fileNameLen - 3] == 'e')) &&
                            ((fileName[fileNameLen - 2] == 'N') || (fileName[fileNameLen - 2] == 'n')) &&
                            ((fileName[fileNameLen - 1] == 'C') || (fileName[fileNameLen - 1] == 'c'))) {
                            Serial.print("Update with ");
                            Serial.print(fileName);
                            Serial.print(" (");
                            Serial.print(file.fileSize());
                            Serial.println(" bytes) (y/N)?");
                            do {
                                if (Serial.available())
                                {
                                    byte incoming = Serial.read();
                                    if ((incoming == 'Y') || (incoming == 'y'))
                                        fileSelected = true;
                                    break;
                                }
                            } while (1);
                        }
                    }
                }
                if (!fileSelected)
                    file.close();
                else
                    break;
            }

            if (!file)
            {
                Serial.println("ERROR: no firmware file found / selected");
                return;
            }

            systemPrintln("Starting IM19 firmware update...");

            // Start timer before erase
            firmwareUpdateStartTime = millis();

            if (im19UpdateFirmwareBegin() == false)
            {
                systemPrintln("Failed to enter update mode (AT+UPDATE_APP).");
                return;
            }
            systemPrintln("IM19 is in update mode, sending firmware...");

            firmwareUpdateBytesToProcess = file.fileSize();
            firmwareUpdateBytesProcessed = 0;

            // Frames already acknowledged by the
            // IM19 (tracked in im19FrameMap) are skipped rather than resent.
            bool updateSuccess = false;
            bool uploadFailed = false;
            int passesLeft = 5;
            while (passesLeft > 0)
            {
                uint32_t blobIndex = 0;
                while (blobIndex < file.fileSize())
                {
                    // Test with 17-byte sized chunks
                    uint32_t chunk = min((uint32_t)17, (uint32_t)(file.fileSize() - blobIndex));
                    uint8_t chunkStore[chunk];
                    size_t n = file.read(chunkStore, chunk);
                    if (im19UpdateFirmware(chunkStore, n) == false)
                    {
                        systemPrintln("Firmware update failed during data upload.");
                        uploadFailed = true;
                        break;
                    }
                    blobIndex += n;
                }
                if (uploadFailed)
                    break;

                int result = im19UpdateFirmwareEndPass();
                passesLeft--;
                if (result == IM19_UPDATE_SUCCESS)
                {
                    updateSuccess = true;
                    break;
                }
                else if (result == IM19_UPDATE_FAILED)
                {
                    break;
                }
                // else IM19_UPDATE_RETRY: loop and re-stream the array
            }

            im19UpdateFirmwareEnd();

            file.close();

            if (updateSuccess)
                systemPrintln("Upgrade completed successfully.");
            else
                systemPrintln("Upgrade failed.");

            if (updateSuccess)
            {
                systemPrint("Checking IM19 version: ");
                if (im19GetVersionString(imuVersion, sizeof(imuVersion)))
                    systemPrintln(imuVersion);
                else
                    systemPrintln("Version query failed.");
            }

            // Stop timer and print elapsed time
            firmwareUpdateElapsed = millis() - firmwareUpdateStartTime;
            systemPrint("Firmware update time: ");
            systemPrint(firmwareUpdateElapsed / 1000.0, 3);
            systemPrintln(" seconds");

            printMenu();
        }
    }
}
