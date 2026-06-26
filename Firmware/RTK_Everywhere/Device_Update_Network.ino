/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Device_Update_Network.ino

  Support routines to use the network for firmware input
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

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
