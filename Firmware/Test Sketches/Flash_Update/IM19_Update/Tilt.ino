// Reset the GNSS/IMU module ahead of entering the bootloader.
// On Flex modules, the IMU reset is tied to the GNSS reset
void imuReset()
{
    if (productVariant == RTK_TORCH)
    {
        digitalWrite(pin_GNSS_DR_Reset, LOW); // Tell UM980 and DR to reset
        delay(50);
        digitalWrite(pin_GNSS_DR_Reset, HIGH);
    }
    else if (productVariant == RTK_FACET_FP)
    {
        gpioExpanderImuReset(); // Drive the GNSS reset pin low to reset both GNSS and IMU
        delay(50);
        gpioExpanderImuBoot();
    }
    else
        systemPrintln("Uncaught imuReset()");
}

// Below are the functions necessary for firmware upgrading the IM19
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// IM19 bootloader state.
//
// im19FrameMap is a small bitmap (one bit per 256 byte frame) that mirrors what the
// IM19 last told us it received. It is required by the wire protocol itself, not an
// optimization we chose to add: after every pass the IM19 replies to FRAME_TYPE_CPL
// with a FRAME_TYPE_REQ frame whose payload IS that bitmap (see im19CheckResponse()
// / FRAME_TYPE_REQ in code/upgrade.c). Without recording it we would have no way to
// tell "fully received" from "still missing some frames", and no way to know which
// bytes to send on a retry - we'd be forced to either trust an unverified flash (risk
// of bricking the IM19) or blindly resend the whole file every retry.
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

// IM19 bootloader wire protocol (268 byte frames: 12 byte header + 256 byte payload).
// Ported from the reference implementation in code/upgrade.c.
#define IM19_FRAME_HEADER 0xAA55
#define IM19_FRAME_TYPE_BIN 0x01 // host -> IM19 : one 256 byte chunk of the firmware image
#define IM19_FRAME_TYPE_REQ 0x02 // IM19 -> host : bitmap of frames received so far (sent in response to CPL)
#define IM19_FRAME_TYPE_CPL 0x03 // host -> IM19 : "that's every frame I have, tell me what you're missing"
#define IM19_FRAME_TYPE_RDY 0x04 // host -> IM19 : "you have everything, boot it" / IM19 -> host : "already booting"
#define IM19_FRAME_PAYLOAD_SIZE 256
#define IM19_FRAME_TOTAL_SIZE 268
#define IM19_FRAME_MAP_SIZE 256 // bitmap bytes -> supports up to 2048 frames (512KB firmware image)

// Delay after each frame is put on the wire, giving the IM19 bootloader time to parse
// and flash it before the next one arrives. The wire protocol has no per-frame ACK, so
// this is a blind pacing value (ported from the vendor's SleepMs(50) in upgrade.c) -
// tune it empirically on hardware: lower it, then watch how many frames the IM19
// reports missing at the end. The existing retry path only re-fetches what's missing,
// so occasional drops are safe; a delay set too low just means more retry passes.
static const uint32_t IM19_FRAME_PACING_MS = 100; // Works - 0.1% frame failure.
// static const uint32_t IM19_FRAME_PACING_MS = 75; // Works - 42% frame failure.
// static const uint32_t IM19_FRAME_PACING_MS = 50; // Original mfg timeout. 87% frame failure.
//  static const uint32_t IM19_FRAME_PACING_MS = 30; // Works - 94% frame failure.
// static const uint32_t IM19_FRAME_PACING_MS = 15; // Works in test sketch. Partial fail in RTK Everywhere.

// How long to wait for the IM19 to reply after CPL. After the last frame lands, the
// IM19 still has to finish flashing it and scan every received frame to build its
// reply bitmap.
static const uint32_t IM19_CPL_RESPONSE_TIMEOUT_MS = 500;
static const int IM19_CPL_RESPONSE_RETRIES = 10; // up to IM19_CPL_RESPONSE_RETRIES * IM19_CPL_RESPONSE_TIMEOUT_MS total

