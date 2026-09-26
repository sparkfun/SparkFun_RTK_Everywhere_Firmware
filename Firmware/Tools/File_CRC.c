/**********************************************************************
* File_CRC.c
*
* Program to compute the file's CRC value
**********************************************************************/

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "CRC32.c"

//----------------------------------------
// Application
//----------------------------------------
int main(int argc, char ** argv)
{
    int binFile;
    uint8_t * buffer;
    uint32_t crc;
    off_t fileBytes;
    char * fileName;
    int status;
    const char * text;

    do
    {
        status = -1;
        binFile = -1;

        // Display the help text
        if ((argc < 2) || (argc > 3))
        {
            printf ("%s  filename   [text]\n", argv[0]);
            break;
        }

        // Get the arguments
        fileName = argv[1];
        text = (argc == 3) ? argv[2] : NULL;

        // Open the file
        binFile = open(fileName, O_RDONLY);
        if (binFile < 0)
        {
            status = errno;
            perror("ERROR: Unable to open the file\n");
            break;
        }

        // Determine the file size
        fileBytes = lseek(binFile, 0, SEEK_END);
        lseek(binFile, 0, SEEK_SET);

        // Map the file into memory
        buffer = (uint8_t *)mmap(NULL, fileBytes, PROT_READ, MAP_PRIVATE, binFile, 0);
        if (buffer == NULL)
        {
            status = errno;
            perror("ERROR: Unable to map the file into memory\n");
            break;
        }

        // Compute the CRC value
        crc = crc32Compute(0, buffer, fileBytes);

        // Output the text, CRC and file size
        if (text)
            printf("%s,", text);
        printf("%ld,0x%08x\n", fileBytes, crc);
        status = 0;
    } while (0);

    // Close the file
    if (binFile >= 0)
        close(binFile);

    return status;
}
