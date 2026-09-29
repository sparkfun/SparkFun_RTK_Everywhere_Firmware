/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Mosaic_Detect.ino

  Find a mosaic-X5 on the Facet FP GNSS UART, the way the firmware does.

  Copied from RTK_Everywhere/GNSS_Mosaic.ino: mosaicIsPresentOnFacetFP(),
  GNSS_MOSAIC::isPresentOnSerial() and GNSS_MOSAIC::sendWithResponse(HardwareSerial *, ...).
  BOOTSTRAP CHANGES: the two class methods are free functions (mosaicIsPresentOnSerial(),
  mosaicSendWithResponse()) without the _isBlocking / inMainMenu handling, and the test
  uses serialGNSS (ESP32 UART1) in place of a local HardwareSerial(2) on the same pins,
  because the mosaic update code uses serialGNSS. See OTA_Bootstrap_Notes.md.
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

//----------------------------------------
// Send message. Wait for up to timeout millis for reply to arrive
// If the reply has started to be received when timeout is reached, wait for a further wait millis
// If the reply is seen, wait for a further wait millis
// Returns true if the reply was received
//----------------------------------------
bool mosaicSendWithResponse(HardwareSerial *serialPort, const char *message, const char *reply,
                            unsigned long timeout, unsigned long wait)
{
    if (strlen(reply) == 0) // Reply can't be zero-length
        return false;

    if (settings.debugGnss == true)
        systemPrintf("sendWithResponse: sending %s\r\n", message);

    if (strlen(message) > 0)
        serialPort->write(message, strlen(message)); // Send the message

    unsigned long startTime = millis();
    size_t replySeen = 0;
    bool keepGoing = true;

    while ((keepGoing) && (replySeen < strlen(reply))) // While not timed out and reply not seen
    {
        if (serialPort->available()) // If a char is available
        {
            uint8_t c = serialPort->read(); // Read it
            if (c == *(reply + replySeen))  // Is it a char from reply?
                replySeen++;
            else
                replySeen = 0; // Reset replySeen on an unexpected char
        }

        // If the reply has started to arrive at the timeout, allow extra time
        if ((millis() - startTime) > timeout) // Have we timed out?
            if (replySeen == 0)               // If replySeen is zero, don't keepGoing
                keepGoing = false;

        if ((millis() - startTime) > (timeout + wait)) // Have we really timed out?
            keepGoing = false;                         // Don't keepGoing
    }

    if (replySeen == strlen(reply)) // If the reply was seen
    {
        startTime = millis();
        while ((millis() - startTime) < wait)
        {
            if (serialPort->available())
                serialPort->read();
        }
        return true;
    }
    return false;
}

//----------------------------------------
// Return true if the receiver is detected (GNSS_MOSAIC::isPresentOnSerial(), without the
// soft reset option, which mosaicIsPresentOnFacetFP() does not use)
//----------------------------------------
bool mosaicIsPresentOnSerial(HardwareSerial *serialPort, const char *command, const char *response,
                             const char *console, int retryLimit, unsigned long commandTimeout,
                             unsigned long consoleTimeout)
{
    // Mosaic could still be starting up, so allow many retries
    for (int retries = 0; retries <= retryLimit; retries++)
    {
        if (mosaicSendWithResponse(serialPort, command, response, commandTimeout, 25))
            return (true);
        mosaicSendWithResponse(serialPort, "SSSSSSSSSSSSSSSSSSSS\n\r", console, consoleTimeout, 25); // Escape sequence
    }
    return (false);
}

//----------------------------------------
// Test for mosaic on UART1 of the ESP32 on Facet FP (GNSS_Mosaic.ino mosaicIsPresentOnFacetFP())
// Leaves COM1 at 460800 and saves that to the receiver's boot configuration, like the firmware
//----------------------------------------
bool mosaicIsPresentOnFacetFP()
{
    // Check with 115200 initially. If that succeeds, increase to 460800
    beginGnssUart(115200);

    // With 3 retries:
    //   With X5 firmware 4.14.4:    isPresentOnSerial detects the GNSS - just
    //   With X5 firmware 4.14.10.1: isPresentOnSerial fails to detect the GNSS
    // With 5 retries:
    //   With X5 firmware 4.14.10.1: isPresentOnSerial detects the GNSS

    // Only try 5 times. LG290P detection will have been done first. X5 should have booted. Baud rate could be wrong.
    if (mosaicIsPresentOnSerial(serialGNSS, "sdio,COM1,auto,RTCMv3+SBF+NMEA+Encapsulate\n\r", "DataInOut", "COM1>",
                                5, 1000, 100) == true)
    {
        if (settings.debugGnss)
            systemPrintln("mosaic-X5 detected at 115200 baud");
    }
    else
    {
        if (settings.debugGnss)
            systemPrintln("mosaic-X5 not detected at 115200 baud. Trying 460800");
    }

    // Now increase the baud rate to 460800
    // The baud rate change is immediate. The response is returned at the new baud rate
    serialGNSS->write("scs,COM1,baud460800,bits8,No,bit1,none\n\r");

    delay(1000);
    beginGnssUart(460800);

    // Only try 5 times, so we fail and pass on to the next Facet GNSS detection
    if (mosaicIsPresentOnSerial(serialGNSS, "sdio,COM1,auto,RTCMv3+SBF+NMEA+Encapsulate\n\r", "DataInOut", "COM1>",
                                5, 1000, 100) == true)
    {
        // Save the configuration so the new baud rate survives a reset
        unsigned long start = millis();
        bool result = mosaicSendWithResponse(serialGNSS, "eccf,Current,Boot\n\r", "CopyConfigFile", 5000, 25);
        if (settings.debugGnss)
            systemPrintf("saveConfiguration: sendWithResponse returned %s after %d ms\r\n",
                         result ? "true" : "false", millis() - start);
        return result;
    }
    return false;
}
