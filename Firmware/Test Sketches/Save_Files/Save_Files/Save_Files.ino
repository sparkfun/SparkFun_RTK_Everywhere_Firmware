/*
  Save_Files.ino

  Work with the Save_Files program on the PC to save the files from the
  spiffs partition in NVM formatted as LittleFS and the configuration
  files from the SD card.  The following messages are exchanged:

    .----------.                                             .-------.
    |    PC    |                                             | ESP32 |
    |          |                 Hello_ESP32                 |       |
    |    PC    |-------------------------------------------->| ESP32 |
    |          |<--------------------------------------------|       |
    |          |                  Hello_PC                   |       |
    |          |                                             |       |
    |          |                                             |       |
    |          |                  Open_LFS                   |       |
    |    PC    |-------------------------------------------->| ESP32 |
    |          |<--------------------------------------------|       |
    |          |                  LFS_Open                   |       |
    |          |                                             |       |
    |          |                                             |       |
    |          |                Save_LFS_File                |       |
    |    PC    |-------------------------------------------->| ESP32 |
    |          |<--------------------------------------------|       |
    |          |               First File Name               |       |
    |          |                                             |       |
    |          |                                             |       |
    |          |                Save_LFS_File                |       |
    |    PC    |-------------------------------------------->| ESP32 |
                                    * * *
    |          |<--------------------------------------------|       |
    |          |               Last File Name                |       |
    |          |                                             |       |
    |          |                                             |       |
    |          |                Save_LFS_File                |       |
    |    PC    |-------------------------------------------->| ESP32 |
    |          |<--------------------------------------------|       |
    |          |              LFS_No_More_Files              |       |
    |          |                                             |       |
    |          |                                             |       |
    |          |                  Close_LFS                  |       |
    |    PC    |-------------------------------------------->| ESP32 |
    |          |<--------------------------------------------|       |
    |          |                 LFS_Closed                  |       |
    |          |                                             |       |
    |          |                                             |       |
    |          |               SD_Card_Present               |       |
    |    PC    |-------------------------------------------->| ESP32 |
    |          |<--------------------------------------------|       |
    |          |                    true                     |       |
    |          |                                             |       |
    |          |                                             |       |
    |          |                   Open_SD                   |       |
    |    PC    |-------------------------------------------->| ESP32 |
    |          |<--------------------------------------------|       |
    |          |                   SD_Open                   |       |
    |          |                                             |       |
    |          |                                             |       |
    |          |                Save_SD_File                 |       |
    |    PC    |-------------------------------------------->| ESP32 |
    |          |<--------------------------------------------|       |
    |          |               First File Name               |       |
    |          |                                             |       |
    |          |                                             |       |
    |          |                Save_SD_File                 |       |
    |    PC    |-------------------------------------------->| ESP32 |
                                    * * *
    |          |<--------------------------------------------|       |
    |          |               Last File Name                |       |
    |          |                                             |       |
    |          |                                             |       |
    |          |                Save_SD_File                 |       |
    |    PC    |-------------------------------------------->| ESP32 |
    |          |<--------------------------------------------|       |
    |          |            SD_Card_No_More_Files            |       |
    |          |                                             |       |
    |          |                                             |       |
    |          |                  Close_SD                   |       |
    |    PC    |-------------------------------------------->| ESP32 |
    |          |<--------------------------------------------|       |
    |          |                  SD_Closed                  |       |
    |          |                                             |       |
    |          |                                             |       |
    |    PC    |                                             | ESP32 |
    '----------'                                             '-------'

*/

#include <LittleFS.h>   // Built-in
#include "SdFat.h"      // http://librarymanager/All#sdfat_exfat by Bill Greiman.
#include <SPI.h>        // Built-in

//----------------------------------------
// Constants
//----------------------------------------

const char * partitionName = "spiffs";

#define EVK_MICRO_SD_CS_PIN     4   // Card select
#define EVK_MICRO_SD_CD_PIN     36  // Card detect
#define EVK_POWER_CONTROL_PIN   32  // Power control to SD card
#define EVK_SPI_FREQUENCY       16  // MHz
#define EVK_SPI_PICO            23  // MicroSD Card --> ESP32
#define EVK_SPI_POCI            19  // ESP32 --> MicroSD Card
#define EVK_SPI_SCLK            18  // ESP32 --> MicroSD Card

//----------------------------------------
// Locals
//----------------------------------------