static uint8_t *im19FrameMap = nullptr; // bit set = IM19 has confirmed receipt of that frame
static uint32_t im19TotalFrames = 0;
static uint32_t im19FileBytes = 0;
static uint32_t im19NextFrameID; // frame ID that the next assembled byte belongs to

static uint8_t rxBuffer[IM19_FRAME_PAYLOAD_SIZE];

static void im19ReleaseBuffers()
{
    if (im19FrameMap != nullptr)
    {
        free(im19FrameMap);
        im19FrameMap = nullptr;
    }
}

static bool im19AllocateBuffers()
{
    im19ReleaseBuffers();

    im19FrameMap = (uint8_t *)malloc(IM19_FRAME_MAP_SIZE);
    if (im19FrameMap == nullptr)
        return false;

    return true;
}

static uint16_t im19BufToUint16(const uint8_t *buffer)
{
    return (uint16_t)(buffer[0] | (buffer[1] << 8));
}

static uint32_t im19BufToUint32(const uint8_t *buffer)
{
    return (uint32_t)(buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24));
}

static uint32_t im19CheckSum(const uint8_t *frame)
{
    uint16_t type = im19BufToUint16(&frame[2]);
    uint32_t id = im19BufToUint32(&frame[8]);
    uint32_t check = type + id;
    for (int i = 12; i < IM19_FRAME_TOTAL_SIZE; i++)
        check += frame[i];
    return check;
}

static void im19BuildFrame(uint16_t type, uint32_t id, uint8_t *frame)
{
    frame[0] = (IM19_FRAME_HEADER >> 0) & 0xFF;
    frame[1] = (IM19_FRAME_HEADER >> 8) & 0xFF;

    frame[2] = (type >> 0) & 0xFF;
    frame[3] = (type >> 8) & 0xFF;

    frame[8] = (id >> 0) & 0xFF;
    frame[9] = (id >> 8) & 0xFF;
    frame[10] = (id >> 16) & 0xFF;
    frame[11] = (id >> 24) & 0xFF;

    uint32_t check = im19CheckSum(frame);
    frame[4] = (check >> 0) & 0xFF;
    frame[5] = (check >> 8) & 0xFF;
    frame[6] = (check >> 16) & 0xFF;
    frame[7] = (check >> 24) & 0xFF;
}

// Sends a command frame (CPL to ask what's missing, or RDY to tell the IM19 to boot).
static void im19SendCmdFrame(uint16_t cmd, uint32_t frameTotal)
{
    uint8_t frame[IM19_FRAME_TOTAL_SIZE] = {0};
    if (cmd == IM19_FRAME_TYPE_RDY)
    {
        uint32_t num = frameTotal / 8, mod = frameTotal % 8;
        for (uint32_t i = 0; i < num; i++)
            frame[12 + i] = 0xFF;
        if (mod > 0)
            frame[12 + num] = 0xFF >> (8 - mod);
    }
    im19BuildFrame(cmd, 0xFFFFFFFF, frame);
    SerialForTilt->write(frame, sizeof(frame));
    SerialForTilt->flush();
    delay(IM19_FRAME_PACING_MS);
}

// Waits for a response frame from the IM19. On FRAME_TYPE_REQ, copies the IM19's
// received-frame bitmap into frameMap. Returns the frame type, or -1 on timeout/garbage.
static int im19CheckResponse(uint8_t *frameMap, uint32_t timeoutMs)
{
    uint8_t buf[350]; // a little slack past one frame (268B) in case of a leading garbage byte
    SerialForTilt->setTimeout(timeoutMs);
    int buf_len = SerialForTilt->readBytes(buf, sizeof(buf));
    uint8_t *p = buf;

    while (buf_len >= IM19_FRAME_TOTAL_SIZE)
    {
        if (im19BufToUint16(p + 0) != IM19_FRAME_HEADER || im19BufToUint32(p + 4) != im19CheckSum(p))
        {
            p++;
            buf_len--;
            continue;
        }

        switch (im19BufToUint16(p + 2))
        {
        case IM19_FRAME_TYPE_REQ:
            if (frameMap == nullptr)
                return -1;
            memcpy(frameMap, p + 12, IM19_FRAME_MAP_SIZE);
            return IM19_FRAME_TYPE_REQ;
        case IM19_FRAME_TYPE_RDY:
            return IM19_FRAME_TYPE_RDY;
        default:
            return -1;
        }
    }
    return -1;
}

