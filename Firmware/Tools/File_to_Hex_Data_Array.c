/**********************************************************************
* File_to_Hex_Data_Array.c
*
* Program to read a file and convert the contexts to C array of unsigned
* characters.
**********************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

//----------------------------------------
// Globals
//----------------------------------------

int binFile;
uint8_t buffer[64 * 1024];
size_t offset;
ssize_t validData;

//----------------------------------------
// Read data from the file and add it to the array as hexadecimal bytes
//----------------------------------------
int writeArray(const char * fileName, off_t fileBytes)
{
    size_t bufferOffset;
    int bytesInLine;
    const int bytesPerLine = 16;
    ssize_t bytesRead;
    int exitStatus;
    int index;
    int remainingBytes;
    size_t startingOffset;

    exitStatus = 0;
    offset = 0;

    // Output the header
    printf("// %s\r\n", fileName);
    printf("// array size is %ld (0x%08lx) bytes\r\n", fileBytes, fileBytes);
    printf("static const uint8_t dataArray[] PROGMEM =\r\n");
    printf("{\r\n");
    while (1)
    {
        // Read some data from the file
        bytesRead = read(binFile, buffer, sizeof(buffer));
        if (bytesRead == 0)
            break;
        if (bytesRead < 0)
        {
            exitStatus = errno;
            perror("ERROR: Failed to read from file!\n");
            break;
        }
        validData = bytesRead;

        // Display the lines of the array
        bufferOffset = 0;
        while (validData)
        {
            // Allow the most of the data to be removed
            if (offset == 0x80)
                printf("#ifdef  COMPILE_ALL_FIRMWARE\r\n");

            // Determine the number of bytes in this line
            bytesInLine = validData;
            if (bytesInLine > bytesPerLine)
                bytesInLine = bytesPerLine;
            remainingBytes = bytesPerLine - bytesInLine;

            // Output the data
            printf("    ");
            startingOffset = offset;
            for (index = 0; index < bytesInLine; index++)
            {
                if (index)
                    printf(" ");
                printf("0x%02x,", buffer[bufferOffset++]);
            }

            // Output space until the comment area
            for (index = 0; index < remainingBytes; index++)
                printf("     ");

            // Output the comment
            printf("  // 0x%08lx, %10ld\r\n", startingOffset, startingOffset);

            // Account for this data
            validData -= bytesInLine;
            offset += bytesInLine;
        }
    }

    // Finish the array
    if (exitStatus == 0)
    {
        if (offset > 0x80)
            printf("#endif  // COMPILE_ALL_FIRMWARE\r\n");
        printf("};  // 0x%08lx, %10ld\r\n", offset, offset);
        printf("// %s\r\n", fileName);
    }
    return exitStatus;
}

//----------------------------------------
// Application
//----------------------------------------
int main(int argc, char ** argv)
{
    off_t fileBytes;
    char * fileName;
    int status;

    do
    {
        status = -1;

        // Display the help text
        if (argc != 2)
        {
            printf ("%s  filename\n", argv[0]);
            return -1;
        }

        // Open the file
        fileName = argv[1];
        binFile = open(fileName, O_RDONLY);
        if (binFile < 0)
        {
            status = errno;
            perror("ERROR: Unable to open the file\n");
        }

        // Determine the file size
        fileBytes = lseek(binFile, 0, SEEK_END);
        lseek(binFile, 0, SEEK_SET);

        // Write "C" array
        status = writeArray(fileName, fileBytes);
    } while (0);

    // Close the file
    if (binFile >= 0)
        close(binFile);

    return status;
}