char command[8192];
uint16_t commandLength;
char fileName[2048];
char line[256];
SdFat * sd;

//----------------------------------------
// Application entry point
//----------------------------------------
void setup()
{
    Serial.begin(115200);
    sprintf(line, "%s\r\n", __FILE__);
    output((uint8_t *)line, strlen(line));
}

//----------------------------------------
// Infinite loop to do the firmware update
//----------------------------------------
void loop()
{
    // Wait for a command from the PC
    if (getCommand())
        processCommand();
}

//----------------------------------------
// Output a buffer of data
//
// Inputs:
//   buffer: Address of a buffer of data to output
//   length: Number of bytes of data to output
//----------------------------------------
void output(uint8_t * buffer, size_t length)
{
    size_t bytesWritten;

    if (Serial)
    {
        while (length)
        {
            // Wait until space is available in the FIFO
            while (Serial.availableForWrite() == 0);

            // Output the character
            bytesWritten = Serial.write(buffer, length);
            buffer += bytesWritten;
            length -= bytesWritten;
        }
    }
}

//----------------------------------------
// Dump the contents of a buffer
//----------------------------------------
void dumpBuffer(size_t offset,
                const uint8_t * buffer,
                size_t length)
{
    size_t bytes;
    const uint8_t *end;
    size_t index;
    char line[132];

    end = &buffer[length];
    while (buffer < end)
    {
        // Determine the number of bytes to display on the line
        bytes = end - buffer;
        if (bytes > (16 - (offset & 0xf)))
            bytes = 16 - (offset & 0xf);

        // Display the offset
        sprintf(line, "0x%08lx: ", offset);

        // Skip leading bytes
        for (index = 0; index < (offset & 0xf); index++)
            strcat(&line[strlen(line)], "   ");

        // Display the data bytes
        for (index = 0; index < bytes; index++)
            sprintf(&line[strlen(line)], "%02x ", buffer[index]);

        // Separate the data bytes from the ASCII
        for (; index < (16 - (offset & 0xf)); index++)
            strcat(&line[strlen(line)], "   ");
        strcat(&line[strlen(line)], " ");

        // Skip leading bytes
        for (index = 0; index < (offset & 0xf); index++)
            strcat(&line[strlen(line)], " ");

        // Display the ASCII values
        for (index = 0; index < bytes; index++)
            sprintf(&line[strlen(line)], "%c", ((buffer[index] < ' ') || (buffer[index] >= 0x7f)) ? '.' : buffer[index]);

        // Output the line
        sprintf(line, "%s\r\n", line);
        output((uint8_t *)line, strlen(line));

        // Set the next line of data
        buffer += bytes;
        offset += bytes;
    }
}

//----------------------------------------
// Display the contents of the partition table
//----------------------------------------
void printPartitionTable(void)
{
    Serial.printf("ESP32 Partition table:\r\n\n");

    Serial.printf("| Type | Sub |  Offset  |   Size   |       Label      |\r\n");
    Serial.printf("| ---- | --- | -------- | -------- | ---------------- |\r\n");

    esp_partition_iterator_t pi = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
    if (pi != NULL)
    {
        do
        {
            const esp_partition_t *p = esp_partition_get(pi);
            Serial.printf("|  %02X  | %02X  | 0x%06X | 0x%06X | %-16s |\r\n", p->type, p->subtype, p->address, p->size,
                          p->label);
        } while ((pi = (esp_partition_next(pi))));
    }
}

//----------------------------------------
// Find the partition in the SPI flash used for the file system
//----------------------------------------
bool findSpiffsPartition(void)
{
    esp_partition_iterator_t pi = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
    if (pi != NULL)
    {
        do
        {
            const esp_partition_t *p = esp_partition_get(pi);
            if (strcmp(p->label, partitionName) == 0)
                return true;
        } while ((pi = (esp_partition_next(pi))));
    }
    return false;
}