// True if every frame in [0, totalFrame) is marked present in frameMap.
static bool im19AllFramesPresent(const uint8_t *frameMap, uint32_t totalFrame)
{
    if (frameMap == nullptr)
        return false;

    for (uint32_t frame = 0; frame < totalFrame; frame++)
    {
        uint8_t bit = 0x01 << (frame % 8);
        if ((frameMap[frame / 8] & bit) == 0)
            return false;
    }
    return true;
}

static bool im19FindStr(const uint8_t *buf, int buf_len, const char *str)
{
    int str_len = strlen(str);
    for (int i = 0; i <= buf_len - str_len; i++)
    {
        if (memcmp(buf + i, str, str_len) == 0)
            return true;
    }
    return false;
}

// Sends an AT command and waits (with retries) for the expected response substring.
static bool im19SendATCommand(const char *cmd, const char *response, int retries, uint8_t *responseBuf, size_t responseBufSize,
                              int *responseLenOut)
{
    if (responseLenOut != nullptr)
        *responseLenOut = 0;

    uint8_t buf[256];
    while (retries--)
    {
        SerialForTilt->write((const uint8_t *)cmd, strlen(cmd));
        delay(50);
        SerialForTilt->setTimeout(50);
        int buf_len = SerialForTilt->readBytes(buf, sizeof(buf));
        if (buf_len > 0)
        {
            if (responseBuf != nullptr && responseBufSize > 0)
            {
                size_t copyLen = (size_t)buf_len;
                if (copyLen >= responseBufSize)
                    copyLen = responseBufSize - 1;

                memcpy(responseBuf, buf, copyLen);
                responseBuf[copyLen] = '\0';
                if (responseLenOut != nullptr)
                    *responseLenOut = (int)copyLen;
            }

            if (im19FindStr(buf, buf_len, response))
                return true;
        }
    }
    return false;
}

// Puts the IM19 into its bootloader and gets ready to receive frames for a file of
// 'fileBytes' bytes. Mallocs nothing - the frame map is a fixed, small static buffer.
bool im19UpdateFirmwareBegin(size_t fileBytes)
{
    uint32_t totalFrames = (fileBytes + IM19_FRAME_PAYLOAD_SIZE - 1) / IM19_FRAME_PAYLOAD_SIZE;
    if (totalFrames > (uint32_t)IM19_FRAME_MAP_SIZE * 8)
    {
        systemPrintf("Firmware image too large for the IM19 update protocol (%lu bytes).\r\n", fileBytes);
        return false;
    }

    if (!im19AllocateBuffers())
    {
        systemPrintln("Unable to allocate IM19 update buffers.");
        return false;
    }

    memset(im19FrameMap, 0, IM19_FRAME_MAP_SIZE);
    im19TotalFrames = totalFrames;
    im19FileBytes = fileBytes;
    im19NextFrameID = 0;

    for (int retry = 0; retry < 3; retry++)
    {
        imuReset();
        delay(1000);
        while (SerialForTilt->available()) // Ensure the RX buffer is clear
            SerialForTilt->read();
        if (im19SendATCommand("AT+UPDATE_APP\r\n", "OK", 5, nullptr, 0, nullptr))
            return true;
    }

    im19ReleaseBuffers();
    return false;
}

// Repositions the frame-assembly cursor to a frame-aligned byte offset. Used before
// streaming a retry range so its bytes land in the right frame IDs.
void im19UpdateFirmwareSeek(uint32_t byteOffset)
{
    im19NextFrameID = byteOffset / IM19_FRAME_PAYLOAD_SIZE;
}

