// The following functions are for the X20P firmware update process.
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

// ==================================================================
//  USER CONFIGURATION
// ==================================================================

// Baud rate used only during the firmware write (reference tool default: 115200).
// #define X20P_FIRMWARE_UPDATE_BAUD 115200u  // Works
#define X20P_FIRMWARE_UPDATE_BAUD 230400u // Works
// #define X20P_FIRMWARE_UPDATE_BAUD 460800u     // Not working
// #define X20P_FIRMWARE_UPDATE_BAUD 921600u

// ==================================================================
//  UBX PROTOCOL CONSTANTS
// ==================================================================

#define UBX_SYNC1 0xB5u
#define UBX_SYNC2 0x62u

// Message classes
#define UBX_CLASS_ACK 0x05u
#define UBX_CLASS_CFG 0x06u
#define UBX_CLASS_UPD 0x09u
#define UBX_CLASS_MON 0x0Au

// CFG message IDs
#define UBX_CFG_VALSET 0x8Au

// CFG-VALSET key IDs for UART1  (from ubxmsg.h in firmwareUpdateTool v26.05)
#define KEY_UART1CFG_DATABITS 0x20520003u
#define KEY_UART1CFG_PARITY 0x20520004u
#define KEY_UART1CFG_INPROT_UBX 0x10730001u
#define KEY_UART1CFG_OUTPROT_UBX 0x10740001u
#define KEY_UART1CFG_BAUDRATE 0x40520001u

// ACK message IDs
#define UBX_ACK_NAK 0x00u
#define UBX_ACK_ACK 0x01u

// MON message IDs
#define UBX_MON_VER 0x04u

// ==================================================================
//  TIMING (ms)
//  (from updateCore.h in firmwareUpdateTool v26.05)
// ==================================================================

#define TIMEOUT_POLL 1000UL
#define TIMEOUT_CHIP_ERASE 45000UL
#define TIMEOUT_WRITE 3000UL
#define TIMEOUT_VERIFY 12000UL
#define WRITE_RETRIES 3u

// ==================================================================
//  FLASH LAYOUT
// ==================================================================
//
// Flash address 0x00 : FIS (Flash Information Sector, 72 bytes)
// Flash address 0x48 : Firmware image starts here
//
// For Gen 200, FwBase = sizeof(DRV_SPI_MEM_FIS_t) = 72.
// The device uses these device-relative byte offsets (not the MCU
// memory-mapped FLASH_BASE = 0x00800000, which is only relevant for
// older generations).

#define FIS_SIZE 72u          // sizeof(DRV_SPI_MEM_FIS_t)
#define FW_BASE_ADDR FIS_SIZE // firmware starts at offset 72 in flash

#define PACKET_SIZE 2048u

// ==================================================================
//  STREAMING UPDATE STATE
//  Bytes arrive from WiFi in arbitrarily-sized chunks (see the network
//  read buffer in x20pStreamFirmware) and are accumulated here until a
//  full PACKET_SIZE page is available to flash. Allocated in
//  x20pFirmwareUpdateBegin(), freed in x20pFirmwareUpdateEnd().
// ==================================================================

static uint8_t *x20pPageBuffer = nullptr; // Accumulates incoming bytes; flushed every PACKET_SIZE bytes
static uint16_t x20pBufferIndex = 0;
static uint32_t x20pCurrentAddress = FW_BASE_ADDR; // Next flash address to write; advances as pages are flashed

static bool x20pUpdateFailed = false; // Set once a chunk write fails; halts further processing until the next Begin()

// Write one byte to ser, updating the running Fletcher-8 checksum.
static inline void x20pWriteByte(HardwareSerial &s, uint8_t v, uint8_t &ca, uint8_t &cb)
{
    ca += v;
    cb += ca;
    s.write(v);
}

