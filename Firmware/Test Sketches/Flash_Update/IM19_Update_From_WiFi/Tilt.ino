// Below are the functions necessary for firmware upgrading the IM19
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

const uint16_t IM19_FRAME_HEADER = 0xAA55;
const uint16_t IM19_FRAME_TYPE_BIN = 0x01;
const uint16_t IM19_FRAME_TYPE_REQ = 0x02;
const uint16_t IM19_FRAME_TYPE_CPL = 0x03;
const uint16_t IM19_FRAME_TYPE_RDY = 0x04;
const int IM19_FRAME_PAYLOAD_SIZE = 256;
const int IM19_FRAME_SIZE = 12 + IM19_FRAME_PAYLOAD_SIZE; // 12-byte frame header + 256-byte payload
const int IM19_FRAME_MAP_SIZE = 256;                 // 2048 bits -> max 2048 frames (512KB firmware) we are able to track

uint8_t *im19FrameMap = nullptr; // Tracks individual frames. Firmware completes when all frames are marked received.

// Frames are 256 bytes max (transfers require 12 more bytes). IM19_FRAME_TYPE_REQ resends are handled by
// having the caller re-stream the source data on another pass; frames already
// marked received in im19FrameMap are skipped rather than resent.
uint8_t *im19FramePayloadBuf = nullptr;
uint32_t im19FramePayloadLen = 0; // bytes currently held in im19FramePayloadBuf
uint32_t im19CurrentFrameID = 0;  // frame ID the assembly buffer belongs to

// Resets frame-assembly state at the start of a streaming pass.
void im19ResetFrameAssembly()
{
    im19FramePayloadLen = 0;
    im19CurrentFrameID = 0;
}

// Reads up to size bytes from IMU serial within timeout and returns bytes read or -1.
int im19ReceiveTimeout(uint8_t *buffer, int size, int timeoutMs)
{
    unsigned long start = millis();
    int len = 0;
    while ((millis() - start) < (unsigned long)timeoutMs && len < size)
    {
        if (uart2Serial.available())
            buffer[len++] = uart2Serial.read();
    }
    return len > 0 ? len : -1;
}

// Returns true if the byte buffer contains the given string sequence.
bool im19FindStr(const uint8_t *buf, int bufLen, const char *str)
{
    int strLen = strlen(str);
    for (int i = 0; i <= bufLen - strLen; i++)
    {
        if (memcmp(buf + i, str, strLen) == 0)
            return true;
    }
    return false;
}

// Sends an AT command and waits for a matching response substring.
bool im19SendCommand(const char *cmd, const char *response)
{
    uint8_t buf[128];
    uart2Serial.write(cmd, strlen(cmd));
    delay(50);

    int retry = 5;
    while (retry--)
    {
        int len = im19ReceiveTimeout(buf, sizeof(buf), 50);
        if (len > 0 && im19FindStr(buf, len, response))
            return true;
    }
    return false;
}