// Feeds a chunk of firmware bytes (any length, any alignment) to the IM19. Internally
// groups them into 256 byte protocol frames and sends each as it fills.
bool im19UpdateFirmware(const uint8_t * data, uint32_t numBytes)
{
    uint8_t frame[IM19_FRAME_TOTAL_SIZE] = {0};

    // Add the payload to the frame
    memcpy(&frame[12], data, numBytes);
    if (numBytes < IM19_FRAME_PAYLOAD_SIZE)
        memset(&frame[12 + numBytes], 0, IM19_FRAME_PAYLOAD_SIZE - numBytes);
    im19BuildFrame(IM19_FRAME_TYPE_BIN, im19NextFrameID, frame);

    // Send the firmware bytes to the IM19
    SerialForTilt->write(frame, sizeof(frame));
    SerialForTilt->flush(); // Block until the frame is actually on the wire, not just queued
    delay(IM19_FRAME_PACING_MS);

    // Account for this frame
    im19NextFrameID++;
    return true;
}

// Confirms the new firmware is running by polling for a response to AT+VERSION.
static bool im19VerifyFirmwareRunning()
{
    delay(5000); // Give the IM19 time to flash and boot the new image
    for (int retry = 0; retry < 3; retry++)
    {
        if (im19SendATCommand("AT+VERSION\r\n", "Version:", 1, nullptr, 0, nullptr))
            return true;
        delay(100);
    }
    return false;
}

// Tells the IM19 "that's every frame I have" and handles its reply. Returns SUCCESS
// once the IM19 confirms it received everything and has booted the new image, RETRY
// if it reports missing frames (caller should re-request just those and call again),
// or FAILED if the IM19 never responds.
Im19UpdateResult im19UpdateFirmwareEnd()
{
    if (im19FrameMap == nullptr)
        return IM19_UPDATE_FAILED;

    im19SendCmdFrame(IM19_FRAME_TYPE_CPL, im19TotalFrames);

    int retry = IM19_CPL_RESPONSE_RETRIES;
    while (retry--)
    {
        int response = im19CheckResponse(im19FrameMap, IM19_CPL_RESPONSE_TIMEOUT_MS);

        if (response == IM19_FRAME_TYPE_RDY)
        {
            Im19UpdateResult result = im19VerifyFirmwareRunning() ? IM19_UPDATE_SUCCESS : IM19_UPDATE_FAILED;
            im19ReleaseBuffers();
            return result;
        }

        if (response == IM19_FRAME_TYPE_REQ)
        {
            if (im19AllFramesPresent(im19FrameMap, im19TotalFrames))
            {
                im19SendCmdFrame(IM19_FRAME_TYPE_RDY, im19TotalFrames);
                Im19UpdateResult result = im19VerifyFirmwareRunning() ? IM19_UPDATE_SUCCESS : IM19_UPDATE_FAILED;
                im19ReleaseBuffers();
                return result;
            }
            return IM19_UPDATE_RETRY;
        }
    }

    im19ReleaseBuffers();
    return IM19_UPDATE_FAILED;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// WiFi streaming: pulls bytes from the URL and feeds them to the IM19 update state
// machine above. A retry only re-requests (via HTTP Range) the byte ranges the IM19
// says it's still missing - the rest of the file is never re-downloaded or re-sent.
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

// Reads 'byteCount' bytes starting at 'startOffset' from an already-open HTTP stream
// and feeds them to the IM19, reporting progress as it goes.
static bool im19StreamFirmware(WiFiClient * stream,
                               uint32_t startOffset,
                               size_t fileBytes,
                               uint8_t * buffer,
                               size_t packetBytes)
{
    // Display the parameters
    if (settings.debugFirmwareUpdate && otaDebugVerbose)
    {
        systemPrintf("startOffset: %d\r\n", startOffset);
        systemPrintf("fileBytes: %d\r\n", fileBytes);
        systemPrintf("packetBytes: %d\r\n", packetBytes);
    }

    im19UpdateFirmwareSeek(startOffset);

    unsigned long lastDataTime = millis();
    size_t validData = 0;
    if (settings.debugFirmwareUpdate)
        systemPrintf("stream->connected(): %d\r\n", stream->connected());
    while (stream->connected() && (fileBytes > 0))
    {
        // Wait until some data is available
        size_t availableBytes = stream->available();
        if (availableBytes == 0)
        {
            if ((millis() - lastDataTime) > OTA_DATA_TIMEOUT)
            {
                systemPrintf("IM19 firmware update timed out waiting for data\r\n");
                break;
            }
            delay(1);
            continue;
        }
        if (settings.debugFirmwareUpdate && otaDebugVerbose)
            systemPrintf("availableBytes: %d\r\n", availableBytes);

        // Read the received data
        size_t bytesToRead = min(availableBytes, packetBytes - validData);
        int bytesRead = stream->readBytes(&buffer[validData], bytesToRead);
        if (settings.debugFirmwareUpdate && otaDebugVerbose)
            systemPrintf("bytesRead: %d\r\n", bytesRead);
        if (bytesRead <= 0)
            break;
        validData += bytesRead;

        // Fill the packet
        if ((validData < packetBytes) && (validData != fileBytes))
            continue;

        // Update this portion of the firmware
        if (im19UpdateFirmware(buffer, validData) == false)
        {
            systemPrintln("IM19 firmware update failed during write");
            break;
        }

        // Display the progress
        firmwareUpdateProgressCallback("IM19", validData);

        // Account for this data
        fileBytes -= validData;
        lastDataTime = millis();
        validData = 0;
    }

    bool success = (fileBytes == 0);
    return success;
}

// Re-downloads only [startByte, endByte] (inclusive) and streams it to the IM19.
static bool im19StreamRange(const char * url, uint32_t startByte, uint32_t endByte)
{
    WiFiClientSecure client;
    if (!otaSecurelyConnectGitHub(client))
    {
        systemPrintln("Failed to securely connect to GitHub.");
        return false;
    }

    HTTPClient http;
    if (!http.begin(client, url))
    {
        systemPrintln("Unable to begin HTTP request.");
        return false;
    }

    char rangeHeader[48];
    snprintf(rangeHeader, sizeof(rangeHeader), "bytes=%lu-%lu", (unsigned long)startByte, (unsigned long)endByte);
    http.addHeader("Range", rangeHeader);

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_PARTIAL_CONTENT)
    {
        // A 200 here means the server ignored our Range request and is about to send
        // the whole file from byte 0 - streaming that into this offset would corrupt
        // the image, so bail rather than guess.
        systemPrintf("HTTP range request failed, code: %d\r\n", httpCode);
        http.end();
        return false;
    }

    bool success = im19StreamFirmware(http.getStreamPtr(),
                                      startByte,
                                      endByte - startByte + 1,
                                      rxBuffer,
                                      sizeof(rxBuffer));
    http.end();
    return success;
}