// Write a little-endian 32-bit word, updating checksum.
static inline void x20pWriteU32(HardwareSerial &s, uint32_t v, uint8_t &ca, uint8_t &cb)
{
    x20pWriteByte(s, (uint8_t)(v), ca, cb);
    x20pWriteByte(s, (uint8_t)(v >> 8), ca, cb);
    x20pWriteByte(s, (uint8_t)(v >> 16), ca, cb);
    x20pWriteByte(s, (uint8_t)(v >> 24), ca, cb);
}

// Block-read one byte with a millis()-based deadline.
// Returns the byte (0–255) or -1 on timeout.
int x20pReadByte(HardwareSerial &s, uint32_t deadline)
{
    while ((int32_t)(millis() - deadline) < 0)
    {
        if (s.available())
            return (uint8_t)s.read();
        yield();
    }
    return -1;
}

// ==================================================================
//  UBX FRAME TX  (based on UbxCreateMessage / GetUbxChecksumU1)
// ==================================================================

// Send a complete UBX frame.  The Fletcher-8 checksum covers
// [class, id, lenL, lenH, payload…] as per the UBX specification.
// payload may be nullptr when payloadLen == 0.
void x20pSend(HardwareSerial &ser, uint8_t cls, uint8_t id, const uint8_t *payload, uint16_t payloadLen)
{
    uint8_t ca = 0, cb = 0;
    ser.write(UBX_SYNC1);
    ser.write(UBX_SYNC2);
    x20pWriteByte(ser, cls, ca, cb);
    x20pWriteByte(ser, id, ca, cb);
    x20pWriteByte(ser, (uint8_t)(payloadLen), ca, cb);
    x20pWriteByte(ser, (uint8_t)(payloadLen >> 8), ca, cb);
    for (uint16_t i = 0; i < payloadLen; i++)
        x20pWriteByte(ser, payload[i], ca, cb);
    ser.write(ca);
    ser.write(cb);
}

// ==================================================================
//  UBX FRAME RX  (based on UbxSearchMsg / UbxCheckCrc)
// ==================================================================

// Receive and validate one UBX frame, blocking until deadline.
// Returns true on success; populates `out`.
// Only the first X20P_RX_PAYLOAD_MAX payload bytes are stored; the rest
// are consumed from the stream but discarded.
bool x20pReceive(HardwareSerial &ser, UbxMsg &out, uint32_t deadline)
{
    int b;

    // Sliding two-byte window - finds 0xB5 0x62 even in NMEA noise
    int prev = -1;
    while (true)
    {
        if ((b = x20pReadByte(ser, deadline)) < 0)
            return false;
        if (prev == UBX_SYNC1 && b == UBX_SYNC2)
            break;
        prev = b;
    }

    // Header: class(1) id(1) lenL(1) lenH(1)
    uint8_t ca = 0, cb = 0;
    uint8_t hdr[4];
    for (int i = 0; i < 4; i++)
    {
        if ((b = x20pReadByte(ser, deadline)) < 0)
            return false;
        hdr[i] = (uint8_t)b;
        ca += hdr[i];
        cb += ca;
    }
    out.cls = hdr[0];
    out.id = hdr[1];
    out.len = (uint16_t)hdr[2] | ((uint16_t)hdr[3] << 8);

    // Payload (consume all, store up to X20P_RX_PAYLOAD_MAX)
    for (uint16_t i = 0; i < out.len; i++)
    {
        if ((b = x20pReadByte(ser, deadline)) < 0)
            return false;
        if (i < X20P_RX_PAYLOAD_MAX)
            out.payload[i] = (uint8_t)b;
        ca += (uint8_t)b;
        cb += ca;
    }

    // CRC bytes
    int cka, ckb;
    if ((cka = x20pReadByte(ser, deadline)) < 0)
        return false;
    if ((ckb = x20pReadByte(ser, deadline)) < 0)
        return false;

    return (ca == (uint8_t)cka) && (cb == (uint8_t)ckb);
}

