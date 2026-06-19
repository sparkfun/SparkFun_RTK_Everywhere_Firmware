/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
support.ino
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

//----------------------------------------
// Dynamically allocate a buffer
//----------------------------------------
bool bufferDynamicallyAllocate(BUFFER_DATA * bufferData)
{
    const char * description;
    bool dynamicAllocation;
    size_t length;

    // Determine if the buffer needs to be dynamically allocated
    dynamicAllocation = (bufferData->_address == nullptr);
    if (dynamicAllocation)
    {
        // Attempt to allocate the buffer
        description = bufferGetDescription(bufferData);
        length = bufferGetLength(bufferData);
        bufferData->_address = (uint8_t *)rtkMalloc(length, description);
        if (bufferData->_address == nullptr)
            systemPrintf("ERROR: Failed to allocate the '%s' buffer!\r\n", description);
        else
            bufferData->_length = length;
    }
    return dynamicAllocation;
}

//----------------------------------------
// Expand an existing buffer
//----------------------------------------
bool bufferExpand(int bufferIndex)
{
    // Locate the buffer data
    BUFFER_DATA * bufferData = bufferInfo[bufferIndex]._bufferData;
    uint8_t * newBuffer;
    size_t newLength;

    // Determine the new buffer size
    newLength = bufferData->_length + 2048;

    // Allocate the new buffer
    newBuffer = (uint8_t *)rtkMalloc(newLength, bufferInfo[bufferIndex]._description);
    if (newBuffer == nullptr)
    {
        systemPrintf("ERROR: Failed to allocate the new buffer of %d bytes!\r\n", newLength);
        return false;
    }

    // Copy the existing file names into the new buffer
    memcpy(newBuffer, bufferData->_address, bufferData->_offset);

    // Free the old buffer
    free((void *)bufferData->_address);

    // Switch to using the new buffer
    bufferData->_address = newBuffer;
    bufferData->_length = newLength;

    // Zero terminate any strings in the new buffer
    memset(&newBuffer[bufferData->_offset], 0, bufferData->_length - bufferData->_offset);
    return true;
}

//----------------------------------------
// Free a dynamically allocated buffer
//----------------------------------------
void bufferFree(BUFFER_DATA * bufferData)
{
    const char * description;

    // Free the buffer
    if (bufferData->_address)
    {
        description = bufferGetDescription(bufferData);
        rtkFree(bufferData->_address, description);
        bufferData->_address = nullptr;
    }
}

//----------------------------------------
// Get the buffer description
//----------------------------------------
const char * bufferGetDescription(BUFFER_DATA * bufferData)
{
    // Walk the list of buffers
    for (int index = 0; index < bufferInfoCount; index++)
    {
        if (bufferData == bufferInfo[index]._bufferData)
            return bufferInfo[index]._description;
    }

    // Buffer not found
    return nullptr;
}

//----------------------------------------
// Get the buffer index
//----------------------------------------
int bufferGetIndex(BUFFER_DATA * bufferData)
{
    // Walk the list of buffers
    for (int index = 0; index < bufferInfoCount; index++)
    {
        if (bufferData == bufferInfo[index]._bufferData)
            return index;
    }

    // Buffer not found
    return -1;
}

//----------------------------------------
// Get the buffer length
//----------------------------------------
size_t bufferGetLength(BUFFER_DATA * bufferData)
{
    // Walk the list of buffers
    for (int index = 0; index < bufferInfoCount; index++)
    {
        if (bufferData == bufferInfo[index]._bufferData)
            return bufferInfo[index]._sizeInBytes;
    }

    // Buffer not found
    return 0;
}

//----------------------------------------
// Allocate the name and sort arrays, return true if successful
//----------------------------------------
bool bufferNameSortAllocate(int bufferIndex, int fileCount)
{
    BUFFER_DATA * bufferData = bufferInfo[bufferIndex]._bufferData;
    char * fileName;
    size_t length;

    // Allocate the sortArray
    length = sizeof(*bufferData->_sortArray) * fileCount;
    bufferData->_sortArray = (int *)rtkMalloc(length, "Sort Array");
    if (bufferData->_sortArray == nullptr)
        systemPrintf("ERROR: Failed to allocate sortArray, %d bytes!\r\n", length);
    else
    {
        // Allocate the nameArray
        length = sizeof(*bufferData->_nameArray) * fileCount;
        bufferData->_nameArray = (char **)rtkMalloc(length, "Name Array");
        if (bufferData->_nameArray == nullptr)
        {
            bufferNameSortFree(bufferIndex);
            systemPrintf("ERROR: Failed to allocate nameArray, %d bytes!\r\n", length);
        }
        else
        {
            // Initialize the sortArray
            for (int index = 0; index < fileCount; index++)
                bufferData->_sortArray[index] = index;

            // Initialize the nameArray
            fileName = (char *)bufferData->_address;
            for (int index = 0; index < fileCount; index++)
            {
                bufferData->_nameArray[index] = fileName;
                fileName += strlen(fileName) + 1;
            }
        }
    }
    return (bufferData->_nameArray != nullptr);
}

//----------------------------------------
// Free the arrays
//----------------------------------------
void bufferNameSortFree(int bufferIndex)
{
    BUFFER_DATA * bufferData = bufferInfo[bufferIndex]._bufferData;

    // Free nameArray
    if (bufferData->_nameArray != nullptr)
    {
        free(bufferData->_nameArray);
        bufferData->_nameArray = nullptr;
    }

    // Free sortArray
    if (bufferData->_sortArray != nullptr)
    {
        free(bufferData->_sortArray);
        bufferData->_sortArray = nullptr;
    }
}
