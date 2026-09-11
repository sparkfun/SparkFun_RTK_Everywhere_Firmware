#if 0

//----------------------------------------
// Perform the flash update using an array
//----------------------------------------
bool im19ArrayFlashUpdate(const char * chip,
                          NetworkClient * stream,
                          size_t fileBytes,
                          uint8_t * buffer,
                          size_t packetBytes)
{
    const char * errorMsg;
    char msgBuffer[128];

    do
    {
        // Initialize the UART communicating with the IM19
        im19InitUart();

        if (!im19UpdateFirmwareBegin(fileBytes))
        {
            //                           1         2         3         4         5         6         7         8         9
            //                  123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890
            sprintf(msgBuffer, "ERROR: %s did not respond to the bootloader entry command.", chip);
            errorMsg = msgBuffer;
            break;
        }

        // Now that the IM19 is in its bootloader and waiting, stream the firmware data
        // straight to it.
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
            if (!im19StreamMissingRanges(chip, nullptr, buffer, packetBytes))
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
    return success;
}

#endif  // 0