// Wait for a UBX message matching class/id (pass -1 to match any).
// Discards non-matching messages received before the timeout.
bool x20pWaitForMsg(HardwareSerial &ser, int wantCls, int wantId, UbxMsg &out, uint32_t timeoutMs)
{
    uint32_t deadline = millis() + timeoutMs;
    while ((int32_t)(millis() - deadline) < 0)
    {
        UbxMsg m;
        if (!x20pReceive(ser, m, deadline))
        {
            // x20pReceive returns false on either a true timeout or a CRC mismatch.
            // Only break on true timeout; on CRC mismatch retry within the window.
            if ((int32_t)(millis() - deadline) >= 0)
                break;
            continue;
        }
        if ((wantCls < 0 || m.cls == (uint8_t)wantCls) && (wantId < 0 || m.id == (uint8_t)wantId))
        {
            out = m;
            return true;
        }
        // Non-matching message - discard and keep waiting
    }
    return false;
}

// Send a message and wait for the standard UBX-ACK-ACK / UBX-ACK-NAK.
// Returns  1 = ACK,  0 = NAK,  -1 = timeout.
int x20pSendAndWaitAck(HardwareSerial &ser, uint8_t cls, uint8_t id, const uint8_t *payload, uint16_t payloadLen,
                       uint32_t timeoutMs)
{
    x20pSend(ser, cls, id, payload, payloadLen);
    uint32_t deadline = millis() + timeoutMs;
    while ((int32_t)(millis() - deadline) < 0)
    {
        UbxMsg m;
        if (!x20pReceive(ser, m, deadline))
            break;
        if (m.cls == UBX_CLASS_ACK && m.len == 2 && m.payload[0] == cls && m.payload[1] == id)
        {
            return (m.id == UBX_ACK_ACK) ? 1 : 0;
        }
    }
    return -1;
}

// Send a poll request and wait for the response with the same class/id.
bool x20pPollMsg(HardwareSerial &ser, uint8_t cls, uint8_t id, const uint8_t *payload, uint16_t payloadLen, UbxMsg &out,
                 uint32_t timeoutMs)
{
    x20pSend(ser, cls, id, payload, payloadLen);
    return x20pWaitForMsg(ser, cls, id, out, timeoutMs);
}

// Poll UBX-MON-VER and print the module's software/hardware version strings.
// Assumes ser is already communicating with the module at the correct baud rate
// (works both in normal application firmware and in the bootloader).
// Returns true if a version response was received and parsed.
bool x20pPrintVersion(HardwareSerial &ser)
{
    UbxMsg monVer;
    if (x20pPollMsg(ser, UBX_CLASS_MON, UBX_MON_VER, nullptr, 0, monVer, TIMEOUT_POLL) == false)
    {
        systemPrintln("Firmware version: no response from module.");
        return false;
    }

    if (monVer.len < 40)
    {
        systemPrintln("Firmware version: response too short to parse.");
        return false;
    }

    char swVersion[31];
    char hwVersion[11];
    memcpy(swVersion, monVer.payload, 30);
    swVersion[30] = '\0';
    memcpy(hwVersion, monVer.payload + 30, 10);
    hwVersion[10] = '\0';

    systemPrint("Firmware version: ");
    systemPrint(swVersion);
    systemPrint("  Hardware version: ");
    systemPrintln(hwVersion);

    return true;
}

// ==================================================================
//  PUBLIC API
// ==================================================================

// Transmit one frame (does NOT flush).
void x20pSendDataFrame(HardwareSerial &ser, uint32_t address, const uint8_t *chunk, uint16_t chunkLen)
{
    uint16_t payloadLen = 4u + 4u + 4u + chunkLen;
    uint8_t ca = 0, cb = 0;
    ser.write(UBX_SYNC1);
    ser.write(UBX_SYNC2);
    x20pWriteByte(ser, UBX_CLASS_UPD, ca, cb);
    x20pWriteByte(ser, 0x2A, ca, cb); // Write data chunk
    x20pWriteByte(ser, (uint8_t)(payloadLen), ca, cb);
    x20pWriteByte(ser, (uint8_t)(payloadLen >> 8), ca, cb);
    x20pWriteU32(ser, (uint32_t)0, ca, cb); // Data version
    x20pWriteU32(ser, address, ca, cb);
    x20pWriteU32(ser, (uint32_t)chunkLen, ca, cb);
    for (uint16_t i = 0; i < chunkLen; i++)
        x20pWriteByte(ser, chunk[i], ca, cb);
    ser.write(ca);
    ser.write(cb);
}