// Sends AT+VERSION and copies the returned "Version:" line into versionOut.
// Returns true if "Version:" is seen in the response
bool im19GetVersionString(char *versionOut, size_t versionOutSize)
{
    if (versionOut == nullptr || versionOutSize < 2)
        return false;

    versionOut[0] = '\0';
    const char CMD_VERSION[] = "AT+VERSION\r\n";

    uint8_t buf[256];
    int retry = 3;
    while (retry--)
    {
        uart2Serial.write(CMD_VERSION, strlen(CMD_VERSION));
        delay(50);

        int totalLen = 0;
        unsigned long start = millis();
        while ((millis() - start) < 500 && totalLen < (int)sizeof(buf) - 1)
        {
            int len = im19ReceiveTimeout(buf + totalLen, sizeof(buf) - 1 - totalLen, 80);
            if (len > 0)
            {
                totalLen += len;
                if (im19FindStr(buf, totalLen, "Version:"))
                    break;
            }
        }

        if (totalLen <= 0)
        {
            delay(100);
            continue;
        }

        buf[totalLen] = '\0';
        char *versionStart = strstr((char *)buf, "Version:");
        if (versionStart != nullptr)
        {
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

        delay(100);
    }

    return false;
}

// Requests a soft reset of the IM19 module.
void im19Reset()
{
    const char CMD_RESET[] = "AT+SYSTEM_RESET\r\n";
    uart2Serial.write(CMD_RESET, strlen(CMD_RESET));
}

// Decodes a little-endian uint16 from a byte buffer.
uint16_t im19BufToUint16(const uint8_t *b)
{
    return (uint16_t)(b[0] | (b[1] << 8));
}

// Decodes a little-endian uint32 from a byte buffer.
uint32_t im19BufToUint32(const uint8_t *b)
{
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

// Computes protocol checksum for a 268-byte frame.
uint32_t im19FrameChecksum(const uint8_t *frame)
{
    uint16_t type = im19BufToUint16(&frame[2]);
    uint32_t id = im19BufToUint32(&frame[8]);
    uint32_t check = (uint32_t)type + id;

    for (int i = 12; i < IM19_FRAME_SIZE; i++)
        check += frame[i];

    return check;
}

// Builds a protocol frame header and writes its checksum.
void im19BuildFrame(uint16_t type, uint32_t id, uint8_t *frame)
{
    frame[0] = IM19_FRAME_HEADER & 0xFF;
    frame[1] = (IM19_FRAME_HEADER >> 8) & 0xFF;
    frame[2] = type & 0xFF;
    frame[3] = (type >> 8) & 0xFF;
    frame[8] = id & 0xFF;
    frame[9] = (id >> 8) & 0xFF;
    frame[10] = (id >> 16) & 0xFF;
    frame[11] = (id >> 24) & 0xFF;

    uint32_t check = im19FrameChecksum(frame);
    frame[4] = check & 0xFF;
    frame[5] = (check >> 8) & 0xFF;
    frame[6] = (check >> 16) & 0xFF;
    frame[7] = (check >> 24) & 0xFF;
}

// Sends a command frame, optionally priming the frame bitmap for RDY.
void im19SendCmdFrame(uint16_t cmd, uint32_t frameTotal)
{
    uint8_t frame[IM19_FRAME_SIZE] = {0};
    if (cmd == IM19_FRAME_TYPE_RDY)
    {
        uint32_t num = frameTotal / 8, mod = frameTotal % 8;
        for (uint32_t i = 0; i < num; i++)
            frame[12 + i] = 0xFF;
        if (mod > 0)
            frame[12 + num] = 0xFF >> (8 - mod);
    }
    im19BuildFrame(cmd, 0xFFFFFFFF, frame);

    uart2Serial.write(frame, IM19_FRAME_SIZE);

    delay(50);
}

// Builds and sends one frame to the IM19 to be recorded.
void im19SendFrameBytes(const uint8_t *payload, uint32_t payloadLen, uint32_t frameID)
{
    uint8_t frame[IM19_FRAME_SIZE] = {0};

    memcpy(&frame[12], payload, payloadLen); // remaining bytes stay zero-padded

    im19BuildFrame(IM19_FRAME_TYPE_BIN, frameID, frame);

    uart2Serial.write(frame, IM19_FRAME_SIZE);

    firmwareUpdateProgressCallback((uint16_t)payloadLen);
    delay(50);
}

// Returns IM19_FRAME_TYPE_REQ, IM19_FRAME_TYPE_RDY, or -1 (no/invalid response).
// Receives and validates response frames from the IM19 update protocol.
int im19CheckResponse()
{
    uint8_t buf[350];
    int bufLen = im19ReceiveTimeout(buf, sizeof(buf), 50);
    if (bufLen < 0)
        return -1;

    uint8_t *p = buf;
    while (bufLen >= IM19_FRAME_SIZE)
    {
        if (im19BufToUint16(p) != IM19_FRAME_HEADER || im19BufToUint32(p + 4) != im19FrameChecksum(p))
        {
            p++;
            bufLen--;
            continue;
        }

        uint16_t type = im19BufToUint16(p + 2);
        if (type == IM19_FRAME_TYPE_REQ)
        {
            if (im19FrameMap == nullptr)
                return -1;
            memcpy(im19FrameMap, p + 12, IM19_FRAME_MAP_SIZE);
            return IM19_FRAME_TYPE_REQ;
        }
        else if (type == IM19_FRAME_TYPE_RDY)
        {
            return IM19_FRAME_TYPE_RDY;
        }
        else
        {
            return -1;
        }
    }
    return -1;
}

// Returns true when all frames up to totalFrame are marked received.
bool im19CheckLostFrame(uint32_t totalFrame)
{
    if (im19FrameMap == nullptr)
        return false;

    uint32_t frameNumber = 0;
    for (int i = 0; i < IM19_FRAME_MAP_SIZE; i++)
    {
        uint8_t byte = im19FrameMap[i];
        for (int j = 0; j < 8; j++)
        {
            if (frameNumber >= totalFrame)
                return true;
            if ((byte >> j) & 0x01)
                frameNumber++;
            else
                return false;
        }
    }
    return true;
}

// Resets the module and attempts to enter application update mode.
bool im19FirmwareUpdateBegin()
{
    im19ResetFrameAssembly();

    int retry = 3;
    while (retry--)
    {
        im19Reset();
        delay(1000);
        if (im19SendCommand("AT+UPDATE_APP", "OK"))
        {
            if (im19FrameMap != nullptr)
            {
                free(im19FrameMap);
                im19FrameMap = nullptr;
            }

            im19FrameMap = (uint8_t *)malloc(IM19_FRAME_MAP_SIZE);
            if (im19FrameMap == nullptr)
            {
                systemPrintln("Failed to allocate im19FrameMap.");
                return false;
            }
            memset(im19FrameMap, 0, IM19_FRAME_MAP_SIZE);

            im19FramePayloadBuf = (uint8_t *)malloc(IM19_FRAME_PAYLOAD_SIZE);
            if (im19FramePayloadBuf == nullptr)
            {
                systemPrintln("Failed to allocate im19FramePayloadBuf.");
                return false;
            }
            return true;
        }
    }
    return false;
}

// Verifies the firmware is running by checking for a version response.
bool im19CheckFirmwareRunning()
{
    bool response = false;
    for (int x = 0; x < 50; x++)
    {
        delay(100);
        char versionLine[96];
        response = im19GetVersionString(versionLine, sizeof(versionLine));
        if (response == true)
            break;
    }
    return response;
}

// Streams firmware bytes, assembling and sending a IM19_FRAME_PAYLOAD_SIZE
// frame at a time. Frames already marked received in im19FrameMap (from a
// previous pass) are skipped rather than resent. Call im19FirmwareUpdateEndPass()
// once all bytes for a pass have been given.
bool im19FirmwareUpdate(const uint8_t *data, uint32_t length)
{
    if (im19FrameMap == nullptr)
    {
        systemPrintln("im19FrameMap not allocated. Call im19FirmwareUpdateBegin first.");
        return false;
    }

    if (length == 0)
        return true;

    if (data == nullptr)
    {
        systemPrintln("im19FirmwareUpdate called with null data and non-zero length.");
        return false;
    }

    uint32_t srcOffset = 0;
    while (srcOffset < length)
    {
        uint32_t space = IM19_FRAME_PAYLOAD_SIZE - im19FramePayloadLen;
        uint32_t take = min(space, length - srcOffset);

        memcpy(im19FramePayloadBuf + im19FramePayloadLen, data + srcOffset, take);
        im19FramePayloadLen += take;
        srcOffset += take;

        if (im19FramePayloadLen == IM19_FRAME_PAYLOAD_SIZE)
        {
            uint8_t bit = 0x01 << (im19CurrentFrameID % 8);
            if ((im19FrameMap[im19CurrentFrameID / 8] & bit) != bit)
                im19SendFrameBytes(im19FramePayloadBuf, im19FramePayloadLen, im19CurrentFrameID);

            im19CurrentFrameID++;
            im19FramePayloadLen = 0;
        }
    }

    return true;
}

// Finalizes one streaming pass: flushes any partial frame, tells the
// IM19 the image is complete, and checks its response. On IM19_UPDATE_RETRY,
// frame-assembly state is reset and the caller should re-stream the full
// source data (already-received frames are tracked in im19FrameMap and will be
// skipped, not resent).
int im19FirmwareUpdateEndPass()
{
    if (im19FrameMap == nullptr)
    {
        systemPrintln("im19FrameMap not allocated. Call im19FirmwareUpdateBegin first.");
        return IM19_UPDATE_FAILED;
    }

    if (im19FramePayloadLen > 0)
    {
        uint8_t bit = 0x01 << (im19CurrentFrameID % 8);
        if ((im19FrameMap[im19CurrentFrameID / 8] & bit) != bit)
            im19SendFrameBytes(im19FramePayloadBuf, im19FramePayloadLen, im19CurrentFrameID);
        im19CurrentFrameID++;
        im19FramePayloadLen = 0;
    }

    const uint32_t totalFrames = im19CurrentFrameID;
    if (totalFrames == 0)
    {
        systemPrintln("No firmware bytes streamed.");
        return IM19_UPDATE_FAILED;
    }

    im19SendCmdFrame(IM19_FRAME_TYPE_CPL, totalFrames);

    int retry = 5;
    bool lostFrames = false;
    while (retry--)
    {
        int response = im19CheckResponse();
        if (response == IM19_FRAME_TYPE_RDY)
        {
            systemPrintln("Device acknowledged complete image, verifying boot...");
            return im19CheckFirmwareRunning() ? IM19_UPDATE_SUCCESS : IM19_UPDATE_FAILED;
        }
        else if (response == IM19_FRAME_TYPE_REQ)
        {
            if (im19CheckLostFrame(totalFrames))
            {
                im19SendCmdFrame(IM19_FRAME_TYPE_RDY, totalFrames);
            }
            else
            {
                lostFrames = true;
                break;
            }
        }
    }

    systemPrintln(lostFrames ? "Resending missing frames" : "No response, retrying");
    im19ResetFrameAssembly();
    return IM19_UPDATE_RETRY;
}

// Frees buffers allocated by im19FirmwareUpdateBegin.
void im19FirmwareUpdateEnd()
{
    if (im19FrameMap != nullptr)
    {
        free(im19FrameMap);
        im19FrameMap = nullptr;
    }
    if (im19FramePayloadBuf != nullptr)
    {
        free(im19FramePayloadBuf);
        im19FramePayloadBuf = nullptr;
    }
    im19ResetFrameAssembly();
}
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// End