// Walks im19FrameMap for runs of missing frames and re-requests just those byte
// ranges from the source URL, instead of re-streaming the entire firmware image.
static bool im19StreamMissingRanges(const char * url)
{
    if (im19FrameMap == nullptr)
        return false;

    uint32_t totalMissingFrames = 0;
    for (uint32_t i = 0; i < im19TotalFrames; i++)
    {
        if ((im19FrameMap[i / 8] & (0x01 << (i % 8))) == 0)
            totalMissingFrames++;
    }

    uint32_t missingRateTenthsPct = 0;
    if (im19TotalFrames > 0)
        missingRateTenthsPct = (totalMissingFrames * 1000 + (im19TotalFrames / 2)) / im19TotalFrames;

    uint32_t frame = 0;
    while (frame < im19TotalFrames)
    {
        uint8_t bit = 0x01 << (frame % 8);
        if (im19FrameMap[frame / 8] & bit)
        {
            frame++;
            continue;
        }

        uint32_t runStart = frame;
        while (frame < im19TotalFrames && !(im19FrameMap[frame / 8] & (0x01 << (frame % 8))))
            frame++;

        uint32_t startByte = runStart * IM19_FRAME_PAYLOAD_SIZE;
        uint32_t endByte = min(frame * IM19_FRAME_PAYLOAD_SIZE, im19FileBytes) - 1;

        systemPrintf("Requesting missing frames %lu-%lu (%lu bytes) from source (failure rate: %lu.%lu%%).\r\n",
                     (unsigned long)runStart, (unsigned long)(frame - 1), (unsigned long)(endByte - startByte + 1),
                     (unsigned long)(missingRateTenthsPct / 10), (unsigned long)(missingRateTenthsPct % 10));

        if (!im19StreamRange(url, startByte, endByte))
            return false;
    }
    return true;
}