/*
 * x20pWriteChunk()
 *
 * Send one frame and wait for the device's 5-byte ACK, retrying up to
 * WRITE_RETRIES times on timeout. Assumes the chip erase has already
 * completed (see x20pChipErase()) before the first call, so every
 * write gets the short TIMEOUT_WRITE deadline - no concurrent-erase
 * bookkeeping is needed here.
 */
bool x20pWriteChunk(HardwareSerial &ser, uint32_t address, const uint8_t *chunk, uint16_t chunkLen)
{
    if (chunkLen == 0 || chunkLen > PACKET_SIZE)
        return false;

    for (uint8_t retries = 0; retries <= WRITE_RETRIES; retries++)
    {
        x20pSendDataFrame(ser, address, chunk, chunkLen);
        ser.flush();
        uint32_t deadline = millis() + TIMEOUT_WRITE;

        while ((int32_t)(millis() - deadline) < 0)
        {
            UbxMsg m;
            if (!x20pReceive(ser, m, deadline))
            {
                if ((int32_t)(millis() - deadline) >= 0)
                    break;
                continue; // CRC noise - keep waiting
            }

            if (m.cls == UBX_CLASS_UPD && m.id == 0x2A) // Write data response
            {
                if (m.len != 5)
                {
                    systemPrint("    [DBG] Data ACK wrong length: ");
                    systemPrintln(m.len);
                    return false;
                }
                if (m.payload[4] != 1)
                {
                    uint32_t devAddr = (uint32_t)m.payload[0] | ((uint32_t)m.payload[1] << 8) |
                                       ((uint32_t)m.payload[2] << 16) | ((uint32_t)m.payload[3] << 24);
                    systemPrintf("    [DBG] Data NACK at 0x%08X\r\n", devAddr);
                    return false;
                }
                return true; // success
            }
            // Ignore ACK-ACK/NAK and any other frames while waiting for the data response
        }

        if (retries < WRITE_RETRIES)
        {
            systemPrint("    [DBG] write timeout, retry ");
            systemPrintln(retries + 1);
        }
    }

    systemPrintln("    [DBG] write retries exhausted");
    return false;
}

/*
 * x20pUpdateFirmware()
 *
 * Feeds a chunk of firmware bytes (of any length, e.g. one WiFi read)
 * into the page-accumulation buffer, flushing a full PACKET_SIZE
 * (2048-byte) page to flash every time the buffer fills. Call this
 * repeatedly with successive chunks between x20pFirmwareUpdateBegin()
 * and x20pFirmwareUpdateEnd() - it does not know the total image size
 * up front, and does not erase, verify, or reboot; those live in
 * Begin()/End() since they only happen once per update.
 *
 * Parameters:
 *   ser      HardwareSerial wired to ZED-X20P UART1
 *   data     Pointer to this chunk's bytes
 *   numBytes Number of bytes in this chunk
 *
 * Returns true on success (or a no-op success if a prior chunk already failed).
 */
bool x20pUpdateFirmware(HardwareSerial &ser, const uint8_t *data, uint32_t numBytes)
{
    if (x20pUpdateFailed)
        return false; // A prior chunk write failed - stop touching the page buffer/flash

    for (uint32_t i = 0; i < numBytes; i++)
    {
        x20pPageBuffer[x20pBufferIndex++] = data[i];

        if (x20pBufferIndex == PACKET_SIZE)
        {
            if (!x20pWriteChunk(ser, x20pCurrentAddress, x20pPageBuffer, PACKET_SIZE))
            {
                systemPrintf("  ERROR: write failed at address 0x%08X\r\n", x20pCurrentAddress);
                x20pUpdateFailed = true;
                return false;
            }
            x20pCurrentAddress += PACKET_SIZE;
            x20pBufferIndex = 0;
        }
    }

    return true;
}