//----------------------------------------
// List the files in LittleFS
//----------------------------------------
// List the contents of the current directory
// Inputs:
//   menuEntry: Address of the object describing the menu entry
//   command: Zero terminated command string
//   display: Device used for output
void directoryListing()
{
    File rootDir;
    bool directory;
    File file;
    bool headerDisplayed;
    const char * name;
    size_t size;

    // Display the partition name
    Serial.printf("\nPartition: %s\r\n", partitionName);

    // Start at the beginning of the directory
    rootDir = LittleFS.open("/", FILE_READ);
    if (rootDir)
    {
        // List the contents of the directory
        headerDisplayed = false;
        while (1)
        {
            // Open the next file
            file = rootDir.openNextFile();
            if (!file)
            {
                if (headerDisplayed == false)
                    Serial.printf("No files found\r\n");
                break;
            }

            // Get the file attributes
            name = file.name();
            size = file.size();
            directory = file.isDirectory();

            // Done with this file
            file.close();

            // Display the attributes
            if (headerDisplayed == false)
            {
                headerDisplayed = true;
                Serial.printf("      Size   Dir   File Name\r\n");
                //                      1                  1         2
                //             1234567890   123   12345678901234567890
                Serial.printf("----------   ---   --------------------\r\n");
            }

            // Display the file attributes
            Serial.printf("%10d   %s   %s\r\n", size, directory ? "Dir" : "   ", name);
        }

        // Close the directory
        close(rootDir);
    }
}

//----------------------------------------
// SD card directory listing
//----------------------------------------
void sdCardDirectoryListing()
{
    SdFile sdRootDir;
    bool directory;
    SdFile sdFile;
    bool headerDisplayed;
    const char * name;
    size_t size;

    // Display the partition name
    Serial.printf("\nSD Card Files\r\n");

    // Start at the beginning of the directory
    if (sdRootDir.open("/"))
    {
        // List the contents of the directory
        headerDisplayed = false;
        while (1)
        {
            // Open the next file
            if (!sdFile.openNext(&sdRootDir, O_RDONLY))
            {
                if (headerDisplayed == false)
                    Serial.printf("No files found\r\n");
                break;
            }

            // Get the file attributes
            sdFile.getName(fileName, sizeof(fileName));
            size = sdFile.fileSize();
            directory = sdFile.isDir();

            // Done with this file
            sdFile.close();

            // Display the attributes
            if (headerDisplayed == false)
            {
                headerDisplayed = true;
                Serial.printf("      Size   Dir   File Name\r\n");
                //                      1                  1         2
                //             1234567890   123   12345678901234567890
                Serial.printf("----------   ---   --------------------\r\n");
            }

            // Display the file attributes
            Serial.printf("%10d   %s   %s\r\n", size, directory ? "Dir" : "   ", fileName);
        }

        // Close the directory
        sdRootDir.close();
    }
}

//----------------------------------------
// Determine if the platform supports the micro SD card
// and if the SD card is in the slot
//----------------------------------------
bool sdCardPresent(void)
{
    uint32_t powerOnStartMsec;

    // RTK EVK
    // Disable the microSD card
    pinMode(EVK_MICRO_SD_CS_PIN, OUTPUT);
    digitalWrite(EVK_MICRO_SD_CS_PIN, HIGH);

    // Enable the card detect input
    pinMode(EVK_MICRO_SD_CD_PIN, INPUT);

    // Initialize the SPI controller
    SPI.begin(EVK_SPI_SCLK, EVK_SPI_POCI, EVK_SPI_PICO);

    // Turn on power to the peripherals
    pinMode(EVK_POWER_CONTROL_PIN, OUTPUT);
    digitalWrite(EVK_POWER_CONTROL_PIN, HIGH);

    // Wait a second for power on
    powerOnStartMsec = millis();
    while ((millis() - powerOnStartMsec) < 1000);

    // Determine if the micro SD card is present
    if (digitalRead(EVK_MICRO_SD_CD_PIN) == LOW)
    {
        sd = new SdFat();
        return (sd != nullptr); // Card detect low = SD in place
    }
    return false;    // Card detect high = No SD
}

