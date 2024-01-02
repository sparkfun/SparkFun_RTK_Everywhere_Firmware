/*
  Demonstrates reading an SD card and showing files on card.

  Uses semaphores to prevent hardware access collisions.
*/

#include "settings.h"

#define ASCII_LF      0x0a
#define ASCII_CR      0x0d

const int pin_microSD_CS = 4;
const int pin_SCK = 18;
const int pin_MISO = 19;
const int pin_MOSI = 23;
const int pin_peripheralPowerControl = 32;
const int pin_microSD_CardDetect = 36;

//microSD Interface
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#include <SPI.h>
#include "SdFat.h" //http://librarymanager/All#sdfat_exfat by Bill Greiman. Currently uses v2.1.1

SdFat sd;

char platformFilePrefix[40] = "SFE_Surveyor"; //Sets the prefix for logs and settings files

SdFile ubxFile; //File that all gnss ubx messages setences are written to
unsigned long lastUBXLogSyncTime = 0; //Used to record to SD every half second
int startLogTime_minutes = 0; //Mark when we start any logging so we can stop logging after maxLogTime_minutes
int startCurrentLogTime_minutes = 0; //Mark when we start this specific log file so we can close it after x minutes and start a new one

SdFile newFirmwareFile; //File that is available if user uploads new firmware via web gui

//System crashes if two tasks access a file at the same time
//So we use a semaphore to see if file system is available
SemaphoreHandle_t xFATSemaphore;
const TickType_t fatSemaphore_shortWait_ms = 10 / portTICK_PERIOD_MS;
const TickType_t fatSemaphore_longWait_ms = 200 / portTICK_PERIOD_MS;

//Display used/free space in menu and config page
uint32_t sdCardSizeMB = 0;
uint32_t sdFreeSpaceMB = 0;
uint32_t sdUsedSpaceMB = 0;

char filename[1024];
uint8_t buffer[5701];

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

void setup()
{
  Serial.begin(115200);

  Serial.println();
  Serial.println();

  reportHeapNow();
  if (psramInit())
  {
    Serial.printf("%d MB of PSRAM detected\r\n", (ESP.getFreePsram() + (512 * 1024)) / (1024 * 1024));
    online.psram = true;
    reportHeapNow();
  }
  else
    Serial.println("PSRAM is not available!");

  Serial.printf("pin_peripheralPowerControl: %d\r\n", pin_peripheralPowerControl);
  pinMode(pin_peripheralPowerControl, OUTPUT);
  digitalWrite(pin_peripheralPowerControl, HIGH);

  // Disable the microSD card
  Serial.printf("pin_microSD_CardDetect: %d\r\n", pin_microSD_CardDetect);
  pinMode(pin_microSD_CardDetect, INPUT); // Internal pullups not supported on input only pins

  Serial.printf("pin_microSD_CS: %d\r\n", pin_microSD_CS);
  pinMode(pin_microSD_CS, OUTPUT);
  digitalWrite(pin_microSD_CS, HIGH);

  delay(1000);
  Serial.println("SD File Listing");

  beginSD();

  if (online.microSD == false)
    Serial.println("SD not detected. Please check card. Is it formatted?");
  else
    sd.ls(LS_R | LS_DATE | LS_SIZE); //Print files on card
  if (online.psram)
    reportHeapNow();
}

void loop()
{
  int bytesRead;
  int bytesToRead;
  char data;
  int length;
  SdFile sdFile;
  SdFile sdRootDir;

  // Read the filename
  Serial.println("\nPlease enter the filename:");
  length = 0;
  do {
    while (!Serial.available());
    data = Serial.read();
    if ((data == ASCII_LF) || (data == ASCII_CR))
      break;
    filename[length++] = data;
  } while (1);
  filename[length] = 0;

  // Skip reading the SD card if no filename is specified
  if (length) {
    Serial.printf("filename: %s\n", filename);
    do {

      // Attempt to open the root directory
      Serial.println("Attempting to open the root directory");
      sdRootDir = SdFile();
      if (!sdRootDir.openRoot(sd.vol())) {
          Serial.println("ERROR - Failed to open root directory!");
          break;
      }

      // Attempt to open the file
      Serial.printf("Attempting to open file %s\n", filename);
      if (!sdFile.open(&sdRootDir, filename, O_RDONLY)) {
        // File not found
        Serial.println("ERROR - File not found!");
        sdRootDir.close();
        break;
      }
      Serial.printf("File %s opened successfully!\n", filename);
      if (online.psram)
        reportHeapNow();

      // Close the root directory
      Serial.println("Closing the root directory");
      sdRootDir.close();
      if (online.psram)
        reportHeapNow();

      // Read the file
      do {

        // Read data from the file
        bytesToRead = sizeof(buffer);
        Serial.printf("Attempting to read %d bytes from %s\n", bytesToRead, filename);
        bytesRead = sdFile.read(buffer, bytesToRead);
        Serial.printf("bytesRead: %d\n", bytesRead);
        if (online.psram)
          reportHeapNow();
      } while (bytesRead > 0);

      // Close the file
      Serial.printf("Closing %s\n", filename);
      sdFile.close();
      if (online.psram)
        reportHeapNow();
      Serial.println();
    } while (0);
  }

  // Wait for user to confirm reset
  Serial.println("Press a key to reset");

  while (!Serial.available());
  ESP.restart();
}

void reportHeapNow()
{
    Serial.printf("FreeHeap: %d / HeapLowestPoint: %d / LargestBlock: %d\r\n", ESP.getFreeHeap(),
                 xPortGetMinimumEverFreeHeapSize(), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    if (online.psram)
        Serial.printf("Free PSRAM: %d\r\n", ESP.getFreePsram());
}