/*
 * x20pEnterBootloaderMode()
 *
 * Hardware-resets the GNSS, autobauds to find its current comms rate,
 * tells the ROM to start the flash-loader (LDR) task, then switches
 * UART1 to X20P_FIRMWARE_UPDATE_BAUD for faster transfers.
 *
 * Called twice per update: once to reach a loader task for the chip
 * erase, and again afterward - with flash now blank - to force a
 * genuine ROM LDR bootloader boot, which is the only state that
 * accepts write-data commands (see x20pFirmwareUpdateBegin()).
 *
 * Returns true on success.
 */
bool x20pEnterBootloaderMode()
{
    systemPrintln("Resetting GNSS");
    gpioExpanderGnssReset();
    delay(25);
    gpioExpanderGnssBoot();
    delay(250);

    bool foundBaud = false;

    // Candidates sorted in generally most common order.
    const uint32_t baudCandidates[] = {115200, 38400, 9600, 230400, 57600, 460800, 921600};

    for (uint8_t i = 0; i < (sizeof(baudCandidates) / sizeof(baudCandidates[0])); i++)
    {
        systemPrintf("Checking communication at %d...\r\n", baudCandidates[i]);

        serialGNSS->updateBaudRate(baudCandidates[i]);

        delay(10);
        while (serialGNSS->available())
            serialGNSS->read();

        // Training sequence - helps the module's autobaud lock on
        serialGNSS->write(0x55);
        serialGNSS->write(0x55);
        delay(10);

        // Confirm UBX communication with a MON-VER poll
        UbxMsg monVer;
        if (x20pPollMsg(*serialGNSS, UBX_CLASS_MON, UBX_MON_VER, nullptr, 0, monVer, TIMEOUT_POLL) == true)
        {
            systemPrintf("  OK at %d baud.\r\n", baudCandidates[i]);
            foundBaud = true;
            break;
        }
        systemPrintf("  No response at %d baud.\r\n", baudCandidates[i]);
    }

    if (!foundBaud)
        return false;

    // ----------------------------------------------------------
    // Start loader task
    //    Payload 0x01 tells the ROM to start the flash-loader
    //    task instead of entering full safeboot.
    // ----------------------------------------------------------
    systemPrintln("Starting flash loader task...");
    const uint8_t startLoaderPayload[] = {0x01};
    int ack = x20pSendAndWaitAck(*serialGNSS, UBX_CLASS_UPD, 0x07, startLoaderPayload, 1,
                                 TIMEOUT_POLL); // Enter safeboot, start loader task
    if (ack == -1)
    {
        systemPrintln("  ERROR: timed out");
        return false;
    }
    systemPrint("  Loader ");
    systemPrintln(ack ? "ACK" : "NAK (continuing - normal on some ROM versions)");

    // ----------------------------------------------------------
    // Switch UART1 to X20P_FIRMWARE_UPDATE_BAUD for faster transfers.
    //      We wait for the ACK at the old baud rate; the device sends the ACK
    //      then switches. A NAK means the loader rejected the requested rate.
    // ----------------------------------------------------------
    systemPrint("Switching to ");
    systemPrint(X20P_FIRMWARE_UPDATE_BAUD);
    systemPrintln(" baud...");
    {
        const uint32_t nb = X20P_FIRMWARE_UPDATE_BAUD;
        const uint8_t cfgPayload[32] = {0x00,
                                        0x01,
                                        0x00,
                                        0x00,
                                        0x03,
                                        0x00,
                                        0x52,
                                        0x20,
                                        0x00,
                                        0x04,
                                        0x00,
                                        0x52,
                                        0x20,
                                        0x00,
                                        0x01,
                                        0x00,
                                        0x73,
                                        0x10,
                                        0x01,
                                        0x01,
                                        0x00,
                                        0x74,
                                        0x10,
                                        0x01,
                                        0x01,
                                        0x00,
                                        0x52,
                                        0x40,
                                        (uint8_t)(nb),
                                        (uint8_t)(nb >> 8),
                                        (uint8_t)(nb >> 16),
                                        (uint8_t)(nb >> 24)};
        // Wait for ACK at old rate - device sends ACK then applies new baud.
        int cfgAck = x20pSendAndWaitAck(*serialGNSS, UBX_CLASS_CFG, UBX_CFG_VALSET, cfgPayload, sizeof(cfgPayload),
                                        TIMEOUT_POLL);
        systemPrint("  [DBG] CFG-VALSET ");
        systemPrintln(cfgAck == 1 ? "ACK" : cfgAck == 0 ? "NAK" : "no-ACK (timeout)");
        if (cfgAck == 0)
        {
            systemPrintln("  ERROR: device NAK'd baud rate change - rate unsupported in loader");
            return false;
        }
        serialGNSS->flush();
        delay(200); // device applies new rate; ACK arrives at new baud
        // updateBaudRate() changes only the baud divisor - no GPIO re-init, no TX glitch.
        // ser.begin() briefly pulses TX low at high baud rates, causing a framing error on
        // the device (observed: bytes 2-3 of next response corrupted at 460800+).
        serialGNSS->updateBaudRate(X20P_FIRMWARE_UPDATE_BAUD);
        uint32_t drainEnd = millis() + 100; // timed drain catches FIFO stragglers
        while ((int32_t)(millis() - drainEnd) < 0)
        {
            if (serialGNSS->available())
                serialGNSS->read();
        }

        // Raw diagnostic: send MON-VER poll and print first bytes received.
        // "silence" = device didn't switch; garbage = framing error; B5 62 = working.
        x20pSend(*serialGNSS, UBX_CLASS_MON, UBX_MON_VER, nullptr, 0);
        serialGNSS->flush();
        {
            uint32_t rawEnd = millis() + 500;
            uint8_t rawCount = 0;
            systemPrint("  [DBG] raw rx:");
            while ((int32_t)(millis() - rawEnd) < 0 && rawCount < 24)
            {
                if (serialGNSS->available())
                {
                    uint8_t b = serialGNSS->read();
                    systemPrint(b < 0x10 ? " 0" : " ");
                    systemPrint(b, HEX);
                    rawCount++;
                }
            }
            systemPrintln(rawCount ? "" : " (silence)");
            while (serialGNSS->available())
                serialGNSS->read();
        }

        // Retry MON-VER - high baud rates may need a nudge before responding.
        bool baudOk = false;
        for (uint8_t attempt = 0; attempt < 3 && !baudOk; attempt++)
        {
            UbxMsg verCheck;
            if (x20pPollMsg(*serialGNSS, UBX_CLASS_MON, UBX_MON_VER, nullptr, 0, verCheck, TIMEOUT_POLL))
                baudOk = true;
            else if (attempt < 2)
            {
                delay(200);
                while (serialGNSS->available())
                    serialGNSS->read();
            }
        }
        if (!baudOk)
        {
            systemPrintln("  ERROR: baud rate switch failed - check X20P_FIRMWARE_UPDATE_BAUD");
            return false;
        }
        systemPrintln("  Baud switch OK.");
    }

    return true;
}

