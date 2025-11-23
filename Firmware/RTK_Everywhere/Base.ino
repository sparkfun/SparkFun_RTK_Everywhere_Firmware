// Enough storage for two rounds of RTCM 1074,1084,1094,1124,1005
// To prevent the "no increase in file size" and "due to lack of RTCM" glitch:
// RTCM is stored in PSRAM by the Uart1 task and written by tripServerUpdate
const uint8_t rtcmConsumerBufferEntries = 16;
const uint16_t rtcmConsumerBufferEntrySize = 1032; // RTCM can be up to 1024 + 6 bytes
uint16_t rtcmConsumerBufferLengths[rtcmConsumerBufferEntries];
uint8_t rtcmConsumerBufferHead;
uint8_t rtcmConsumerBufferTail;
uint8_t *rtcmConsumerBufferPtr = nullptr;

bool rtcmConsumerBufferAllocated()
{
    if (rtcmConsumerBufferPtr == nullptr)
    {
        rtcmConsumerBufferPtr = (uint8_t *)rtkMalloc(
            (size_t)rtcmConsumerBufferEntrySize * (size_t)rtcmConsumerBufferEntries,
            "ntripBuffer");
        for (uint8_t i = 0; i < rtcmConsumerBufferEntries; i++)
            rtcmConsumerBufferLengths[i] = 0;
        rtcmConsumerBufferHead = 0;
        rtcmConsumerBufferTail = 0;
    }
    if (rtcmConsumerBufferPtr == nullptr)
    {
        systemPrintln("rtcmConsumerBuffer malloc failed!");
        return false;
    }

    return true;
}

void storeRTCMForConsumers(uint8_t *rtcmData, uint16_t dataLength)
{
    // Store each RTCM message in a PSRAM buffer
    // The messages are written to the servers by ntripServerUpdate

    if (!rtcmConsumerBufferAllocated())
        return;

    // Check if a buffer is available
    uint8_t buffersInUse;
    if (rtcmConsumerBufferHead == rtcmConsumerBufferTail) // All buffers are empty
        buffersInUse = 0;
    else if (rtcmConsumerBufferHead > rtcmConsumerBufferTail)
        buffersInUse = rtcmConsumerBufferHead - rtcmConsumerBufferTail;
    else // rtcmConsumerBufferHead < rtcmConsumerBufferTail
        buffersInUse = (rtcmConsumerBufferEntries + rtcmConsumerBufferHead) - rtcmConsumerBufferTail;
    if (buffersInUse < (rtcmConsumerBufferEntries - 1))
    {
        uint8_t *dest = rtcmConsumerBufferPtr;
        dest += (size_t)rtcmConsumerBufferEntrySize * (size_t)rtcmConsumerBufferHead;
        memcpy(dest, rtcmData, dataLength); // Store the RTCM
        rtcmConsumerBufferLengths[rtcmConsumerBufferHead] = dataLength;
        rtcmConsumerBufferHead++;
        rtcmConsumerBufferHead %= rtcmConsumerBufferEntries;
    }
    else
    {
        if (settings.debugNtripServerRtcm && (!inMainMenu))
            systemPrintln("rtcmConsumerBuffer full. RTCM lost");
    }
}

void sendRTCMToConsumers()
{
    if (!rtcmConsumerBufferAllocated())
        return;

    while (rtcmConsumerBufferHead != rtcmConsumerBufferTail)
    {
        uint8_t *dest = rtcmConsumerBufferPtr;
        dest += (size_t)rtcmConsumerBufferEntrySize * (size_t)rtcmConsumerBufferTail;

        for (int serverIndex = 0; serverIndex < NTRIP_SERVER_MAX; serverIndex++)
            ntripServerSendRTCM(serverIndex, dest, rtcmConsumerBufferLengths[rtcmConsumerBufferTail]);

        loraProcessRTCM(dest, rtcmConsumerBufferLengths[rtcmConsumerBufferTail]);

        for (uint16_t x = 0; x < rtcmConsumerBufferLengths[rtcmConsumerBufferTail]; x++)
            espNowProcessRTCM(dest[x]);

        rtcmConsumerBufferTail++;
        rtcmConsumerBufferTail %= rtcmConsumerBufferEntries;
    }
}

// This function gets called when an RTCM packet passes parser check in processUart1Message() task
// Pass data along to NTRIP Server, or ESP-NOW radio
void processRTCM(uint8_t *rtcmData, uint16_t dataLength)
{
    storeRTCMForConsumers(rtcmData, dataLength);

    rtcmLastPacketSent = millis();
    rtcmPacketsSent++;

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
}

//------------------------------
// When settings.baseCasterOverride is true, override enableTcpServer, tcpServerPort, and enableNtripCaster settings
//------------------------------
void baseCasterEnableOverride()
{
    settings.baseCasterOverride = true;
}

void baseCasterDisableOverride()
{
    settings.baseCasterOverride = false;
}