// Updates the IM19 module firmware from the given URL over WiFi.
//
// Structure (see the header comment at the top of the .ino for the general pattern):
//   1. Connect to WiFi.
//   2. im19UpdateFirmwareBegin() puts the IM19 into its bootloader.
//   3. Stream the file once, feeding chunks to im19UpdateFirmware().
//   4. im19UpdateFirmwareEnd() asks the IM19 what it's missing. If anything, re-request
//      only those byte ranges (im19StreamMissingRanges) and ask again - up to a few
//      attempts - rather than re-streaming the whole binary.
bool im19FirmwareUpdate(const char * url)
{
    WiFiClientSecure client;
    if (!otaSecurelyConnectGitHub(client))
    {
        systemPrintln("Failed to securely connect to GitHub.");
        return false;
    }

    if(settings.debugFirmwareUpdate)
        systemPrintf("URL: %s\r\n", url);

    HTTPClient http;
    if (!http.begin(client, url))
    {
        systemPrintln("Unable to begin HTTP request.");
        return false;
    }

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK)
    {
        systemPrintf("HTTP GET failed, code: %d\r\n", httpCode);
        http.end();
        return false;
    }

    size_t fileBytes = http.getSize();
    if (fileBytes <= 0)
    {
        systemPrintln("Server did not report a firmware size.");
        http.end();
        return false;
    }
    firmwareUpdateBytesToProcess = fileBytes;

    if (!im19UpdateFirmwareBegin(fileBytes))
    {
        systemPrintln("IM19 did not respond to the bootloader entry command.");
        http.end();
        return false;
    }

    // Now that the IM19 is in its bootloader and waiting, stream the already-open
    // response body straight to it.
    bool streamed = im19StreamFirmware(http.getStreamPtr(),
                                       0,
                                       fileBytes,
                                       rxBuffer,
                                       sizeof(rxBuffer));
    http.end();

    if (!streamed)
    {
        systemPrintln("Firmware update failed during initial WiFi download.");
        im19ReleaseBuffers();
        return false;
    }

    const int maxAttempts = 5;
    for (int attempt = 1; attempt <= maxAttempts; attempt++)
    {
        Im19UpdateResult result = im19UpdateFirmwareEnd();

        if (result == IM19_UPDATE_SUCCESS)
        {
            return true;
        }

        if (result == IM19_UPDATE_FAILED)
        {
            systemPrintln("IM19 firmware update failed: no response from IM19.");
            return false;
        }

        // IM19_UPDATE_RETRY - the IM19 told us exactly which frames it's missing.
        systemPrintf("Attempt %d: IM19 reports missing frames.\r\n", attempt);
        if (!im19StreamMissingRanges(url))
        {
            systemPrintln("Firmware update failed while re-requesting missing frames.");
            im19ReleaseBuffers();
            return false;
        }
    }

    systemPrintln("IM19 firmware update failed: too many retries.");
    im19ReleaseBuffers();
    return false;
}

// Sends AT+VERSION and copies the returned "Version:" line into versionOut.
// Returns true if "Version:" is seen in the response
bool im19GetVersionString(char *versionOut, size_t versionOutSize)
{
    if (versionOut == nullptr || versionOutSize < 2)
        return false;

    versionOut[0] = '\0';
    uint8_t responseBuf[256];

    if (!im19SendATCommand("AT+VERSION\r\n", "Version:", 3, responseBuf, sizeof(responseBuf), nullptr))
        return false;

    char *versionStart = strstr((char *)responseBuf, "Version:");
    if (versionStart == nullptr)
        return false;

    char *lineEnd = strchr(versionStart, '\r');
    if (lineEnd == nullptr)
        lineEnd = strchr(versionStart, '\n');

    size_t copyLen = lineEnd != nullptr ? (size_t)(lineEnd - versionStart) : strlen(versionStart);
    if (copyLen >= versionOutSize)
        copyLen = versionOutSize - 1;

    memcpy(versionOut, versionStart, copyLen);
    versionOut[copyLen] = '\0';
    return true;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// End of IM19 firmware update functions.