/*
 * x20pChipErase()
 *
 * Sends UBX-UPD CERASE (full chip erase) and blocks until the device
 * confirms completion (up to TIMEOUT_CHIP_ERASE), unlike the old
 * concurrent-erase path this replaces. Must be called while the
 * device is in the ROM loader task (see x20pEnterBootloaderMode()).
 * This device has no flash-retention footer, so FLASHRET/FLREST are
 * not used.
 *
 * Returns true if the erase completed successfully.
 */
bool x20pChipErase()
{
    systemPrintln("Starting chip erase...");
    x20pSend(*serialGNSS, UBX_CLASS_UPD, 0x16, nullptr, 0);

    uint32_t deadline = millis() + TIMEOUT_CHIP_ERASE;
    while ((int32_t)(millis() - deadline) < 0)
    {
        UbxMsg m;
        if (!x20pReceive(*serialGNSS, m, deadline))
        {
            if ((int32_t)(millis() - deadline) >= 0)
                break;
            continue; // CRC noise - keep waiting
        }

        if (m.cls == UBX_CLASS_UPD && m.id == 0x16) // Chip erase response
        {
            if (m.len >= 1 && m.payload[0] == 1)
            {
                systemPrintln("  Chip erase complete.");
                return true;
            }
            systemPrintln("  ERROR: chip erase reported failure");
            return false;
        }
        // Ignore stray ACKs/other frames while waiting for the CERASE response
    }

    systemPrintln("  ERROR: chip erase did not complete within 45 s");
    return false;
}

