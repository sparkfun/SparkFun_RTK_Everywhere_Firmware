/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Tilt.ino
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

//----------------------------------------
// Get the IM19 firmware version message
//----------------------------------------
String tiltGetFirmwareVersion()
{
    return tiltFirmwareVersion;
}

//----------------------------------------
// IM19 reset
//----------------------------------------
bool tiltIm19Reset()
{
    size_t bytesWritten;
    char data;
    const char * reset = "AT+SYSTEM_RESET\r\n";
    uint32_t startMsec;
    String temp;
    uint32_t timeoutMsec;
    bool versionFound;

    do
    {
        // Configure the serial port
        versionFound = false;
        if (configureUart2(&SerialForTilt) == false)
            break;

        // Reset the IM19
        if (settings.enableImuDebug)
            systemPrintf("Sending %s\r\n", reset);
        bytesWritten = SerialForTilt->write((uint8_t *)reset, strlen(reset));
        if (bytesWritten != strlen(reset))
        {
            systemPrintf("ERROR: Failed to write the reset string!\r\n");
            break;
        }

        // Wait for the OK response
        startMsec = millis();
        timeoutMsec = 1 * MILLISECONDS_IN_A_SECOND;
        if (tiltIm19WaitForOkResponse(timeoutMsec) == false)
            break;

        // Search for the version message
        while ((millis() - startMsec) < (uint32_t)timeoutMsec)
        {
            // Wait for a response
            if (SerialForTilt->available() == 0)
                delay(1);
            else
            {
                // Discard any input data before the firmware version
                data = (char)SerialForTilt->read();
                if (settings.enableImuDebug)
                    systemPrintf(((data >= ' ') && (data < 0x7f)) ? "%c\r\n" : "0x%02x\r\n", data);
                if (versionFound == false)
                {
                    if (data == 'V')
                    {
                        temp = data;
                        versionFound = true;
                    }
                }
                else
                {
                    if (data == '\r')
                    {
                        // Remove "Version:"
                        tiltFirmwareVersion = temp.substring(8);
                        return true;
                    }
                    temp += data;
                }
            }
        }
        systemPrintf("ERROR: Timeout waiting for version string!\r\n");
    } while (0);
    return false;
}

//----------------------------------------
// IM19 wait for OK response
//----------------------------------------
bool tiltIm19WaitForOkResponse(uint32_t timeout)
{
    char data;
    const char * error = "Error\r\n";
    size_t errorBytes = strlen(error);
    size_t errorOffset;
    const char * ok = "OK\r\n";
    size_t okBytes = strlen(ok);
    size_t okOffset;
    uint32_t startMsec;

    // Delay for a while
    startMsec = millis();
    errorOffset = 0;
    okOffset = 0;
    while ((millis() - startMsec) < timeout)
    {
        // Wait for a response
        if (SerialForTilt->available() == 0)
            delay(1);
        else
        {
            // Get any input data
            data = (char)SerialForTilt->read();
            if (settings.enableImuDebug)
                systemPrintf(((data >= ' ') && (data < 0x7f)) ? "%c\r\n" : "0x%02x\r\n", data);

            // Search for OK
            if (data == ok[okOffset++])
            {
                if (okOffset == okBytes)
                {
                    if (settings.enableImuDebug)
                        systemPrintf("OK received\r\n");
                    return true;
                }
            }
            else
                okOffset = 0;

            // Search for ERROR
            if (data == error[errorOffset++])
            {
                if (errorOffset == errorBytes)
                {
                    systemPrintf("ERROR received\r\n");
                    return false;
                }
            }
            else
                errorOffset = 0;
        }
    }
    systemPrintf("ERROR: Timeout waiting for OK string!\r\n");
    return false;
}
