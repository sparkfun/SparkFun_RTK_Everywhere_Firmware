/*
Save_Files.c

  Save files from the SD card and NVM (Little File System) onto the PC
  in the SD_Card and LittleFS directories

  This program exchanges the following messages between the PC or Raspberry
  Pi system and the ESP32 in the RTK device.

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

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

//----------------------------------------
// New types
//----------------------------------------

typedef struct _COMMAND_OPTION
{
    bool _displayHandshakeDiagram;
    bool _getBaudRate;
    bool * _optionBoolean;
    const char * _optionString;
    const char * _helpText;
} COMMAND_OPTION;

//----------------------------------------
// Constants
//----------------------------------------

#define MAX_PACKET_SIZE             (5 * 1024)

#define BAIL_WITH_SUCCESS           0x8000000

// File save states
enum FILE_SAVE_STATES
{
    FSS_HELLO = 0,          // 0
    FSS_LFS_OPEN,           // 1
    FSS_LFS_SAVE_FILE,      // 2
    FSS_LFS_SAVE_DATA,      // 3
    FSS_LFS_CLOSE,          // 4
    FSS_SD_CARD_PRESENT,    // 5
    FSS_SD_OPEN,            // 6
    FSS_SD_SAVE_FILE,       // 7
    FSS_SD_SAVE_DATA,       // 8
    FSS_SD_CLOSE,           // 9
    // Add new states above this line
    FSS_MAX                 // 10
};

#define nullptr                 NULL

const char * dashes = "---------------------------------------------";
const char * leftArrow = "<";
const char * rightArrow = ">";
const char * esp32       = "|       |";
const char * esp32Bottom = "'-------'";
const char * esp32Label  = "| ESP32 |";
const char * esp32Top    = ".-------.";
const char * pc       = "|          |";
const char * pcBottom = "'----------'";
const char * pcLabel  = "|    PC    |";
const char * pcTop    = ".----------.";
const char * spaces = "                                                                                ";

//----------------------------------------
// Globals
//----------------------------------------

size_t commandResponseLength;
int comPort;
bool displayBytesReceived;
bool displayCommand;
bool displayResponse;
bool displayHandshakeDiagram;
int file;
const char * helloESP32 = "Hello_ESP32";
uint32_t pollTimeoutUsec;
uint8_t response[8192];
size_t responseLength;
int state;
char * timeoutMessage;

//----------------------------------------
// Add a command to the handshake diagram
//----------------------------------------
void addCommandToHandshakeDiagram(const char * command)
{
    size_t length;
    size_t spacesAfter;
    size_t spacesBefore;

    // Display the command
    length = strlen(command);
    spacesAfter = strlen(dashes) - 1 - length - 1;
    spacesBefore = spacesAfter / 2;
    spacesAfter -= spacesBefore;
    spacesBefore = strlen(spaces) - spacesBefore;
    spacesAfter = strlen(spaces) - spacesAfter;
    printf("%s%s %s %s%s\r\n",
           pc,
           &spaces[spacesBefore],
           command,
           &spaces[spacesAfter],
           esp32);

    // Display the arrow
    printf("%s%s%s%s\r\n", pcLabel, &dashes[1], rightArrow, esp32Label);
}

//----------------------------------------
// Add a response to the handshake diagram
//----------------------------------------
void addResponseToHandshakeDiagram(const char * responseString)
{
    bool displayLabel;
    size_t length;
    size_t spacesAfter;
    size_t spacesBefore;

    // Display the arrow
    printf("%s%s%s%s\r\n", pc, leftArrow, &dashes[1], esp32);

    // Display the response
    displayLabel = ! displayCommand;
    length = strlen(responseString);
    spacesAfter = strlen(dashes) - 1 - length - 1;
    spacesBefore = spacesAfter / 2;
    spacesAfter -= spacesBefore;
    spacesBefore = strlen(spaces) - spacesBefore;
    spacesAfter = strlen(spaces) - spacesAfter;
    printf("%s%s %s %s%s\r\n",
           displayLabel ? pcLabel : pc,
           &spaces[spacesBefore],
           responseString,
           &spaces[spacesAfter],
           displayLabel ? esp32Label : esp32);

    // Display a couple of blank lines
    printf("%s%s%s\r\n", pc, &spaces[strlen(spaces) - strlen(dashes)], esp32);
    printf("%s%s%s\r\n", pc, &spaces[strlen(spaces) - strlen(dashes)], esp32);
}

//----------------------------------------
// Configure the serial port talking with the ESP32
//----------------------------------------
int configureComPort(speed_t baudRate)
{
    struct termios options;
    int status;

    do
    {
        // Get the COM Port settings
        status = tcgetattr(comPort, &options);
        if (status < 0)
        {
            status = errno;
            perror("ERROR: Failed to get COM Port settings\r\n");
            break;
        }

        // Set the baud rates
        cfsetispeed(&options, baudRate);
        cfsetospeed(&options, baudRate);

        // 8 bits, no parity, one stop bit
        options.c_cflag = (options.c_cflag & ~CSIZE) | CS8;
        options.c_cflag &= ~PARENB;
        options.c_cflag &= ~CSTOPB;

        // Disable hardware flow control (CRTSCTS)
        options.c_cflag &= ~CRTSCTS;

        // Disable software flow control (IXON, IXOFF, IXANY)
        options.c_iflag &= ~(IXON | IXOFF | IXANY);

        options.c_cflag |= CREAD | CLOCAL;

        options.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHONL | ISIG);

        options.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

        options.c_oflag &= ~(OPOST | ONLCR);

        // Update the COM Port settings
        status = tcsetattr(comPort, TCSANOW, &options);
        if (status < 0)
        {
            status = errno;
            perror("ERROR: Failed to set COM Port settings\r\n");
            break;
        }
        status = 0;
    } while (0);

    // Return the configuration status
    return status;
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
        printf("%s\r\n", line);

        // Set the next line of data
        buffer += bytes;
        offset += bytes;
    }
}

//----------------------------------------
// Write some data to the ESP32
//----------------------------------------
int writeData(const uint8_t * command, ssize_t length)
{
    size_t bytes;
    const size_t maxBytes = 4096;
    errno = 0;

    // Ensure that all of the data is sent to the ESP32
    while (length)
    {
        // Send some data to the ESP32
        bytes = length;
        if (bytes > maxBytes)
            bytes = maxBytes;
        int bytesWritten = write(comPort, command, bytes);

        // Handle write errors
        if (bytesWritten < 0)
            break;

        // Account for the data sent
        length -= bytesWritten;
        command += bytesWritten;
    }
    return errno;
}

//----------------------------------------
// Send a command to the ESP32
//----------------------------------------
int writeCommand(const char * command)
{
    // Display the command
    if (displayCommand)
        addCommandToHandshakeDiagram(command);

    // Send the command to the ESP32
    if (writeData((const uint8_t *)command, strlen(command)))
        return errno;
    return writeData((const uint8_t *)"\r\n", 2);
}

//----------------------------------------
// Read a response from the ESP32
//----------------------------------------
bool getResponse()
{
    char data;
    bool gotResponse;

    errno = 0;
    gotResponse = false;
    do
    {
        // Read data from the ESP32
        ssize_t bytesRead = read(comPort, &response[responseLength], 1);

        // Handle the errors
        if (bytesRead <= 0)
            break;

        // Display the byte
        if (displayBytesReceived)
            printf("0x%02x\r\n", response[responseLength]);

        // Ignore both CR and LF
        if ((response[responseLength] == '\r') || (response[responseLength] == '\n'))
        {
            // Done when LF received
            gotResponse = (responseLength != 0) && (response[responseLength] == '\n');
            break;
        }

        // Buffer the response
        responseLength += 1;
    } while (0);

    // Zero terminate the string
    response[responseLength] = 0;

    // Start a new response if necessary
    if (gotResponse)
    {
        responseLength = 0;
    }

    // Tell the caller of the response
    return gotResponse;
}

//----------------------------------------
// Walk through the states needed to save the files
//----------------------------------------
int updateStateMachine(bool timeout)
{
    ssize_t bytesRead;
    ssize_t bytesWritten;
    uint16_t commandStatus;
    uint32_t crc;
    int exitStatus;
    ssize_t fileBytes;
    const char * fileName;
    static bool firstFile;
    bool gotResponse;
    int length;
    const char * message;
    static char path[2048];
    bool printResponse;

    // Handle timeouts
    if (timeout && timeoutMessage && (state < FSS_MAX))
    {
        printf("%s\r\n", timeoutMessage);
        timeoutMessage = nullptr;
        return -1;
    }

    // Get the response
    exitStatus = 0;
    gotResponse = false;
    if (timeout == false)
    {
        // Get the response
        gotResponse = getResponse();

        // Handle the any response errors
        exitStatus = errno;
        if (exitStatus)
            return exitStatus;
    }

    // Process the state
    printResponse = gotResponse;
    switch (state)
    {
        // Default case for development
        default:
            // Display the error
            printf("ERROR: Unknown state %d\r\n", state);
            if (timeout)
                printf("Timeout!\r\n");
            exitStatus = -1;
            timeoutMessage = nullptr;
            break;

        // Establish a connection to the ESP32
        case FSS_HELLO:
            if (timeout)
                exitStatus = writeCommand(helloESP32);

            // Determine if the ESP32 is connected
            if (gotResponse && strcmp((char *)response, "Hello_PC") == 0)
            {
                printResponse = false;

                // Display the response
                if (displayResponse)
                    addResponseToHandshakeDiagram((char *)response);

                // Send the open LFS command
                exitStatus = writeCommand("Open_LFS");
                timeoutMessage = "ERROR: Timeout opening LittleFS!\r\n";
                firstFile = true;
                state = FSS_LFS_OPEN;
            }
            break;

        // Open the little file system in NVM
        case FSS_LFS_OPEN:
            // Determine if LittleFS is open
            if (gotResponse && strcmp((char *)response, "LFS_Open") == 0)
            {
                printResponse = false;

                // Display the response
                if (displayResponse)
                    addResponseToHandshakeDiagram((char *)response);

                // Send the save file command
                exitStatus = writeCommand("Save_LFS_File");
                timeoutMessage = "ERROR: Timeout saving LittleFS file!\r\n";
                state = FSS_LFS_SAVE_FILE;
            }
            break;

        // Save a file to the LittleFS directory
        case FSS_LFS_SAVE_FILE:
            if (gotResponse)
            {
                printResponse = false;

                // Display the response
                if (displayResponse)
                    addResponseToHandshakeDiagram((char *)response);

                file = -1;
                do
                {
                    const char * directory = "LittleFS";

                    // Determine if no more files exist
                    if (strcmp((char *)response, "LFS_No_More_Files") == 0)
                    {
                        // Done reading files, close LittleFS
                        exitStatus = writeCommand("Close_LFS");
                        timeoutMessage = "ERROR: Timeout Closing LittleFS!\r\n";
                        state = FSS_LFS_CLOSE;
                        break;
                    }

                    // Create the LittleFS directory if necessary
                    if (firstFile && mkdir(directory, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH))
                    {
                        if (errno != EEXIST)
                        {
                            exitStatus = errno;
                            perror("ERROR: Failed to create directory!");
                            break;
                        }
                        firstFile = false;
                    }

                    // Attempt to create the file
                    fileName = (const char *)response;
                    sprintf(path, "%s/%s", directory, fileName);
                    file = creat(path, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                    if (file < 0)
                    {
                        exitStatus = errno;
                        perror("ERROR: Failed to open file!");
                        break;
                    }

                    // Save the file contents
                    state = FSS_LFS_SAVE_DATA;
                    timeoutMessage = "ERROR: Timeout waiting for file length!\r\n";
                } while (0);

                // Close the file
                if (exitStatus && (file >= 0))
                    close(file);
            }
            break;

        // Save the file data
        case FSS_LFS_SAVE_DATA:
            // Wait for the file length to be received
            if (gotResponse)
            {
                if (sscanf((char *)response, "%ld", &fileBytes) != 1)
                {
                    exitStatus = -1;
                    printf("ERROR: Failed to get file length!\r\n");
                    break;
                }
                printResponse = false;

                // Save the file data
                while (fileBytes > 0)
                {
                    // Determine the number of bytes to read
                    length = sizeof(response);
                    if (length > fileBytes)
                        length = fileBytes;

                    // Read more of the file data
                    bytesRead = read(comPort, &response, length);
                    if (bytesRead < 0)
                    {
                        exitStatus = errno;
                        perror("ERROR: Failed reading file data!");
                        break;
                    }

                    // Write the data to the file
                    bytesWritten = write(file, response, bytesRead);
                    if (bytesWritten != bytesRead)
                    {
                        exitStatus = errno;
                        perror("ERROR: Failed writing file data!");
                        break;
                    }

                    // Read the rest of the file
                    fileBytes -= bytesRead;
                }

                // Save the next file
                exitStatus = writeCommand("Save_LFS_File");
                timeoutMessage = "ERROR: Timeout saving LittleFS file!\r\n";
                state = FSS_LFS_SAVE_FILE;
            }
            break;

        // Done with the LittleFS file system
        case FSS_LFS_CLOSE:
            // Determine if LittleFS is closed
            if (gotResponse && strcmp((char *)response, "LFS_Closed") == 0)
            {
                printResponse = false;

                // Display the response
                if (displayResponse)
                    addResponseToHandshakeDiagram((char *)response);

                // Determine if the SD card is present
                exitStatus = writeCommand("SD_Card_Present");
                timeoutMessage = "ERROR: Timeout determining if the SD card is present!\r\n";
                pollTimeoutUsec = 3 * 1000 * 1000;
                state = FSS_SD_CARD_PRESENT;
            }
            break;

        // Determine if the SD card is present
        case FSS_SD_CARD_PRESENT:
            if (gotResponse)
            {
                printResponse = false;

                // Display the response
                if (displayResponse)
                    addResponseToHandshakeDiagram((char *)response);

                if (strcmp((char *)response, "true") == 0)
                {
                    // Send the open SD command
                    exitStatus = writeCommand("Open_SD");
                    timeoutMessage = "ERROR: Timeout opening micro SD card!\r\n";
                    firstFile = true;
                    state = FSS_SD_OPEN;
                }
                else if (strcmp((char *)response, "false") == 0)
                    // No SD card, all done
                    state = FSS_MAX;
            }
            break;

        // Open the SD card file system
        case FSS_SD_OPEN:
            // Determine if LittleFS is open
            if (gotResponse && strcmp((char *)response, "SD_Open") == 0)
            {
                printResponse = false;

                // Display the response
                if (displayResponse)
                    addResponseToHandshakeDiagram((char *)response);

                // Send the save file command
                exitStatus = writeCommand("Save_SD_File");
                timeoutMessage = "ERROR: Timeout saving SD card file!\r\n";
                pollTimeoutUsec = 500 * 1000;
                state = FSS_SD_SAVE_FILE;
            }
            break;

        // Save a file to the SD_Card directory
        case FSS_SD_SAVE_FILE:
            if (gotResponse)
            {
                printResponse = false;

                // Display the response
                if (displayResponse)
                    addResponseToHandshakeDiagram((char *)response);

                file = -1;
                do
                {
                    const char * directory = "SD_Card";

                    // Determine if no more files exist
                    if (strcmp((char *)response, "SD_Card_No_More_Files") == 0)
                    {
                        // Done reading files, close LittleFS
                        exitStatus = writeCommand("Close_SD");
                        timeoutMessage = "ERROR: Timeout Closing SD Card!\r\n";
                        state = FSS_SD_CLOSE;
                        break;
                    }

                    // Create the SD_Card directory if necessary
                    if (firstFile && mkdir(directory, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH))
                    {
                        if (errno != EEXIST)
                        {
                            exitStatus = errno;
                            perror("ERROR: Failed to create directory!");
                            break;
                        }
                        firstFile = false;
                    }

                    // Attempt to create the file
                    fileName = (const char *)response;
                    sprintf(path, "%s/%s", directory, fileName);
                    file = creat(path, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
                    if (file < 0)
                    {
                        exitStatus = errno;
                        perror("ERROR: Failed to open file!");
                        break;
                    }

                    // Save the file contents
                    state = FSS_SD_SAVE_DATA;
                    timeoutMessage = "ERROR: Timeout waiting for file length!\r\n";
                } while (0);

                // Close the file
                if (exitStatus && (file >= 0))
                    close(file);
            }
            break;

        // Save the file data
        case FSS_SD_SAVE_DATA:
            // Wait for the file length to be received
            if (gotResponse)
            {
                if (sscanf((char *)response, "%ld", &fileBytes) != 1)
                {
                    exitStatus = -1;
                    printf("ERROR: Failed to get file length!\r\n");
                    break;
                }
                printResponse = false;

                // Save the file data
                while (fileBytes > 0)
                {
                    // Determine the number of bytes to read
                    length = sizeof(response);
                    if (length > fileBytes)
                        length = fileBytes;

                    // Read more of the file data
                    bytesRead = read(comPort, &response, length);
                    if (bytesRead < 0)
                    {
                        exitStatus = errno;
                        perror("ERROR: Failed reading file data!");
                        break;
                    }

                    // Write the data to the file
                    bytesWritten = write(file, response, bytesRead);
                    if (bytesWritten != bytesRead)
                    {
                        exitStatus = errno;
                        perror("ERROR: Failed writing file data!");
                        break;
                    }

                    // Read the rest of the file
                    fileBytes -= bytesRead;
                }

                // Save the next file
                exitStatus = writeCommand("Save_SD_File");
                timeoutMessage = "ERROR: Timeout saving SD card file!\r\n";
                state = FSS_SD_SAVE_FILE;
            }
            break;

        // Done with the SD card file system
        case FSS_SD_CLOSE:
            printResponse = false;

            // Determine if SD card is closed
            if (gotResponse && strcmp((char *)response, "SD_Closed") == 0)
            {
                // Display the response
                if (displayResponse)
                    addResponseToHandshakeDiagram((char *)response);

                // All done
                state = FSS_MAX;
                exitStatus = BAIL_WITH_SUCCESS;
            }
            break;
    }

    // Display the response
    if (printResponse)
        printf("%s\r\n", response);

    // Return the processing status
    return exitStatus;
}

//----------------------------------------
// Handle data flow with the COM Port
//----------------------------------------
int handleComPort()
{
    fd_set currentfds;
    int exitStatus;
    int maxfds;
    int numfds;
    fd_set readfds;
    struct timeval timeout;
    const char * versionInfoCommand = "$PQTMVERNO*58\r\n";


    //Initialize the fd_sets
    FD_ZERO(&readfds);
    FD_SET(comPort, &readfds);
    maxfds = fileno(stdin);
    if (maxfds < comPort)
        maxfds = comPort;

    // Send the initial command
    timeoutMessage = nullptr;
    exitStatus = 0;
    exitStatus = writeCommand(helloESP32);

    // Wait for a response
    while ((exitStatus == 0) && (state < FSS_MAX))
    {
        //Set the timeout
        timeout.tv_sec = pollTimeoutUsec / (1000 * 1000);
        timeout.tv_usec = pollTimeoutUsec % (10000 * 1000);

        //Wait for receive data or timeout
        memcpy((void *)&currentfds, (void *)&readfds, sizeof(readfds));
        numfds = select(maxfds + 1, &currentfds, NULL, NULL, &timeout);
        if (numfds < 0)
        {
            exitStatus = errno;
            perror("ERROR: select call failed!");
            break;
        }

        //Determine ESP32 output is available
        else if (FD_ISSET(comPort, &currentfds))
            exitStatus = updateStateMachine(false);

        // Check for timeout
        else if (numfds == 0)
            exitStatus = updateStateMachine(true);
    }
    return exitStatus;
}

//----------------------------------------
// Connect to the COM port
//----------------------------------------
int connectComPort(const char * portName)
{
    int exitStatus;

    exitStatus = 0;
    do
    {
        // Wait for the COM Port to become available
        printf("Waiting for GNSS power on and COM Port %s\r\n", portName);

        // Attempt to open the COM Port
        comPort = open(portName, O_RDWR, 0);

        // Handle the connection error
        if (comPort < 0)
        {
            exitStatus = errno;
            printf("ERROR: Failed to open COM Port %s\r\n", portName);
            perror("");
            break;
        }

        // Configure the COM Port
        exitStatus = configureComPort(B115200);
        if (exitStatus)
            break;

        // Display the headshake header
        if (displayHandshakeDiagram)
        {
            size_t spaceCount = strlen(spaces) - strlen(dashes);
            printf("%s%s%s\r\n", pcTop, &spaces[spaceCount], esp32Top);
            printf("%s%s%s\r\n", pcLabel, &spaces[spaceCount], esp32Label);
        }
    } while (0);
    return exitStatus;
}

//----------------------------------------
// Application to save files to the PC
//----------------------------------------
int main(int argc, char **argv)
{
    int argCount;
    int argOffset;
    size_t bytes;
    static bool displayArguments;
    static bool displayHandshake;
    int exitStatus;
    int index;
    const COMMAND_OPTION options[] =
    {
        {0, 0, &displayArguments,       "--display-arguments", "Display the command arguments"},
        {0, 0, &displayBytesReceived,   "--display-bytes-received", "Display each of the received bytes"},
        {1, 0, &displayCommand,         "--display-command", "Display the ESP32 commands"},
        {1, 0, &displayResponse,        "--display-response", "Display the ESP32 command responses"},
        {1, 0, &displayHandshake,       "--display-handshake-diagram", "Display the handshake diagram"},
    };
    const int optionCount = sizeof(options) / sizeof(options[0]);
    const char * portName;
    bool validCommand;

    exitStatus = -1;
    do
    {
        // Get the options
        argCount = 0;
        argOffset = 1;
        portName = "";
        displayHandshakeDiagram = false;
        validCommand = true;
        while (argc - argOffset)
        {
            bool match;

            // Check for an option
            match = false;
            if (strncmp(argv[argOffset], "--", 2) == 0)
            {
                // Walk the list of options
                for (index = 0; index < optionCount; index++)
                {
                    match = (strcmp(argv[argOffset], options[index]._optionString) == 0);
                    if (match)
                    {
                        displayHandshakeDiagram |= options[index]._displayHandshakeDiagram;
                        *options[index]._optionBoolean = true;
                        argOffset += 1;
                        break;
                    }
                }

                // Check for an integer value
                if (match && options[index]._getBaudRate)
                {
                    int value;

                    // Verify that at least one more argument is present
                    if (argOffset >= argc)
                    {
                        printf("ERROR: Baudrate not specified\r\n");
                        validCommand = false;
                        break;
                    }
                }
            }

            // Check for a valid argument
            else if (argCount < 1)
            {
                if (argCount == 0)
                    // Save the terminal port name
                    portName = argv[argOffset++];
                match = true;
                argCount += 1;
            }

            // Handle the error
            if (match == false)
            {
                // Display the help message
                printf("Invalid argument or option: %s\r\n", argv[argOffset]);
                break;
            }
        }

        // Check for a valid command
        if ((argCount < 1) || (argOffset != argc))
            validCommand = false;

        // Display the help text
        if (validCommand == false)
        {
            printf("%s   [options]   <COM_Port%s%s>\r\n",
                   argv[0],
                   (argc == 2) ? ": " : "",
                   (argc == 2) ? argv[1] : ""
                   );
            printf("    COM_PORT: Example COM3 or /dev/ttyACM0\r\n");
            printf("    Firmware_File: Example LG290P03AANR02A01S.pkg\r\n");
            printf("Options:\r\n");
            for (index = 0; index < optionCount; index++)
                printf("    %s: %s\r\n", options[index]._optionString, options[index]._helpText);
            printf("\r\n");
            printf("Program to update the firmware on the Quectel GNSS device.\r\n");
            break;
        }

        // Display the options and arguments
        if (displayArguments)
        {
            // Display the options
            for (index = 1; index < optionCount; index++)
                printf("%s: %s\r\n", options[index]._optionString,
                       *options[index]._optionBoolean ? "true" : "false");

            // Display the command arguments
            printf("Port: %s\r\n", portName);
        }

        // Determine if displaying the handshake diagram
        if (displayHandshake)
        {
            displayCommand = true;
            displayResponse = true;
        }

        // Attempt to connect to the COM port
        exitStatus = connectComPort(portName);
        if (exitStatus)
            break;

        // Attempt to upgrade the GNSS firmware
        pollTimeoutUsec = 500 * 1000;
        exitStatus = handleComPort();
    } while (0);

    // Done with the COM port
    if (comPort >= 0)
        close(comPort);

    // Convert the exitStatus value if necessary
    if (exitStatus == BAIL_WITH_SUCCESS)
    {
        if (displayHandshakeDiagram)
        {
            size_t spaceCount = strlen(spaces) - strlen(dashes);
            printf("%s%s%s\r\n", pcLabel, &spaces[spaceCount], esp32Label);
            printf("%s%s%s\r\n", pcBottom, &spaces[spaceCount], esp32Bottom);
        }
        exitStatus = 0;
    }
    return exitStatus;
}