/*
 * x20pFirmwareUpdateBegin()
 *
 * Owns the full pre-write sequence:
 *   1. Enter the bootloader (reset + autobaud + start loader task + baud switch).
 *      When the module was still running full application firmware, this
 *      lands in an app-hosted loader shim: it answers MON-VER and CERASE,
 *      but does not accept the write-data command at all (confirmed on the
 *      bench - writes silently get zero response from that state, every
 *      time, while they succeed immediately from a genuine ROM LDR boot).
 *   2. Perform a full chip erase and wait for it to complete. Flash is now
 *      blank, so the ROM has nothing else to boot into.
 *   3. Reset the module again. With flash blank it can only land in the
 *      real ROM LDR bootloader, which does accept writes.
 *   4. Allocate the page-accumulation buffer used by x20pUpdateFirmware()
 *      so bootloading of the new code can begin.
 *
 * Returns true on success.
 */
bool x20pFirmwareUpdateBegin()
{
    if (x20pEnterBootloaderMode() == false)
        return false;

    if (x20pChipErase() == false)
        return false;

    systemPrintln("Resetting module to guarantee true ROM LDR bootloader...");
    if (x20pEnterBootloaderMode() == false)
        return false;

    // Allocate the page-accumulation buffer and reset streaming state for this update.
    if (x20pPageBuffer == nullptr)
        x20pPageBuffer = (uint8_t *)malloc(PACKET_SIZE);
    x20pBufferIndex = 0;
    x20pCurrentAddress = FW_BASE_ADDR;
    x20pUpdateFailed = false;

    return true;
}

/*
 * x20pFirmwareUpdateEnd()
 *
 * Flushes any partial trailing page left in the accumulation buffer,
 * verifies the written image, frees the buffer, and reboots the
 * device (fire-and-forget) into the new firmware.
 *
 * uploadSucceeded should be the return value of x20pStreamFirmware().
 * If the WiFi upload itself failed partway (TLS/HTTP error, dropped
 * connection, etc.) the flash image is known incomplete, so the final
 * flush and verify are skipped - verifying a partial image against the
 * device is pointless and just produces a misleading NAK.
 *
 * Returns true only if the upload, flush, and verify all succeeded.
 */
