/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Tilt.ino
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

IM19 * tiltSensor;

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

static uint8_t im19FrameMap[IM19_FRAME_MAP_SIZE]; // bit set = IM19 has confirmed receipt of that frame
static uint32_t im19TotalFrames;
static uint32_t im19NextFrameID;

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

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
static bool im19SendATCommand(const char *cmd, const char *response, int retries)
{
    uint8_t buf[256];
    while (retries--)
    {
        SerialForTilt->write((const uint8_t *)cmd, strlen(cmd));
        delay(50);
        SerialForTilt->setTimeout(50);
        int buf_len = SerialForTilt->readBytes(buf, sizeof(buf));
        if ((buf_len > 0) && im19FindStr(buf, buf_len, response))
            return true;
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

    memset(im19FrameMap, 0, IM19_FRAME_MAP_SIZE);
    im19TotalFrames = totalFrames;
    otaFileBytes = fileBytes;

    for (int retry = 0; retry < 3; retry++)
    {
        imuReset();
        delay(1000);
        while (SerialForTilt->available()) // Ensure the RX buffer is clear
            SerialForTilt->read();
        if (im19SendATCommand("AT+UPDATE_APP\r\n", "OK", 5))
            return true;
    }
    return false;
}

// Feeds a chunk of firmware bytes (any length, any alignment) to the IM19. Internally
// groups them into 256 byte protocol frames and sends each as it fills.
bool im19UpdateFirmware(const uint8_t * data, uint32_t numBytes)
{
    uint8_t frame[IM19_FRAME_TOTAL_SIZE] = {0};

    // Test the retry mechanism
    if (settings.debugFirmwareUpdate && otaDebugVerbose)
        systemPrintf("Frame #: %d, %d bytes\r\n", im19NextFrameID, numBytes);
    if ((previousBadBlocks < previousBadBlocksEnd)
        && (*previousBadBlocks == im19NextFrameID))
    {
        if (settings.debugFirmwareUpdate && !otaDebugVerbose)
            systemPrintf("Frame #: %d, %d bytes\r\n", im19NextFrameID, numBytes);
        previousBadBlocks += 1;
    }
    if ((badBlocks < badBlocksEnd) && (*badBlocks == im19NextFrameID))
    {
        if (settings.debugFirmwareUpdate)
            systemPrintf("Dropping frame # %d, %d bytes\r\n", im19NextFrameID, numBytes);
        badBlocks += 1;
        im19NextFrameID++;
        return true;
    }

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

// Tells the IM19 "that's every frame I have" and handles its reply. Returns SUCCESS
// once the IM19 confirms it received everything and has booted the new image, RETRY
// if it reports missing frames (caller should re-request just those and call again),
// or FAILED if the IM19 never responds.
Im19UpdateResult im19UpdateFirmwareEnd()
{
    // Select the next set of bad blocks
    previousBadBlocks = badBlocks;
    previousBadBlocksEnd = badBlocks;
    badBlocks = nextBadBlocks;
    badBlocksEnd = nextBadBlocksEnd;
    nextBadBlocks = nullptr;
    nextBadBlocksEnd = nullptr;

    im19SendCmdFrame(IM19_FRAME_TYPE_CPL, im19TotalFrames);

    int retry = IM19_CPL_RESPONSE_RETRIES;
    while (retry--)
    {
        int response = im19CheckResponse(im19FrameMap, IM19_CPL_RESPONSE_TIMEOUT_MS);

        if (response == IM19_FRAME_TYPE_RDY)
            return im19VerifyFirmwareRunning() ? IM19_UPDATE_SUCCESS : IM19_UPDATE_FAILED;

        if (response == IM19_FRAME_TYPE_REQ)
        {
            if (im19AllFramesPresent(im19FrameMap, im19TotalFrames))
            {
                im19SendCmdFrame(IM19_FRAME_TYPE_RDY, im19TotalFrames);
                return im19VerifyFirmwareRunning() ? IM19_UPDATE_SUCCESS : IM19_UPDATE_FAILED;
            }
            return IM19_UPDATE_RETRY;
        }
    }
    return IM19_UPDATE_FAILED;
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// WiFi streaming: pulls bytes from the URL and feeds them to the IM19 update state
// machine above. A retry only re-requests (via HTTP Range) the byte ranges the IM19
// says it's still missing - the rest of the file is never re-downloaded or re-sent.
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

//----------------------------------------
// Reads packetBytes from an already-open HTTP stream and feeds them to the device,
// reporting progress as it goes.
//
// The generic process is:
// 1) Call the updateFirmwareBegin function to erase the flash on the device
// 2) Call firmwareUpdateProgressReset to initialize the progress bar and set
//    the file size
// 3) Loop reading firmware from the stream and writing it to the device, call
//    firmwareUpdateProgressCallback to update the progress bar
// 4) Call the updateFirmwareEnd function to complete the flash write operation
// 5) Display the flash write status
//
// The IM19 differs because it supports a block retry mechansim, the differences
// are:
// 1) The updateFirmwareBegin routine is called in the im19FirmwareUpdate routine
// 2) The updateFirmwareEnd routine is called in the im19FirmwareUpdate routine
// 3) After calling updateFirmwareEnd, the code determines if any blocks are
//    missing.  If so, im19FirmwareUpdate calls im19StreamMissingRanges to send
//    the missing blocks.
// 4) Upon successful completion, hard failure or to many retries, the flash
//    write status is displayed by the im19FirmwareUpdate routine
// 5) im19ArrayFlashUpdate is a stripped down version of im19FirmwareUpdate
//----------------------------------------
static bool im19StreamFirmware(const char * chip,
                               NetworkClient * stream,
                               size_t fileBytes,
                               uint8_t * buffer,
                               size_t packetBytes)
{
    bool success;

    do
    {
        success = false;

        // Display the parameters
        if (settings.debugFirmwareUpdate && otaDebugVerbose)
        {
            systemPrintf("fileBytes: %d\r\n", fileBytes);
            systemPrintf("packetBytes: %d\r\n", packetBytes);
        }

        // Initialize the progress bar
        firmwareUpdateProgressReset(fileBytes);

        // Loop until all data has been transferred or another error occurs.
        // HTTPS conections remain open even after the data has been transferred
        // and HTTP connections close after data has been transferred but some
        // may still be available.  Only test the network connection when no
        // data is available.
        unsigned long lastDataTime = millis();
        size_t validData = 0;
        while (fileBytes > 0)
        {
            // Wait until some data is available
            size_t availableBytes = stream->available();
            if (availableBytes == 0)
            {
                // Verify network connection
                if (stream->connected() == false)
                {
                    systemPrintln("ERROR: lost connection to network server");
                    break;
                }

                // Check for network timeout
                if ((millis() - lastDataTime) > OTA_DATA_TIMEOUT)
                {
                    systemPrintf("ERROR: Timed out waiting for data\r\n");
                    break;
                }
                yield();
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
            {
                systemPrintln("ERROR: Failed reading data from network");
                break;
            }
            validData += bytesRead;

            // Fill the packet
            if ((validData < packetBytes) && (validData != fileBytes))
                continue;

            // Update this portion of the firmware
            if (im19UpdateFirmware(buffer, validData) == false)
            {
                systemPrintln("ERROR: Failed during write");
                break;
            }

            // Display the progress
            firmwareUpdateProgressCallback(chip, validData);

            // Account for this data
            fileBytes -= validData;
            lastDataTime = millis();
            validData = 0;
        }
        if (fileBytes)
            break;
        success = true;
    } while (0);

    if (fileBytes && settings.debugFirmwareUpdate)
        systemPrintf("fileBytes: %d\r\n", fileBytes);
    return success;
}

//----------------------------------------
// Re-downloads the range and streams it to the IM19.
//----------------------------------------
static bool im19StreamRange(const char * chip,
                            const char * url,
                            size_t startByte,
                            size_t numBytes,
                            uint8_t * buffer,
                            size_t packetBytes)
{
    const char * cert;
    NetworkClientSecure client;
    HTTPClient http;
    const char * ipAddress;
    String ipAddressString;
    const char * server;
    String serverString;
    NetworkClient * stream;
    bool success;

    // Display the parameters
    if (settings.debugFirmwareUpdate && otaDebugVerbose)
    {
        systemPrintf("startByte: 0x%08x (%d)\r\n", startByte, startByte);
        systemPrintf("numBytes: 0x%08x (%d)\r\n", numBytes, numBytes);
        systemPrintf("packetBytes: %d\r\n", packetBytes);
    }
    do
    {
        success = false;
        if (url)
        {
            // Locate the server for this URL
            serverString = getServerFromUrl(url);
            if (serverString.length() == 0)
            {
                systemPrintf("%s firmware update failed to find server name in URL string\r\n", chip);
                break;
            }
            server = serverString.c_str();

            // Translate the server name into an IP address
            ipAddressString = getServerIpAddress(server);
            if (ipAddressString.length() == 0)
            {
                systemPrintln("Failed to get the IP address for the server");
                break;
            }
            ipAddress = ipAddressString.c_str();

            cert = getCertFromUrl(url);
            if (cert)
            {
                if (!securelyConnectToServer(url, client, cert))
                {
                    systemPrintf("Failed to securely connect to %s (%s)", server, ipAddress);
                    break;
                }

                if (!http.begin(client, url))
                {
                    systemPrintf("%s firmware update unable to begin HTTPS request.\r\n", chip);
                    break;
                }
            }
            else if (!http.begin(url))
            {
                systemPrintf("%s firmware update unable to begin HTTP request.\r\n", chip);
                break;
            }

            char rangeHeader[48];
            snprintf(rangeHeader, sizeof(rangeHeader), "bytes=%lu-%lu", startByte, startByte + numBytes - 1);
            http.addHeader("Range", rangeHeader);

            int httpCode = http.GET();
            if (httpCode != HTTP_CODE_PARTIAL_CONTENT)
            {
                // A 200 here means the server ignored our Range request and is about to send
                // the whole file from byte 0 - streaming that into this offset would corrupt
                // the image, so bail rather than guess.
                systemPrintf("HTTP range request failed, code: %d\r\n", httpCode);
                break;
            }

            // Get the data stream
            stream = http.getStreamPtr();
            success = true;
        }
        else
        {
            stream = (NetworkClient *)&dataArray;
            dataArray.init(startByte);
            success = true;
        }

        // Stream the data
        if (success)
            success = im19StreamFirmware(chip,
                                         stream,
                                         numBytes,
                                         buffer,
                                         packetBytes);
    } while (0);
    http.end();
    return success;
}

//----------------------------------------
// Walks im19FrameMap for runs of missing frames and re-requests just those byte
// ranges from the source URL, instead of re-streaming the entire firmware image.
//----------------------------------------
static bool im19StreamMissingRanges(const char * chip,
                                    const char * url,
                                    uint8_t * buffer,
                                    size_t packetBytes)
{
    bool success = true;

    if (im19TotalFrames == 0)
        return success;

    // Count the number of missing frames
    uint32_t totalMissingFrames = 0;
    for (uint32_t i = 0; i < im19TotalFrames; i++)
    {
        if ((im19FrameMap[i / 8] & (0x01 << (i % 8))) == 0)
            totalMissingFrames++;
    }

    // Count and display the missing frames
    if (totalMissingFrames && settings.debugFirmwareUpdate)
    {
        int32_t previousFrame = -1;
        for (int32_t i = 0; i < im19TotalFrames; i++)
        {
            if ((im19FrameMap[i / 8] & (0x01 << (i % 8))) == 0)
            {
                if (previousFrame < 0)
                    previousFrame = i;
            }
            else
            {
                if (previousFrame >= 0)
                {
                    if ((previousFrame + 1) == i)
                        systemPrintf("Frame #: %d\r\n", previousFrame);
                    else
                        systemPrintf("Frame # %d - %d\r\n", previousFrame, i - 1);
                }
                previousFrame = -1;
            }
        }
    }

    // Determine if any frames are misssing
    if (totalMissingFrames)
    {
        uint32_t missingRateTenthsPct = 0;
        missingRateTenthsPct = (totalMissingFrames * 10 * 100 + (im19TotalFrames / 2)) / im19TotalFrames;

        systemPrintf("%s firmware update missed %d frames (%d.%d%%)\r\n",
                     chip, totalMissingFrames,
                     missingRateTenthsPct / 10, missingRateTenthsPct % 10);

        uint32_t frame = 0;
        while (frame < im19TotalFrames)
        {
            // Walk the bitmap of received frames to find the next missed frame
            uint8_t bit = 0x01 << (frame % 8);
            if (im19FrameMap[frame / 8] & bit)
            {
                frame++;
                continue;
            }

            // Walk the bitmap of received frames to find the next received frame
            uint32_t runStart = frame;
            while (frame < im19TotalFrames && !(im19FrameMap[frame / 8] & (0x01 << (frame % 8))))
                frame++;

            size_t fileBytes = (frame - runStart) * IM19_FRAME_PAYLOAD_SIZE;
            systemPrintf("Requesting frames %lu-%lu (%lu bytes) from source\r\n",
                         runStart, (frame - 1), fileBytes);

            // Send the firmware data to the IM19
            im19NextFrameID = runStart;
            uint32_t startByte = runStart * IM19_FRAME_PAYLOAD_SIZE;
            uint32_t endByte = min(frame * IM19_FRAME_PAYLOAD_SIZE, otaFileBytes);
            success = im19StreamRange(chip,
                                      url,
                                      startByte,
                                      endByte - startByte,
                                      buffer,
                                      packetBytes);

            // Stop upon error
            if (success == false)
                break;
        }
    }
    return success;
}

//----------------------------------------
// Confirms the new firmware is running by polling for a response to AT+VERSION.
//----------------------------------------
static bool im19VerifyFirmwareRunning()
{
    delay(5000); // Give the IM19 time to flash and boot the new image
    for (int retry = 0; retry < 3; retry++)
    {
        if (im19SendATCommand("AT+VERSION\r\n", "Version:", 1))
            return true;
        delay(100);
    }
    return false;
}

//----------------------------------------
// Initialize the UART that communicates with the IM19
//----------------------------------------
void im19InitUart()
{
    // Initialize the UART communicating with the IM19
    if (SerialForTilt == nullptr)
    {
        SerialForTilt = new HardwareSerial(2);
        if (SerialForTilt == nullptr)
            reportFatalError("Failed to allocate the SerialForTilt port!");
    }
    else
        SerialForTilt->end();
    SerialForTilt->begin(115200, SERIAL_8N1, pin_IMU_RX, pin_IMU_TX);
}

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
        otaFileBytes = fileBytes;

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

        // Start the firmware update
        im19NextFrameID = 0;
        if (im19StreamFirmware(chip,
                               stream,
                               fileBytes,
                               buffer,
                               packetBytes) == false)
        {
            errorMsg = "ERROR: Failed to stream firmware to the device.";
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
                sprintf(msgBuffer, "ERROR: %s firmware update failed: no response from IM19.", chip, chip);
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

//----------------------------------------
// Sends AT+VERSION and copies the returned "Version:" line into imuFirmwareVersionStr.
// Returns true if "Version:" is seen in the response
//----------------------------------------
bool im19GetVersionString()
{
    int imuFirmwareVersionInt;
    char imuFirmwareVersionStr[32];    // Ex: IM19_H2_B2.2_A11.4.1
    bool success = false;
    IM19 * tiltSensor = nullptr;
    do
    {
        imuReset();
        delay(5000);

        // Initialize the UART communicating with the IM19
        im19InitUart();

        tiltSensor = new IM19();
        if (tiltSensor == nullptr)
        {
            systemPrintln("ERROR: IM19 firmware upload fail to allocate tiltSensor");
            break;
        }

        if (settings.debugFirmwareUpdate && otaDebugVerbose)
            tiltSensor->enableDebugging(); // Print all debug to Serial

        if (tiltSensor->begin(*SerialForTilt) == false) // Give the serial port over to the library
        {
            systemPrintln("IM19 firmware version not available");
            break;
        }

        success = true;
        success &= tiltSensor->getAppVersion(imuFirmwareVersionInt);
        char rawFirmwareVersionStr[32]; // Ex: IM19_H2_B2.2_A11.4.1
        success &= tiltSensor->getVersion(rawFirmwareVersionStr, sizeof(rawFirmwareVersionStr));

        // Pull the pure number app version out of the full version string Ex: IM19_H2_B2.2_A11.4.1 -> 11.4.1
        char *appVersionPtr = strstr(rawFirmwareVersionStr, "A");
        if (appVersionPtr != nullptr)
            snprintf(imuFirmwareVersionStr, sizeof(imuFirmwareVersionStr), "%s", appVersionPtr + 1);
        else
        {
            systemPrintln("IM19 App Version not found in full version string");
            imuFirmwareVersionStr[0] = '\0';
        }

        if (settings.debugFirmwareUpdate)
            systemPrintf("IM19 Full Version: %s\r\n", rawFirmwareVersionStr);
        else
            systemPrintf("IMU firmware: %s\r\n", imuFirmwareVersionStr);
    } while (0);
    if (tiltSensor)
        delete tiltSensor;
    return success;
}

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
        dataArray.init(0);
        otaFileBytes = fileBytes;

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

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// End of IM19 firmware update functions.
