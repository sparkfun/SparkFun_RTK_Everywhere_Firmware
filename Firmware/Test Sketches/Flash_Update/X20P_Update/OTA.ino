// The following routines support reading directory listings from GitHub
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

//----------------------------------------
// Expand the buffer
//----------------------------------------
bool otaExpandBuffer(const char * description,
                     uint8_t * &buffer,
                     size_t &bufferBytes,
                     size_t allocBytes,
                     bool debug)
{
    // Allocate a larger buffer
    size_t moreBytes = bufferBytes + allocBytes;
    uint8_t * temp = (uint8_t *)rtkMalloc(moreBytes, description);
    if (temp == nullptr)
    {
        systemPrintf("ERROR: Failed to allocate buffer\r\n");
        return false;
    }

    // Zero the new space
    memset(&temp[bufferBytes], 0, allocBytes);

    // Copy data into the larger buffer and free the origin buffer
    if (buffer)
    {
        memcpy(temp, buffer, bufferBytes);
        free(buffer);
    }

    // Start using the larger buffer
    buffer = temp;
    bufferBytes = moreBytes;
    if (debug)
        systemPrintf("buffer: %p, bufferBytes: %d\r\n", buffer, bufferBytes);
    return true;
}

//----------------------------------------
// Get the next network file name
//----------------------------------------
bool otaFileNamesGet(NetworkClient * stream,
                     const char * dirSuffix,
                     const char * filePrefix,
                     const char * fileSuffix,
                     const char * namePart,
                     const char * extension,
                     int &fileCount,
                     const char * bufferDescription,
                     uint8_t * &buffer,
                     size_t &bufferBytes,
                     size_t &bufferOffset,
                     const size_t allocBytes,
                     bool debug)
{
    char * fileName;
    size_t offset;
    size_t suffixBytes;

    do
    {
        // Expand the buffer if necessary
        size_t requiredBufferSize = bufferOffset + 256;
        while ((bufferBytes < requiredBufferSize)
            && (otaExpandBuffer(bufferDescription,
                                buffer,
                                bufferBytes,
                                allocBytes,
                                settings.debugFirmwareUpdate && otaDebugVerbose) == false))
        {
        }
        if (bufferBytes < requiredBufferSize)
            // There may be some file names in the buffer but there are
            // more.  Display the ones that were found and skip the rest.
            break;

        // Read in the file name
        offset = bufferOffset;
        fileName = (char *)&buffer[offset];
        suffixBytes = strlen(dirSuffix);
        while ((offset < (bufferBytes - 1))
            && (stream->connected() || stream->available()))
        {
            if (stream->available())
            {
                // Build up the file name one character at a time
                buffer[offset] = stream->read();
                if (buffer[offset] == fileSuffix[0])
                    break;
                offset += 1;
            }
        }

        // Get more data if necessary, call this routine again to expand
        // the buffer
        if (buffer[offset] != fileSuffix[0])
            return true;

        // Zero terminate the file name string
        buffer[offset++] = 0;

        // Display the file name
        if (debug)
            systemPrintf("File: %s\r\n", fileName);

        // Determine if this file should be in the list
        if (((namePart == nullptr) || strstr(fileName, namePart))
            && ((extension == nullptr) || strstr(fileName, extension)))
        {
            // Add this file name to the buffer
            bufferOffset = offset;
            fileCount += 1;
        }

        // Locate the next file name
        if (stream->findUntil(filePrefix, dirSuffix))
            return true;

        // End of file list
    } while (0);
    return false;
}

//----------------------------------------
// Sort the list of files
//----------------------------------------
void otaFileListSort(const char ** nameArray, int * sortArray, int fileCount)
{
    // Bubble sort the file names newest to oldest
    for (int i = 0; i < (fileCount - 1); i++)
        for (int j = i + 1; j < fileCount; j++)
            // Determine if the entries should be switched
            if (strcmp(nameArray[sortArray[i]], nameArray[sortArray[j]]) < 0)
            {
                // Switch the entries
                int temp = sortArray[i];
                sortArray[i] = sortArray[j];
                sortArray[j] = temp;
            }
}

