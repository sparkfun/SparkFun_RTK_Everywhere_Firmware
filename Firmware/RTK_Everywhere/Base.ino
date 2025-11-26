/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Base.ino
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

// Enough storage for two rounds of RTCM 1074,1084,1094,1124,1005
// To help prevent the "no increase in file size" and "due to lack of RTCM" glitch:
// RTCM is stored in PSRAM by the processUart1Message task and written by sendRTCMToConsumers
const uint8_t rtcmConsumerBufferEntries = 16;
const uint16_t rtcmConsumerBufferEntrySize = 1032; // RTCM can be up to 1024 + 6 bytes
uint16_t rtcmConsumerBufferLengths[rtcmConsumerBufferEntries];
volatile uint8_t rtcmConsumerBufferHead;
volatile uint8_t rtcmConsumerBufferTail;
uint8_t *rtcmConsumerBufferPtr = nullptr;

//----------------------------------------
// Allocate and initialize the rtcmConsumerBuffer
//----------------------------------------
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

//----------------------------------------
// Check how many RTCM buffers contain data
//----------------------------------------
uint8_t rtcmBuffersInUse()
{
    if (!rtcmConsumerBufferAllocated())
        return 0;

    uint8_t buffersInUse = rtcmConsumerBufferHead - rtcmConsumerBufferTail;
    if (buffersInUse >= rtcmConsumerBufferEntries) // Wrap if Tail is > Head
        buffersInUse += rtcmConsumerBufferEntries;
    return buffersInUse;
}

//----------------------------------------
// Check if any RTCM data is available
//----------------------------------------
bool rtcmDataAvailable()
{
    return (rtcmBuffersInUse() > 0);
}

//----------------------------------------
// Store each RTCM message in a PSRAM buffer
// This function gets called as each complete RTCM message comes in
// The messages are written to the servers by sendRTCMToConsumers
//----------------------------------------
void storeRTCMForConsumers(uint8_t *rtcmData, uint16_t dataLength)
{
    if (!rtcmConsumerBufferAllocated())
        return;

    // Check if a buffer is available
    if (rtcmBuffersInUse() < (rtcmConsumerBufferEntries - 1))
    {
        uint8_t *dest = rtcmConsumerBufferPtr;
        dest += (size_t)rtcmConsumerBufferEntrySize * (size_t)rtcmConsumerBufferHead;
        memcpy(dest, rtcmData, dataLength); // Store the RTCM
        rtcmConsumerBufferLengths[rtcmConsumerBufferHead] = dataLength; // Store the length
        rtcmConsumerBufferHead++; // Increment the Head
        rtcmConsumerBufferHead %= rtcmConsumerBufferEntries; // Wrap
    }
    else
    {
        if (settings.debugNtripServerRtcm && (!inMainMenu))
            systemPrintln("rtcmConsumerBuffer full. RTCM lost");
    }
}

//----------------------------------------
// Send the stored RTCM to consumers: ntripServer, LoRa and ESP-NOW
//----------------------------------------
void sendRTCMToConsumers()
{
    if (!rtcmConsumerBufferAllocated())
        return;

    while (rtcmConsumerBufferHead != rtcmConsumerBufferTail)
    {
        uint8_t *dest = rtcmConsumerBufferPtr;
        dest += (size_t)rtcmConsumerBufferEntrySize * (size_t)rtcmConsumerBufferTail;

        // NTRIP Server
        for (int serverIndex = 0; serverIndex < NTRIP_SERVER_MAX; serverIndex++)
            ntripServerSendRTCM(serverIndex, dest, rtcmConsumerBufferLengths[rtcmConsumerBufferTail]);

        // LoRa
        loraProcessRTCM(dest, rtcmConsumerBufferLengths[rtcmConsumerBufferTail]);

        // ESP-NOW
        for (uint16_t x = 0; x < rtcmConsumerBufferLengths[rtcmConsumerBufferTail]; x++)
            espNowProcessRTCM(dest[x]);

        rtcmConsumerBufferTail++; // Increment the Tail
        rtcmConsumerBufferTail %= rtcmConsumerBufferEntries; // Wrap
    }
}

//----------------------------------------
// Store data ready to be passed along to NTRIP Server, or ESP-NOW radio
// This function gets called when an RTCM packet passes parser check in processUart1Message() task
//----------------------------------------
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
