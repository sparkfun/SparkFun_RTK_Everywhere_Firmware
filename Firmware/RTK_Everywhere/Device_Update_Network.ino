/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Device_Update_Network.ino

  Support routines to use the network for firmware input
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

//----------------------------------------
// Done with the network link
//----------------------------------------
void dfuNetworkCleanup(DEVICE_FIRMWARE_CTX *ctx,
                       DFU_BUFFER_DATA * bufferData)
{
    // Restore access to the data buffer
    deviceFirmwareBufferRestore(ctx, bufferData);

    // Free the network resources
    if (ctx->_networkClient)
    {
        ctx->_networkClient->stop();
        ctx->_networkClient = nullptr;
    }

    if (ctx->_https)
    {
        ctx->_https->end();
        delete(ctx->_https);
        ctx->_https = nullptr;
    }

    // Done with the secure client
    if (ctx->_httpsClient)
    {
        delete ctx->_httpsClient;
        ctx->_httpsClient = nullptr;
    }

    // Display the transfer data
    if (settings.debugFirmwareUpdate && bufferData && bufferData->_offset)
    {
        // Finish the header
        uint32_t deltaMsec = millis() - ctx->_startMsec;
        if (deltaMsec)
        {
            uint32_t milliseconds = deltaMsec;
            uint32_t seconds = milliseconds / 1000;
            milliseconds -= seconds * 1000;
            uint32_t bytesPerSecond = (bufferData->_offset * 1000) / deltaMsec;
            systemPrintf("Useful data: %d bytes in %d.%03d seconds, %d bytes/second\r\n",
                          bufferData->_offset, seconds, milliseconds, bytesPerSecond);
        }
        else
            systemPrintf("Useful data: %d bytes\r\n", bufferData->_offset);
    }
}

//----------------------------------------
// Request the web page containing the file specifications
//----------------------------------------
void dfuNetworkFileListBuildUrl(DEVICE_FIRMWARE_CTX * ctx)
{
    const DEVICE_FIRMWARE_INFO * deviceInfo;

    // Build the URL
    deviceInfo = ctx->_deviceInfo;
    ctx->_url = deviceInfo->_server;
    if (deviceInfo->_branch)
        ctx->_url += deviceInfo->_branch;
    if (deviceInfo->_directory)
        ctx->_url += deviceInfo->_directory;
    if (settings.debugFirmwareUpdate)
        systemPrintf("URL: %s\r\n", ctx->_url.c_str());

    ctx->_attemptNumber = 0;

    // Attempt the HTTP request
    deviceFirmwareStateSet(ctx, DFUS_GET_HTTP_FILE_LIST_REQ);
}

//----------------------------------------
// Request the web page containing the file specifications
//----------------------------------------
void dfuNetworkFileListHtmlRequest(DEVICE_FIRMWARE_CTX * ctx,
                                   uint32_t currentMsec)
{
    DFU_BUFFER_DATA * bufferData;
    const char * dirPrefix;
    const char * dirSuffix;
    const char * filePrefix;
    const char * fileListPrefix;
    int httpResponseCode;

    // Connect to the remote web page
    ctx->_startMsec = currentMsec;
    ctx->_https = new HTTPClient;
    ctx->_https->begin(ctx->_url);
    ctx->_https->setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    // Send HTTP GET request
    httpResponseCode = ctx->_https->GET();

    // Display the error
    if (httpResponseCode != 200)
    {
        // Display the response
        if (settings.debugFirmwareUpdate)
            systemPrintf("HTTP Response code: %d (%s)\r\n",
                         httpResponseCode,
                         ctx->_https->errorToString(httpResponseCode));

        // Done with the HTTP link
        dfuNetworkCleanup(ctx, nullptr);

        // Stop with known error or too many retries
        if ((httpResponseCode != -1) || (ctx->_attemptNumber++ >= 3))
        {
            systemPrintf("Failed to open url: %s\r\n", ctx->_url.c_str());
            deviceFirmwareStateSet(ctx, DFUS_GET_NVM_FILE_LIST);
        }
        else
        {
            // Retry accessing the web server
        }
    }
    else
    {
        // Get TCP stream
        ctx->_networkClient = ctx->_https->getStreamPtr();

        // Temporarily use the network name buffer
        bufferData = &dfuFirmwareFileNamesNet;
        ctx->_buffer = bufferData->_address;
        ctx->_bufferLength = bufferData->_length;
        ctx->_validDataBytes = 0;

        // Locate the beginning of the directory listing
        dirPrefix = ctx->_deviceInfo->_dirPrefix;
        dirSuffix = ctx->_deviceInfo->_dirSuffix;
        fileListPrefix = ctx->_deviceInfo->_dirPrefix2;
        filePrefix = ctx->_deviceInfo->_entryPrefix;
        if ((dirPrefix && (ctx->_networkClient->find(dirPrefix) == false))
            || (fileListPrefix && (ctx->_networkClient->find(fileListPrefix) == false)))
        {
            systemPrintf("ERROR: Directory listing not found!\r\n");
            dfuNetworkCleanup(ctx, bufferData);
            deviceFirmwareStateSet(ctx, DFUS_GET_NVM_FILE_LIST);
        }
        else
        {
            // Locate the first file name
            if (ctx->_networkClient->findUntil(filePrefix, dirSuffix))
                // Get the list of names
                deviceFirmwareStateSet(ctx, DFUS_GET_NETWORK_FILE_LIST);
            else
            {
                // Restore access to the data buffer
                dfuNetworkCleanup(ctx, bufferData);

                // No files available
                deviceFirmwareStateSet(ctx, DFUS_GET_NVM_FILE_LIST);
            }
        }
    }
}