//----------------------------------------
// Select a URL from a web site directory listing
//----------------------------------------
String otaSelectFileFromWebPageDirectoryListing(const char * url,
                                                const char * dirPrefix,
                                                const char * dirSuffix,
                                                const char * fileListPrefix,
                                                const char * filePrefix,
                                                const char * fileSuffix,
                                                const char * namePart,
                                                const char * extension,
                                                const char * fileServerPath)
{
    const size_t allocBytes = 1024;
    uint8_t * buffer = nullptr;
    size_t bufferBytes = allocBytes;
    const char * bufferDescription = "Web page file list";
    size_t bufferOffset = 0;
    int fileCount = 0;
    HTTPClient http;
    int incoming;
    int index;
    const char ** nameArray = nullptr;
    const char * nameArrayDescription = "Array of file name addresses";
    size_t requiredBytes;
    String selectedEntry;
    int * sortArray = nullptr;
    const char * sortArrayDescription = "Array of indexes into name buffer";
    NetworkClient * stream;

    do
    {
        systemPrintf("URL: %s\r\n", url);
        if (!http.begin(url))
        {
            systemPrintln("Unable to begin HTTP request.");
            break;
        }

        int httpCode = http.GET();
        if (httpCode != HTTP_CODE_OK)
        {
            systemPrintf("HTTP GET failed, code: %d\r\n", httpCode);
            break;
        }

        // Allocate space for the directory listing
        if (otaExpandBuffer(bufferDescription,
                            buffer,
                            bufferBytes,
                            allocBytes,
                            settings.debugFirmwareUpdate && otaDebugVerbose) == false)
        {
            break;
        }

        // Get TCP stream
        stream = http.getStreamPtr();

        // Locate the beginning of the directory listing
        if (dirPrefix && (stream->find(dirPrefix) == false))
        {
            systemPrintf("Directory prefix not found!\r\n");
            break;
        }

        if (fileListPrefix && (stream->find(fileListPrefix) == false))
        {
            systemPrintf("File list prefix not found!\r\n");
            break;
        }

        // Locate the first file name
        if (stream->findUntil(filePrefix, dirSuffix) == false)
        {
            systemPrintf("File not found!\r\n");
            break;
        }

        // Get the list of files
        while (otaFileNamesGet(stream,
                               dirSuffix,
                               filePrefix,
                               fileSuffix,
                               namePart,
                               extension,
                               fileCount,
                               bufferDescription,
                               buffer,
                               bufferBytes,
                               bufferOffset,
                               allocBytes,
                               settings.debugFirmwareUpdate && otaDebugVerbose))
        {
        }
        if (settings.debugFirmwareUpdate && otaDebugVerbose)
        {
            dumpBuffer((uintptr_t)buffer, buffer, bufferOffset);
            systemPrintf("fileCount: %d\r\n", fileCount);
        }

        // Verify that files were found
        if (fileCount == 0)
        {
            systemPrintf("No files found\r\n");
            break;
        }

        // Allocate the arrays
        requiredBytes = sizeof(const char *) * fileCount;
        nameArray = (const char **)rtkMalloc(requiredBytes, nameArrayDescription);
        if (nameArray == nullptr)
            break;
        requiredBytes = sizeof(int) * fileCount;
        sortArray = (int *)rtkMalloc(requiredBytes, sortArrayDescription);
        if (sortArray == nullptr)
            break;

        // Initialize the arrays
        const char * data = (const char *)buffer;
        for (index = 0; index < fileCount; index++)
        {
            sortArray[index] = index;
            nameArray[index] = data;
            data += strlen(data) + 1;
        }

        // Sort the file names
        otaFileListSort(nameArray, sortArray, fileCount);

        // Display the file list
file_menu:
        systemPrintf("\r\nFile Menu\r\n");
        for (index = 0; index < fileCount; index++)
            systemPrintf("%d) %s\r\n", index, nameArray[sortArray[index]]);
        systemPrintf("Select file: ");

        // Get the user's selection
        if (systemGetNumberFromUser(&incoming) == false)
            break;

        // Validate the user entry
        if ((incoming >= fileCount) || (incoming < 0))
            goto file_menu;

        // Build the selected entry string
        selectedEntry = fileServerPath;
        selectedEntry += nameArray[sortArray[incoming]];
    } while (0);

    // Done with the buffers and the HTTP object
    if (sortArray)
        rtkFree(sortArray, sortArrayDescription);
    if (nameArray)
        rtkFree(nameArray, nameArrayDescription);
    if (buffer)
        rtkFree(buffer, bufferDescription);
    http.end();
    return selectedEntry;
}