bool x20pFirmwareUpdateEnd(bool uploadSucceeded)
{
    bool success = uploadSucceeded && !x20pUpdateFailed;

    if (success && x20pBufferIndex > 0)
    {
        success = x20pWriteChunk(*serialGNSS, x20pCurrentAddress, x20pPageBuffer, x20pBufferIndex);
        if (!success)
            systemPrintf("  ERROR: final chunk write failed at address 0x%08X\r\n", x20pCurrentAddress);
    }

    free(x20pPageBuffer);
    x20pPageBuffer = nullptr;

    if (success)
    {
        // ----------------------------------------------------------
        // Verify
        //    Verify triggers the device to re-read and
        //    validate the written image in flash, returning ACK/NAK.
        //    Version field in payload must be 0.
        // ----------------------------------------------------------
        systemPrintln("Verifying image...");
        const uint8_t verPayload[4] = {0, 0, 0, 0};
        int ack = x20pSendAndWaitAck(*serialGNSS, UBX_CLASS_UPD, 0x2B, verPayload, 4, TIMEOUT_VERIFY); // Verify
        if (ack != 1)
        {
            systemPrint("  ERROR: verify ");
            systemPrintln(ack == 0 ? "NAK" : "timeout");
            success = false;
        }
        else
            systemPrintln("  Verify OK.");
    }
    else
        systemPrintln("Skipping verify - firmware upload did not complete successfully.");

    // Reboot (fire-and-forget - device does not send a response)
    x20pSend(*serialGNSS, UBX_CLASS_UPD, 0x0E, nullptr, 0); // Reboot

    return success;
}

// Update the X20P firmware
// Owns the full update sequence: enters bootloader mode, streams the image
// over WiFi, then verifies/reboots - callers only need to call this one
// function and do not need to know about Begin()/End().
bool x20pStreamFirmware(char *relativeFirmwareFileLocation)
{
    if (relativeFirmwareFileLocation == nullptr)
    {
        systemPrintln("Firmware file location is null.");
        return false;
    }

    systemPrintln("Starting X20P firmware update...");

    firmwareUpdateProgressReset();

    // Enter the bootloader and erase flash before opening the GitHub connection.
    // This sequence involves two hardware resets and autobaud probing and can take
    // 30+ seconds; opening the HTTPS GET first and leaving it idle that long risked
    // the connection going stale (and a stalled TLS read blocking forever) before a
    // single body byte was ever consumed.
    if (x20pFirmwareUpdateBegin() == false)
    {
        systemPrintln("Failed to enter bootloader mode.");
        return false;
    }

    systemPrintln("Device is in bootloader mode.");

    WiFiClientSecure client;
    if (!otaSecurelyConnectGitHub(client))
    {
        systemPrintln("Failed to securely connect to GitHub.");
        x20pFirmwareUpdateEnd(false);
        return false;
    }

    HTTPClient http;
    if (!http.begin(client, otaGetGithubFileLocation(relativeFirmwareFileLocation)))
    {
        systemPrintln("Unable to begin HTTP request.");
        x20pFirmwareUpdateEnd(false);
        return false;
    }

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK)
    {
        systemPrintf("HTTP GET failed, code: %d\r\n", httpCode);
        http.end();
        x20pFirmwareUpdateEnd(false);
        return false;
    }

    int contentLength = http.getSize();
    if (contentLength > 0)
        firmwareUpdateBytesToProcess = (uint32_t)contentLength;

    WiFiClient *stream = http.getStreamPtr();
    uint8_t buffer[256];

    bool success = true;

    while (http.connected() && (contentLength > 0 || contentLength == -1))
    {
        size_t available = stream->available();
        if (available == 0)
        {
            if (!client.connected())
                break;
            delay(1);
            continue;
        }

        size_t toRead = min(available, sizeof(buffer));
        int bytesRead = stream->readBytes(buffer, toRead);
        if (bytesRead <= 0)
            break;

        if (x20pUpdateFirmware(*serialGNSS, buffer, (uint32_t)bytesRead) == false)
        {
            systemPrintln("Firmware update failed during WiFi data upload.");
            success = false;
            break;
        }

        firmwareUpdateProgressCallback(bytesRead);

        if (contentLength > 0)
            contentLength -= bytesRead;
    }

    http.end();

    if (success)
        systemPrintln("X20P update successfully completed.");
    else
        systemPrintln("X20P firmware update failed.");

    // x20pFirmwareUpdateBegin() succeeded above, so End() must always run -
    // it verifies (when success), frees the page buffer, and reboots the device.
    systemPrintln("Rebooting receiver...");
    bool updateOk = x20pFirmwareUpdateEnd(success);

    return updateOk;
}
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// End of X20P firmware update functions.