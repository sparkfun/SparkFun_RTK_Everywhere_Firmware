#if 0

//----------------------------------------
// Updates the IM19 module firmware from the given URL over WiFi.
//
// Structure (see the header comment at the top of the .ino for the general pattern):
//   1. Connect to WiFi.
//   2. im19UpdateFirmwareBegin() puts the IM19 into its bootloader.
//   3. Stream the file once, feeding chunks to im19UpdateFirmware().
//   4. im19UpdateFirmwareEnd() asks the IM19 what it's missing. If anything, re-request
//      only those byte ranges (im19StreamMissingRanges) and ask again - up to a few
//      attempts - rather than re-streaming the whole binary.
//----------------------------------------
bool im19FirmwareUpdate(const char * chip,
                        const char * url,
                        uint8_t * buffer,
                        size_t packetBytes)
{
    const char * cert;
    NetworkClientSecure client;
    const char * errorMsg;
    size_t fileBytes;
    HTTPClient http;
    String ipAddressString;
    const char * ipAddress;
    char msgBuffer[128];
    const char * server;
    String serverString;
    NetworkClient * stream;

    do
    {
        errorMsg = nullptr;

        // Verify that a URL was specified
        if(settings.debugFirmwareUpdate)
            systemPrintf("URL: %s\r\n", url ? url : "[nullptr]");
        if ((url == nullptr) || (strlen(url) == 0))
        {
            errorMsg = "ERROR: No URL was specified!";
            break;
        }

        // Initialize the UART communicating with the IM19
        im19InitUart();

        // Locate the server for this URL
        serverString = getServerFromUrl(url);
        if (serverString.length() == 0)
        {
            errorMsg = "ERROR: Failed to find server name in URL string";
            break;
        }
        server = serverString.c_str();

        // Translate the server name into an IP address
        ipAddressString = getServerIpAddress(server);
        if (ipAddressString.length() == 0)
        {
            errorMsg = "Failed to get the IP address for the server\r\n";
            break;
        }
        ipAddress = ipAddressString.c_str();

        // Determine if the certificate is known for this server
        cert = getCertFromUrl(url);
        if(settings.debugFirmwareUpdate)
            systemPrintf("Certificate: %s\r\n", cert ? "available" : "none");

        // Use an encrypted and verified connection when possible
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        if (cert)
        {
            // Verify the server using the certificate
            if (!securelyConnectToServer(url, client, cert))
            {
                //                           1         2         3         4         5         6         7         8         9
                //                  123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890
                sprintf(msgBuffer, "ERROR: Failed to securely connect to %s (%s)", server, ipAddress);
                errorMsg = msgBuffer;
                break;
            }

            // Request the URL from the web server
            if (!http.begin(client, url))
            {
                errorMsg = "ERROR: unable to begin HTTPS request.";
                break;
            }
        }

        // Request the URL from the web server
        else if (!http.begin(url))
        {
            errorMsg = "ERROR: Unable to begin HTTP request.";
            break;
        }

        // Get the web server's response
        int httpCode = http.GET();
        if (httpCode != HTTP_CODE_OK)
        {
            //                           1         2         3         4         5         6         7         8         9
            //                  123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890
            sprintf(msgBuffer, "ERROR: Update failed HTTP GET request, code: %d", httpCode);
            errorMsg = msgBuffer;
            break;
        }

        // Get the file size
        fileBytes = http.getSize();
        if (settings.debugFirmwareUpdate)
            systemPrintf("File size: %d (0x%08x) bytes\r\n", fileBytes, fileBytes);
        if (fileBytes <= 0)
        {
            errorMsg = "ERROR: Web server did not report a file size.";
            break;
        }

        // Get the connection to the file data
        stream = http.getStreamPtr();

        if (!im19UpdateFirmwareBegin(fileBytes))
        {
            //                           1         2         3         4         5         6         7         8         9
            //                  123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890
            sprintf(msgBuffer, "ERROR: %s did not respond to the bootloader entry command.", chip);
            errorMsg = msgBuffer;
            break;
        }

        // Now that the IM19 is in its bootloader and waiting, stream the already-open
        // response body straight to it.
        im19NextFrameID = 0;
        if (im19StreamFirmware(chip,
                               stream,
                               fileBytes,
                               buffer,
                               packetBytes) == false)
        {
            //                           1         2         3         4         5         6         7         8         9
            //                  123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890
            sprintf(msgBuffer, "ERROR: %s firmware update failed during transfer", chip);
            errorMsg = msgBuffer;
            break;
        }

        const int maxAttempts = 5;
        //                           1         2         3         4         5         6         7         8         9
        //                  123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890
        sprintf(msgBuffer, "ERROR: %s firmware update failed: too many retries.", chip);
        errorMsg = msgBuffer;
        for (int attempt = 1; attempt <= maxAttempts; attempt++)
        {
            Im19UpdateResult result = im19UpdateFirmwareEnd();
            if (result == IM19_UPDATE_SUCCESS)
            {
                errorMsg = nullptr;
                break;
            }

            if (result == IM19_UPDATE_FAILED)
            {
                //                           1         2         3         4         5         6         7         8         9
                //                  123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890
                sprintf(msgBuffer, "ERROR: %s firmware update failed: no response from %s.", chip, chip);
                errorMsg = msgBuffer;
                break;
            }

            // IM19_UPDATE_RETRY - the IM19 told us exactly which frames it's missing.
            systemPrintf("Attempt %d: %s reports missing frames.\r\n", attempt, chip);
            if (!im19StreamMissingRanges(chip, url, buffer, packetBytes))
            {
                //                           1         2         3         4         5         6         7         8         9
                //                  123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890
                sprintf(msgBuffer, "ERROR: %s firmware update failed while requesting missing frames.", chip);
                errorMsg = msgBuffer;
                break;
            }
        }
    } while (0);

    // Display the firmware update status
    bool success = (errorMsg == nullptr);
    systemPrintln(otaEqualSigns);
    if (success)
        systemPrintf("%s firmware update completed successfully\r\n", chip);
    else
        systemPrintf("%s\r\n", errorMsg);

    // Attempt to display the IM19 firmware version
    im19GetVersionString();
    systemPrintln(otaEqualSigns);

    // Release the resources
    http.end();
    return success;
}

#endif  // 0
