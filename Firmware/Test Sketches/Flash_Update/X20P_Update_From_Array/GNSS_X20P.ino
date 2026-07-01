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
// Only the first RX_PAYLOAD_MAX payload bytes are stored; the rest
// are consumed from the stream but discarded.
bool x20pReceive(HardwareSerial &ser, UbxMsg &out, uint32_t deadline)
{
    int b;

    // Sliding two-byte window — finds 0xB5 0x62 even in NMEA noise
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

    // Payload (consume all, store up to RX_PAYLOAD_MAX)
    for (uint16_t i = 0; i < out.len; i++)
    {
        if ((b = x20pReadByte(ser, deadline)) < 0)
            return false;
        if (i < RX_PAYLOAD_MAX)
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
        // Non-matching message — discard and keep waiting
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
bool x20pPollMsg(HardwareSerial &ser, uint8_t cls, uint8_t id, const uint8_t *payload, uint16_t payloadLen,
                    UbxMsg &out, uint32_t timeoutMs)
{
    x20pSend(ser, cls, id, payload, payloadLen);
    return x20pWaitForMsg(ser, cls, id, out, timeoutMs);
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
 * Send one frame and wait for the device's 5-byte ACK.
 * Handles the concurrent chip-erase case
 *   - Packet 0 is sent while CERASE is in flight; the device ignores it.
 *   - When the CERASE completion arrives, the deadline is tightened to
 *     TIMEOUT_WRITE (3 s) from the original send time.
 *   - On timeout, the packet is resent (up to WRITE_RETRIES times).
 *   - For packets 1+, eraseComplete is already true and the shorter
 *     TIMEOUT_WRITE deadline is used from the start.
 *
 * eraseComplete / eraseCompleteAt are in/out: once the CERASE response is
 * seen they are set and remain true for all subsequent packet calls.
 */
bool x20pWriteChunk(HardwareSerial &ser, uint32_t address, const uint8_t *chunk, uint16_t chunkLen,
                     bool &eraseComplete, uint32_t &eraseCompleteAt)
{
    if (chunkLen == 0 || chunkLen > PACKET_SIZE)
        return false;

    x20pSendDataFrame(ser, address, chunk, chunkLen);
    ser.flush();
    uint32_t sendTime = millis();

    // If erase is already done, expect the ACK quickly; otherwise allow the full chip-erase window.
    uint32_t deadline = eraseComplete ? (millis() + TIMEOUT_WRITE) : (sendTime + TIMEOUT_CHIP_ERASE);

    for (uint8_t retries = 0; retries <= WRITE_RETRIES; retries++)
    {
        while ((int32_t)(millis() - deadline) < 0)
        {
            UbxMsg m;
            if (!x20pReceive(ser, m, deadline))
            {
                if ((int32_t)(millis() - deadline) >= 0)
                    break;
                continue; // CRC noise — keep waiting
            }

            if (m.cls == UBX_CLASS_UPD && m.id == 0x16) // Chip erase response
            {
                if (!eraseComplete && m.len >= 1 && m.payload[0] == 1)
                {
                    eraseComplete = true;
                    eraseCompleteAt = millis();
                    Serial.print("    [DBG] CERASE done, t=");
                    Serial.print(eraseCompleteAt - sendTime);
                    Serial.println(" ms after send");
                    // Tighten deadline to TIMEOUT_WRITE after first send
                    uint32_t fireAt = sendTime + TIMEOUT_WRITE;
                    deadline = ((int32_t)(millis() - fireAt) >= 0) ? millis() : fireAt;
                }
                else if (m.len >= 1 && m.payload[0] != 1)
                {
                    Serial.println("    [DBG] CERASE reported failure");
                    return false;
                }
            }
            else if (m.cls == UBX_CLASS_ACK)
            {
                // ACK-ACK or NAK for FLDET/CERASE — ignore
            }
            else if (m.cls == UBX_CLASS_UPD && m.id == 0x2A) // Write data response
            {
                if (m.len != 5)
                {
                    Serial.print("    [DBG] Data ACK wrong length: ");
                    Serial.println(m.len);
                    return false;
                }
                if (m.payload[4] != 1)
                {
                    uint32_t devAddr = (uint32_t)m.payload[0] | ((uint32_t)m.payload[1] << 8) |
                                       ((uint32_t)m.payload[2] << 16) | ((uint32_t)m.payload[3] << 24);
                    Serial.print("    [DBG] Data NACK at 0x");
                    Serial.println(devAddr, HEX);
                    return false;
                }
                return true; // success
            }
            else
            {
                Serial.print("    [DBG] rx cls=0x");
                Serial.print(m.cls, HEX);
                Serial.print(" id=0x");
                Serial.print(m.id, HEX);
                Serial.print(" len=");
                Serial.println(m.len);
            }
        }

        // Deadline expired. If CERASE never came, we have a hard failure.
        if (!eraseComplete)
        {
            Serial.println("    [DBG] CERASE did not complete within 45 s");
            return false;
        }

        // Erase done but no Data ACK — retry.
        if (retries < WRITE_RETRIES)
        {
            Serial.print("    [DBG] write timeout, retry ");
            Serial.println(retries + 1);
            x20pSendDataFrame(ser, address, chunk, chunkLen);
            ser.flush();
            sendTime = millis();
            deadline = millis() + TIMEOUT_WRITE;
        }
    }

    Serial.println("    [DBG] write retries exhausted");
    return false;
}

/*
 * x20pUpdateFirmware()
 *
 * Full firmware update sequence for the ZED-X20P (Gen 200, ROM > 4).
 * The caller must already have opened `ser` at the correct baud rate
 * and confirmed basic communication (see setup() below for example).
 *
 * Sequence:
 *   1. Start loader task     payload=[0x01]   → ACK/NAK
 *   2. Chip erase            no payload      → 1-byte status
 *   3. Write all chunks      (2048 B each) → 5-byte ACK each
 *   4. Verify                payload=[0,0,0,0] → ACK
 *   5. Reboot                no payload       (fire-and-forget)
 *
 * Parameters:
 *   ser      HardwareSerial wired to ZED-X20P UART1
 *   data     Pointer to firmware binary array (x20p_firmware[])
 *   numBytes Total byte count of the firmware image
 *
 * Returns true on success.
 */
bool x20pUpdateFirmware(HardwareSerial &ser, const uint8_t *data, uint32_t numBytes)
{
    UbxMsg msg;
    int ack;

    // ----------------------------------------------------------
    // Start loader task
    //    Payload 0x01 tells the ROM to start the flash-loader
    //    task instead of entering full safeboot.
    // ----------------------------------------------------------
    Serial.println("Starting flash loader task...");
    const uint8_t startLoaderPayload[] = {0x01};
    ack = x20pSendAndWaitAck(ser, UBX_CLASS_UPD, 0x07, startLoaderPayload, 1,
                         TIMEOUT_POLL); // Enter safeboot, start loader task
    if (ack == -1)
    {
        Serial.println("  ERROR: timed out");
        return false;
    }
    Serial.print("  Loader ");
    Serial.println(ack ? "ACK" : "NAK (continuing — normal on some ROM versions)");

    // ----------------------------------------------------------
    // Switch UART1 to UPDATE_BAUD for faster transfers.
    //      We wait for the ACK at the old baud rate; the device sends the ACK
    //      then switches. A NAK means the loader rejected the requested rate.
    // ----------------------------------------------------------
    Serial.print("Switching to ");
    Serial.print(UPDATE_BAUD);
    Serial.println(" baud...");
    {
        const uint32_t nb = UPDATE_BAUD;
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
        // Wait for ACK at old rate — device sends ACK then applies new baud.
        int cfgAck = x20pSendAndWaitAck(ser, UBX_CLASS_CFG, UBX_CFG_VALSET, cfgPayload, sizeof(cfgPayload), TIMEOUT_POLL);
        Serial.print("  [DBG] CFG-VALSET ");
        Serial.println(cfgAck == 1 ? "ACK" : cfgAck == 0 ? "NAK" : "no-ACK (timeout)");
        if (cfgAck == 0)
        {
            Serial.println("  ERROR: device NAK'd baud rate change — rate unsupported in loader");
            return false;
        }
        ser.flush();
        delay(200); // device applies new rate; ACK arrives at new baud
        // updateBaudRate() changes only the baud divisor — no GPIO re-init, no TX glitch.
        // ser.begin() briefly pulses TX low at high baud rates, causing a framing error on
        // the device (observed: bytes 2-3 of next response corrupted at 460800+).
        ser.updateBaudRate(UPDATE_BAUD);
        uint32_t drainEnd = millis() + 100; // timed drain catches FIFO stragglers
        while ((int32_t)(millis() - drainEnd) < 0)
        {
            if (ser.available())
                ser.read();
        }

        // Raw diagnostic: send MON-VER poll and print first bytes received.
        // "silence" = device didn't switch; garbage = framing error; B5 62 = working.
        x20pSend(ser, UBX_CLASS_MON, UBX_MON_VER, nullptr, 0);
        ser.flush();
        {
            uint32_t rawEnd = millis() + 500;
            uint8_t rawCount = 0;
            Serial.print("  [DBG] raw rx:");
            while ((int32_t)(millis() - rawEnd) < 0 && rawCount < 24)
            {
                if (ser.available())
                {
                    uint8_t b = ser.read();
                    Serial.print(b < 0x10 ? " 0" : " ");
                    Serial.print(b, HEX);
                    rawCount++;
                }
            }
            Serial.println(rawCount ? "" : " (silence)");
            while (ser.available())
                ser.read();
        }

        // Retry MON-VER — high baud rates may need a nudge before responding.
        bool baudOk = false;
        for (uint8_t attempt = 0; attempt < 3 && !baudOk; attempt++)
        {
            UbxMsg verCheck;
            if (x20pPollMsg(ser, UBX_CLASS_MON, UBX_MON_VER, nullptr, 0, verCheck, TIMEOUT_POLL))
                baudOk = true;
            else if (attempt < 2)
            {
                delay(200);
                while (ser.available())
                    ser.read();
            }
        }
        if (!baudOk)
        {
            Serial.println("  ERROR: baud rate switch failed — check UPDATE_BAUD");
            return false;
        }
        Serial.println("  Baud switch OK.");
    }

    // ----------------------------------------------------------
    // Chip erase — concurrent write path.
    //    This device has no flash-retention footer so FLASHRET/FLREST
    //    are not used.  CERASE is sent immediately; packet 0 will be
    //    sent concurrently and the write loop handles the retry after
    //    the erase completes.
    // ----------------------------------------------------------
    Serial.println("Starting chip erase...");
    x20pSend(ser, UBX_CLASS_UPD, 0x16, nullptr, 0);
    // Do NOT wait for chip erase ACK here — it arrives while packet 0 is in flight.

    // ----------------------------------------------------------
    // Write firmware in PACKET_SIZE (2048-byte) chunks.
    //    Packet 0 is sent concurrently with the chip erase; x20pWriteChunk
    //    watches for the CERASE completion and retries after TIMEOUT_WRITE (3 s)
    //    exactly as updWritePacket does for the flashret==0 chip-erase path.
    //    Packets 1+ have eraseComplete==true and just use TIMEOUT_WRITE each.
    // ----------------------------------------------------------
    uint32_t totalPackets = (numBytes + PACKET_SIZE - 1) / PACKET_SIZE;
    Serial.print("Writing ");
    Serial.print(totalPackets);
    Serial.print(" packets (");
    Serial.print(numBytes);
    Serial.println(" bytes)...");

    bool eraseComplete = false;
    uint32_t eraseCompleteAt = 0;

    for (uint32_t pkt = 0; pkt < totalPackets; pkt++)
    {
        uint32_t offset = pkt * (uint32_t)PACKET_SIZE;
        uint16_t chunkLen = (uint16_t)min((uint32_t)PACKET_SIZE, numBytes - offset);
        uint32_t address = (uint32_t)FW_BASE_ADDR + offset;

        if (!x20pWriteChunk(ser, address, data + offset, chunkLen, eraseComplete, eraseCompleteAt))
        {
            Serial.print("  ERROR: write failed at packet ");
            Serial.print(pkt);
            Serial.print(" address=0x");
            Serial.println(address, HEX);
            return false;
        }

        if ((pkt & 0x0F) == 0)
        { // progress every 16 packets
            Serial.print("  packet ");
            Serial.print(pkt + 1);
            Serial.print("/");
            Serial.println(totalPackets);
        }
    }
    Serial.println("  Write complete.");

    // ----------------------------------------------------------
    // Verify
    //    Verify triggers the device to re-read and
    //    validate the written image in flash, returning ACK/NAK.
    //    Version field in payload must be 0.
    // ----------------------------------------------------------
    Serial.println("Verifying image...");
    const uint8_t verPayload[4] = {0, 0, 0, 0};
    ack = x20pSendAndWaitAck(ser, UBX_CLASS_UPD, 0x2B, verPayload, 4, TIMEOUT_VERIFY); // Verify
    if (ack != 1)
    {
        Serial.print("  ERROR: verify ");
        Serial.println(ack == 0 ? "NAK" : "timeout");
        return false;
    }
    Serial.println("  Verify OK.");

    return true;
}

bool x20pFirmwareUpdateBegin()
{
    // Training sequence — helps the module's autobaud lock on
    SerialGNSS.write(0x55);
    SerialGNSS.write(0x55);
    delay(10);

    // Confirm UBX communication with a MON-VER poll
    Serial.println("Checking communication...");
    UbxMsg monVer;
    if (!x20pPollMsg(SerialGNSS, UBX_CLASS_MON, UBX_MON_VER, nullptr, 0, monVer, TIMEOUT_POLL))
    {
        Serial.println("ERROR: cannot communicate with ZED-X20P.");
        Serial.println("       Check wiring and GPS_BAUD setting.");
        return false;
    }
    Serial.println("Connected to ZED-X20P.");
    return true;
}

void x20pFirmwareUpdateEnd()
{
    // Reboot (fire-and-forget — device does not send a response)
    x20pSend(SerialGNSS, UBX_CLASS_UPD, 0x0E, nullptr, 0); // Reboot
}