

// This function gets called from the SparkFun u-blox Arduino Library.
// As each RTCM byte comes in you can specify what to do with it
// Useful for passing the RTCM correction data to a radio, Ntrip broadcaster, etc.
void DevUBLOXGNSS::processRTCM(uint8_t incoming)
{
    // We need to prevent ntripServerProcessRTCM from writing data via Ethernet (SPI W5500)
    // during an SPI checkUbloxSpi...
    // We can pass incoming to ntripServerProcessRTCM if the GNSS is I2C or the variant does not have Ethernet.
    // For the Ref Stn, processRTCMBuffer is called manually from inside ntripServerUpdate
    if ((USE_SPI_GNSS) && (HAS_ETHERNET))
        return;

    // Check for too many digits
    if (settings.enableResetDisplay == true)
    {
        if (rtcmPacketsSent > 99)
            rtcmPacketsSent = 1; // Trim to two digits to avoid overlap
    }
    else
    {
        if (rtcmPacketsSent > 999)
            rtcmPacketsSent = 1; // Trim to three digits to avoid log icon and increasing bar
    }

    // Determine if we should check this byte with the RTCM checker or simply pass it along
    bool passAlongIncomingByte = true;

    if (settings.enableRtcmMessageChecking == true)
        passAlongIncomingByte &= checkRtcmMessage(incoming);

    // Give this byte to the various possible transmission methods
    if (passAlongIncomingByte)
    {
        rtcmLastReceived = millis();
        rtcmBytesSent++;

        ntripServerProcessRTCM(incoming);

        espnowProcessRTCM(incoming);
    }
}

// For Ref Stn (USE_SPI_GNSS and HAS_ETHERNET), call ntripServerProcessRTCM manually if there is RTCM data in the buffer
void processRTCMBuffer()
{
    if ((USE_I2C_GNSS) || (!HAS_ETHERNET))
        return;

    // Check if there is any data waiting in the RTCM buffer
    uint16_t rtcmBytesAvail = gnssRtcmBufferAvailable();
    if (rtcmBytesAvail > 0)
    {
        // Check for too many digits
        if (settings.enableResetDisplay == true)
        {
            if (rtcmPacketsSent > 99)
                rtcmPacketsSent = 1; // Trim to two digits to avoid overlap
        }
        else
        {
            if (rtcmPacketsSent > 999)
                rtcmPacketsSent = 1; // Trim to three digits to avoid log icon and increasing bar
        }

        while (rtcmBytesAvail > 0)
        {
            uint8_t incoming;

            if (gnssRtcmRead(&incoming, 1) != 1)
                return;

            rtcmBytesAvail--;

            // Data in the u-blox library RTCM buffer is pre-checked. We don't need to check it again here.

            rtcmLastReceived = millis();
            rtcmBytesSent++;

            ntripServerProcessRTCM(incoming);

            espnowProcessRTCM(incoming);
        }
    }
}