//----------------------------------------
// Process the commands
//----------------------------------------
void processCommand()
{
    size_t bytesRead;
    size_t bytesToRead;
    bool directory;
    File file;
    bool headerDisplayed;
    const int maxTries = 3;
    const char * name;
    static File rootDir;
    SdFile sdFile;
    static SdFile sdRootDir;
    size_t size;
    int tries;

    // Wait for a connection
    if (strcmp(command, "Hello_ESP32") == 0)
        Serial.printf("Hello_PC\r\n");

    // Open LittleFS
    else if (strcmp(command, "Open_LFS") == 0)
    {
        // Display the partition table contents
        printPartitionTable();

        // Determine if the spiffs partition was found
        if (findSpiffsPartition() && (LittleFS.begin(true)))
        {
            directoryListing();

            // Start at the beginning of the directory
            rootDir = LittleFS.open("/", FILE_READ);
            if (rootDir)
                Serial.printf("LFS_Open\r\n");
        }
    }

    // Save LittleFS file
    else if (strcmp(command, "Save_LFS_File") == 0)
    {
        do
        {
            // Open the next file
            file = rootDir.openNextFile();
            if (!file)
            {
                // Done sending files
                Serial.printf("LFS_No_More_Files\r\n");
                break;
            }

            // Get the file attributes
            name = file.name();
            size = file.size();
            directory = file.isDirectory();

            // Handle the file
            if (!directory)
            {
                // Send the file name as the command
                Serial.printf("%s\r\n", name);

                // Send the file size
                Serial.printf("%d\r\n", size);

                // Send the file data
                while (size)
                {
                    if (Serial.availableForWrite())
                    {
                        Serial.write(file.read());
                        size -= 1;
                    }
                }
            }

            // Done with this file
            file.close();
        } while (directory);
    }

    // Close LittleFS
    else if (strcmp(command, "Close_LFS") == 0)
    {
        // Close the directory
        close(rootDir);

        // Done with LittleFS
        LittleFS.end();
        Serial.printf("LFS_Closed\r\n");
    }

    // Determine if the SD card is present
    else if (strcmp(command, "SD_Card_Present") == 0)
        Serial.printf("%s\r\n", sdCardPresent() ? "true" : "false");

    // Open the SD card
    else if (strcmp(command, "Open_SD") == 0)
    {
        // Initialize the SD card
        for (tries = 0; tries < maxTries; tries++)
        {
            if (sd->begin(SdSpiConfig(EVK_MICRO_SD_CS_PIN,
                                      DEDICATED_SPI,
                                      SD_SCK_MHZ(EVK_SPI_FREQUENCY))))
                break;

            // Give SD more time to power up, then try again
             delay(250);
        }

        if (tries == maxTries)
            Serial.printf("ERROR: Failed to initialize the SD card\r\n");

        // Start at the beginning of the directory
        else
        {
            // Display the files in the directory
            sdCardDirectoryListing();

            // Open the root directory to save the files
            if (sdRootDir.open("/"))
                Serial.printf("SD_Open\r\n");
        }
    }

    // Save SD card file
    else if (strcmp(command, "Save_SD_File") == 0)
    {
        bool skipFile = false;
        do
        {
            // Open the next file
            if (!sdFile.openNext(&sdRootDir, O_RDONLY))
            {
                // Done sending files
                Serial.printf("SD_Card_No_More_Files\r\n");
                break;
            }

            // Get the file attributes
            sdFile.getName(fileName, sizeof(fileName));
            size = sdFile.fileSize();
            directory = sdFile.isDir();

            // Handle the file, skip directories and data files
            skipFile = ! directory;
            skipFile |= (strcmp(&fileName[strlen(fileName) - 4], ".txt") == 0);
            if ((!directory) && (strcmp(&fileName[strlen(fileName) - 4], ".txt") == 0))
            {
                // Send the file name as the command
                Serial.printf("%s\r\n", fileName);

                // Send the file size
                Serial.printf("%d\r\n", size);

                // Send the file data
                while (size)
                {
                    if (Serial.availableForWrite())
                    {
                        Serial.write(file.read());
                        size -= 1;
                    }
                }
            }

            // Done with this file
            sdFile.close();
        } while (skipFile);
    }

    // Close SD card
    else if (strcmp(command, "Close_SD") == 0)
    {
        // Close the directory
        sdRootDir.close();

        // Done with micro SD card
        sd->end();
        Serial.printf("SD_Closed\r\n");
    }
}

//----------------------------------------
// Get a command from the PC
//----------------------------------------
bool getCommand()
{
    bool gotCommand = false;
    if (Serial)
    {
        // Wait for data from the PC
        while (Serial.available())
        {
            // Read the data from the PC
            char data = Serial.read();

            // The command is complete upon receiving either a CR or LF
            if ((data == '\r') || (data == '\n'))
            {
                gotCommand = (commandLength > 0);
                commandLength = 0;
                break;
            }

            // Add this character to the command buffer
            command[commandLength++] = data;
            command[commandLength] = 0;
        }
    }

    // Notify the caller when the command is available
    return gotCommand;
}
