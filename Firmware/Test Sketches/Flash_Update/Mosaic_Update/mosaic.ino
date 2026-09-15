// The following functions are for the mosaic-X5 firmware update process.
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
//
// This file drives the module's ASCII command-line interface
// (the "SSSSSSSSSSSSSSSSSSSS\n\r" escape sequence that yields a "COM1>"
// prompt is confirmed against production GNSS_Mosaic.ino) and then puts the
// receiver into upgrade mode and streams the .suf file to it directly.
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

// ==================================================================
//  USER CONFIGURATION
// ==================================================================

// Command sent at the "COM1>" prompt to put the mosaic-X5 into upgrade mode.
#define MOSAIC_FW_UPDATE_TRIGGER_CMD "exeResetReceiver, Upgrade, none\n\r"
#define MOSAIC_SUF_READY_TEXT "Ready for SUF download"

// Increase bootload speed as much as possible because the SUF files are ~22MB
static const uint32_t mosaicUpgradeBaudCandidates[] = {4000000, 3000000, 921600};

// Normal/idle operating baud to leave COM1 at once an update finishes
#define MOSAIC_NORMAL_BAUD 460800u

// ==================================================================
//  COMMAND-LINE INTERFACE CONSTANTS
// ==================================================================

// Confirmed in production GNSS_Mosaic.ino: sending this string interrupts
// NMEA/SBF streaming and yields a "COM1>" prompt.
#define MOSAIC_ESCAPE_SEQUENCE "SSSSSSSSSSSSSSSSSSSS\n\r"
#define MOSAIC_PROMPT "COM1>"

// ==================================================================
//  TIMING (ms)
// ==================================================================

#define TIMEOUT_POLL 1000UL              // Command-prompt / short responses
#define TIMEOUT_BOOTLOADER_ENTRY 45000UL // Worst case for the receiver to report ready for the SUF download
#define TIMEOUT_IDENTIFICATION 5000UL    // "lif,Identification" reply is a multi-block XML dump, not a single line

// Baud rates the mosaic-X5 command line is known/likely to be running at.
static const uint32_t mosaicBaudCandidates[] = {460800, 921600, 115200, 230400, 9600};

// Baud rate found by the most recent successful mosaicFindCommandPrompt()
// call. Tried first on subsequent calls so a normal call doesn't have to
// re-scan mosaicBaudCandidates every time.
static uint32_t mosaicKnownBaud = 0;

// ==================================================================
//  COMMAND-LINE HELPERS
// ==================================================================

/*
 * mosaicWaitForPrompt()
 *
 * Reads bytes from ser, sliding them through a window the length of
 * `prompt`, until that exact substring is seen or the deadline expires.
 * If `response` is non-null, every byte read is also copied there (up to
 * responseSize - 1) so callers can inspect/print what the module said.
 */
static bool mosaicWaitForPrompt(HardwareSerial &ser, const char *prompt, uint32_t timeoutMs, char *response = nullptr,
                                 size_t responseSize = 0)
{
    size_t promptLen = strlen(prompt);
    char window[32];
    if (promptLen == 0 || promptLen >= sizeof(window))
        return false;

    size_t windowLen = 0;
    size_t respLen = 0;
    uint32_t deadline = millis() + timeoutMs;

    while ((int32_t)(millis() - deadline) < 0)
    {
        if (!ser.available())
        {
            yield();
            continue;
        }

        char c = (char)ser.read();

        if (response != nullptr && respLen < responseSize - 1)
            response[respLen++] = c;

        if (windowLen < promptLen)
            window[windowLen++] = c;
        else
        {
            memmove(window, window + 1, promptLen - 1);
            window[promptLen - 1] = c;
        }

        if (windowLen == promptLen && strncmp(window, prompt, promptLen) == 0)
        {
            if (response != nullptr)
                response[respLen] = '\0';
            return true;
        }
    }

    if (response != nullptr)
        response[respLen] = '\0';
    return false;
}

// Switch to baud, then retry the escape+prompt a few times. The module
// streams NMEA/SBF continuously once booted (confirmed via raw-byte capture
// elsewhere in this codebase - it is never silent), so a single attempt can
// land mid-sentence and miss the prompt within its own timeout.
static bool mosaicTryBaud(HardwareSerial &ser, uint32_t baud, char *response, size_t responseSize)
{
    ser.updateBaudRate(baud);
    delay(10);

    for (uint8_t attempt = 0; attempt < 3; attempt++)
    {
        while (ser.available())
            ser.read();

        ser.print(MOSAIC_ESCAPE_SEQUENCE);

        if (mosaicWaitForPrompt(ser, MOSAIC_PROMPT, TIMEOUT_POLL, response, responseSize))
            return true;
    }
    return false;
}

/*
 * mosaicFindCommandPrompt()
 *
 * Finds the module's current command-line baud rate and leaves it sitting at
 * the "COM1>" prompt. Tries the last-known-good baud (mosaicKnownBaud) first;
 * only falls back to scanning the full mosaicBaudCandidates list if that
 * fails (e.g. on the very first call, or if the module's baud changed).
 * Leaves ser's baud rate set to whichever candidate worked, and updates
 * mosaicKnownBaud so future calls skip straight to it. If
 * `response`/`responseSize` are given, the bytes read while waiting on the
 * winning attempt are copied there.
 *
 * Returns true if the prompt was reached.
 */
bool mosaicFindCommandPrompt(HardwareSerial &ser, char *response = nullptr, size_t responseSize = 0)
{
    if (mosaicKnownBaud != 0)
    {
        if (mosaicTryBaud(ser, mosaicKnownBaud, response, responseSize))
            return true;
        systemPrintf("No response at previously-known %d baud, rescanning...\r\n", mosaicKnownBaud);
    }

    for (uint8_t i = 0; i < (sizeof(mosaicBaudCandidates) / sizeof(mosaicBaudCandidates[0])); i++)
    {
        systemPrintf("Checking communication at %d...\r\n", mosaicBaudCandidates[i]);

        if (mosaicTryBaud(ser, mosaicBaudCandidates[i], response, responseSize))
        {
            systemPrintf("  OK at %d baud.\r\n", mosaicBaudCandidates[i]);
            mosaicKnownBaud = mosaicBaudCandidates[i];
            return true;
        }
        systemPrintf("  No response at %d baud.\r\n", mosaicBaudCandidates[i]);
    }
    return false;
}

// Version fields populated by mosaicGetVersion().
int mosaicVersionMajor = 0;
int mosaicVersionMinor = 0;
int mosaicVersionPatch = 0;
int mosaicVersionRevision = 0;

/*
 * mosaicGetVersion()
 *
 * Finds the command prompt (see mosaicFindCommandPrompt()), sends
 * "lif,Identification", and parses the <firmware version="X.Y.Z[.W]">
 * attribute out of the XML reply into
 * mosaicVersionMajor/Minor/Patch/Revision.
 *
 * The reply is a multi-"BLOCK n / N" XML dump, e.g.:
 *   COM1>$R;  lif,Identification
 *   ---->
 *   $-- BLOCK 1 / 6
 *   <?xml version="1.0" encoding="ISO-8859-1" ?>
 *   ...
 *   ---->
 *   $-- BLOCK 3 / 6
 *       <firmware version="4.15.0" date="250716" rev="g5e108b">
 *   ...
 *   COM1>
 *
 * Returns true if a version string was found and at least major.minor.patch parsed.
 */
bool mosaicGetVersion(HardwareSerial &ser)
{
    if (mosaicFindCommandPrompt(ser) == false)
    {
        systemPrintln("mosaicGetVersion: no response from module.");
        return false;
    }

    while (ser.available())
        ser.read();
    ser.print("lif,Identification\n\r");

    static char response[1024 * 4]; // XML is ~3k
    if (mosaicWaitForPrompt(ser, MOSAIC_PROMPT, TIMEOUT_IDENTIFICATION, response, sizeof(response)) == false)
        systemPrintln("mosaicGetVersion: warning - prompt not seen before timeout, parsing what was received.");

    const char *tag = strstr(response, "<firmware version=\"");
    if (tag == nullptr)
    {
        systemPrintln("mosaicGetVersion: firmware version tag not found in response.");
        return false;
    }
    tag += strlen("<firmware version=\"");

    char versionStr[32];
    size_t i = 0;
    while (tag[i] != '"' && tag[i] != '\0' && i < sizeof(versionStr) - 1)
    {
        versionStr[i] = tag[i];
        i++;
    }
    versionStr[i] = '\0';

    mosaicVersionMajor = 0;
    mosaicVersionMinor = 0;
    mosaicVersionPatch = 0;
    mosaicVersionRevision = 0;
    int fieldsParsed = sscanf(versionStr, "%d.%d.%d.%d", &mosaicVersionMajor, &mosaicVersionMinor,
                               &mosaicVersionPatch, &mosaicVersionRevision);
    if (fieldsParsed < 3)
    {
        systemPrintf("mosaicGetVersion: unable to parse version string '%s'\r\n", versionStr);
        return false;
    }

    systemPrintf("mosaic-X5 firmware version: %d.%d.%d.%d\r\n", mosaicVersionMajor, mosaicVersionMinor,
                 mosaicVersionPatch, mosaicVersionRevision);
    return true;
}

// ==================================================================
//  PUBLIC API
// ==================================================================

/*
 * mosaicUpdateFirmware()
 *
 * Writes a chunk of firmware bytes (of any length, e.g. one WiFi read)
 * straight to the module - the .suf transfer is a plain byte stream, no
 * XMODEM/YMODEM framing. Call this repeatedly with successive chunks between
 * mosaicFirmwareUpdateBegin() and mosaicFirmwareUpdateEnd().
 *
 * Parameters:
 *   ser      HardwareSerial wired to mosaic-X5 COM1
 *   data     Pointer to this chunk's bytes
 *   numBytes Number of bytes in this chunk
 *
 * Returns true if all bytes were written.
 */
bool mosaicUpdateFirmware(HardwareSerial &ser, const uint8_t *data, uint32_t numBytes)
{
    return ser.write(data, numBytes) == numBytes;
}

/*
 * mosaicTrySetBaud()
 *
 * Asks the module to switch COM1 to candidate via "scs" (Set COM Settings)
 * and confirms we can still talk to it there before accepting it. Command
 * syntax ("scs,COM1,baudNNNNNN,bits8,No,bit1,none") and the "COMSettings"
 * reply are the same pattern production GNSS_Mosaic.ino uses for other COM
 * ports; only 921600 is confirmed there, so candidates above that are
 * opportunistic.
 *
 * Always verifies directly at candidate regardless of whether the
 * "COMSettings" reply text was seen - the module has been observed to
 * actually switch even when we don't catch that reply in time (its internal
 * switch can lag the reply by more than one retry window), and skipping the
 * direct check on an unconfirmed reply previously meant a switch that DID
 * happen could go completely undetected. Retries several times before
 * giving up on candidate.
 *
 * On failure, re-syncs communication via mosaicFindCommandPrompt() - but
 * that only knows about mosaicBaudCandidates, not candidate itself, so if
 * the module switched to a rate outside that list and our direct retries
 * still didn't catch it, communication is genuinely lost and a hardware
 * reset ('g') is the only way back.
 *
 * Returns true if candidate was confirmed and is now the active/known rate.
 */
static bool mosaicTrySetBaud(HardwareSerial &ser, uint32_t candidate)
{
    char cmd[48];
    snprintf(cmd, sizeof(cmd), "scs,COM1,baud%lu,bits8,No,bit1,none\n\r", (unsigned long)candidate);

    systemPrintf("Attempting to raise COM1 to %lu baud...\r\n", (unsigned long)candidate);

    while (ser.available())
        ser.read();
    ser.print(cmd);

    bool confirmed = mosaicWaitForPrompt(ser, "COMSettings", TIMEOUT_POLL);

    bool responding = false;
    for (uint8_t attempt = 0; attempt < 5 && !responding; attempt++)
        responding = mosaicTryBaud(ser, candidate, nullptr, 0);

    if (responding)
    {
        systemPrintf("  COM1 now running at %lu baud.\r\n", (unsigned long)candidate);
        mosaicKnownBaud = candidate;
        return true;
    }

    systemPrintf("  %lu baud not usable (%s) - reconnecting at a known rate...\r\n", (unsigned long)candidate,
                 confirmed ? "no response after switch" : "change not confirmed");

    if (mosaicFindCommandPrompt(ser) == false)
        systemPrintf("  ERROR: lost communication with the module. It may be stuck at %lu baud, which isn't "
                     "in the recovery list - try 'g' to hardware-reset it.\r\n",
                     (unsigned long)candidate);

    return false;
}

/*
 * mosaicRaiseBaud()
 *
 * Tries mosaicUpgradeBaudCandidates in order (fastest first) via
 * mosaicTrySetBaud(), stopping at the first one that's confirmed working.
 *
 * Leaves mosaicKnownBaud at whatever rate is left working - the original
 * rate if every candidate failed.
 *
 * Returns true if the baud was successfully raised.
 */
static bool mosaicRaiseBaud(HardwareSerial &ser)
{
    for (uint8_t i = 0; i < (sizeof(mosaicUpgradeBaudCandidates) / sizeof(mosaicUpgradeBaudCandidates[0])); i++)
    {
        uint32_t candidate = mosaicUpgradeBaudCandidates[i];
        if (candidate <= mosaicKnownBaud)
            break; // Candidates are listed fastest-first; nothing slower is worth trying

        if (mosaicTrySetBaud(ser, candidate))
            return true;
    }

    return false;
}

// Baud rates tried (ascending) by mosaicFindMaxBaudRate() to empirically
// find the fastest COM1 rate this specific module + wiring can sustain.
static const uint32_t mosaicBaudSweep[] = {921600,  1000000, 1500000, 2000000, 2500000,
                                            3000000, 3500000, 4000000, 4500000, 5000000};

/*
 * mosaicFindMaxBaudRate()
 *
 * Diagnostic: (1) scans mosaicBaudCandidates to establish communication at
 * whatever rate the module is currently running (mosaicFindCommandPrompt()),
 * then (2) walks mosaicBaudSweep upward one step at a time via
 * mosaicTrySetBaud(), stopping at the first rate that doesn't work. Reports
 * the highest confirmed rate.
 *
 * Leaves mosaicKnownBaud (and ser's active baud) at that highest rate.
 */
void mosaicFindMaxBaudRate(HardwareSerial &ser)
{
    systemPrintln("=== Finding max COM1 baud rate ===");

    if (mosaicFindCommandPrompt(ser) == false)
    {
        systemPrintln("No response from module at any known baud rate.");
        return;
    }

    systemPrintf("Starting point: %lu baud confirmed.\r\n", (unsigned long)mosaicKnownBaud);

    for (uint8_t i = 0; i < (sizeof(mosaicBaudSweep) / sizeof(mosaicBaudSweep[0])); i++)
    {
        if (mosaicBaudSweep[i] <= mosaicKnownBaud)
            continue;

        if (mosaicTrySetBaud(ser, mosaicBaudSweep[i]) == false)
            break; // mosaicTrySetBaud() already reconnected at the last working rate
    }

    systemPrintf("=== Max confirmed COM1 baud rate: %lu ===\r\n", (unsigned long)mosaicKnownBaud);
}

/*
 * mosaicEnterBootloaderMode()
 *
 * Does NOT hardware-reset the module - a reset can take up to ~10 seconds
 * to reboot, and we don't need one: communication has already been proven
 * out (mosaicKnownBaud), so we reuse it directly. Finds the command prompt,
 * attempts to raise the link speed (see mosaicRaiseBaud() - not fatal if it
 * doesn't work), sends the confirmed upgrade-mode trigger command, and waits
 * for the confirmed "Ready for SUF download" response.
 *
 * Returns true on success.
 */
bool mosaicEnterBootloaderMode()
{
    if (mosaicFindCommandPrompt(*serialGNSS) == false)
        return false;

    uint32_t baudBeforeRaise = mosaicKnownBaud;
    bool raised = mosaicRaiseBaud(*serialGNSS);
    uint32_t raisedBaud = mosaicKnownBaud;

    systemPrintln("Requesting firmware upgrade mode...");
    serialGNSS->print(MOSAIC_FW_UPDATE_TRIGGER_CMD);

    if (mosaicWaitForPrompt(*serialGNSS, MOSAIC_SUF_READY_TEXT, TIMEOUT_BOOTLOADER_ENTRY) == false)
    {
        // The receiver's internal reset into upgrade mode may not carry a
        // just-raised baud forward - if we raised it, retry once at the
        // rate that was confirmed working immediately beforehand.
        if (!raised)
        {
            systemPrintln("  ERROR: receiver did not report ready for SUF download.");
            return false;
        }

        systemPrintf("  No response at %lu after upgrade trigger - retrying at %lu...\r\n",
                     (unsigned long)raisedBaud, (unsigned long)baudBeforeRaise);
        serialGNSS->updateBaudRate(baudBeforeRaise);
        mosaicKnownBaud = baudBeforeRaise;
        delay(10);

        if (mosaicWaitForPrompt(*serialGNSS, MOSAIC_SUF_READY_TEXT, TIMEOUT_BOOTLOADER_ENTRY) == false)
        {
            systemPrintln("  ERROR: receiver did not report ready for SUF download.");
            return false;
        }
    }

    systemPrintln("  Receiver is ready for SUF download.");
    return true;
}

/*
 * mosaicFirmwareUpdateBegin()
 *
 * Puts the receiver into upgrade mode (see mosaicEnterBootloaderMode()) so
 * streaming of the .suf file via mosaicUpdateFirmware() can begin.
 *
 * Returns true on success.
 */
bool mosaicFirmwareUpdateBegin()
{
    return mosaicEnterBootloaderMode();
}

/*
 * mosaicFirmwareUpdateEnd()
 *
 * The .suf transfer has no explicit end-of-transfer marker that's been
 * confirmed yet (see file header) - this just reports whether the upload
 * itself completed cleanly.
 *
 * uploadSucceeded should be the return value of mosaicStreamFirmware()'s
 * byte-streaming loop.
 *
 * Returns uploadSucceeded.
 */
bool mosaicFirmwareUpdateEnd(bool uploadSucceeded)
{
    if (!uploadSucceeded)
    {
        systemPrintln("Skipping - firmware upload did not complete successfully.");
        return false;
    }

    systemPrintln("SUF transfer complete.");
    return true;
}

// Confirmed on real hardware: the module is unresponsive for a while after
// a .suf transfer while it reboots into the new image. mosaicFinishUpdate()
// polls once per TIMEOUT_POLL (1 second) for up to this long before giving
// up on MOSAIC_NORMAL_BAUD and falling back to a full rescan.
#define TIMEOUT_POST_UPDATE_BOOT 30000UL

/*
 * mosaicFinishUpdate()
 *
 * Call after mosaicStreamFirmware() (regardless of whether it succeeded).
 * The module reboots into the new image and is unresponsive for a while, so
 * this polls for it once per second, up to TIMEOUT_POST_UPDATE_BOOT total,
 * at MOSAIC_NORMAL_BAUD (460800) - confirmed on real hardware to be the rate
 * a completed update boots back up at, regardless of what (possibly much
 * higher) baud the transfer itself ran at; polling at the transfer baud
 * here previously just wasted the whole window on doomed attempts. Falls
 * back to a full mosaicFindCommandPrompt() scan only if that doesn't pan
 * out within the poll window. Once reconnected, ensures COM1 is left at
 * MOSAIC_NORMAL_BAUD, then queries and prints the (hopefully new) firmware
 * version.
 */
void mosaicFinishUpdate(HardwareSerial &ser)
{
    uint8_t maxPolls = TIMEOUT_POST_UPDATE_BOOT / TIMEOUT_POLL;

    systemPrintf("Polling for module at %lu baud (once per second, up to %lu seconds)...\r\n",
                 (unsigned long)MOSAIC_NORMAL_BAUD, (unsigned long)(TIMEOUT_POST_UPDATE_BOOT / 1000));

    ser.updateBaudRate(MOSAIC_NORMAL_BAUD);
    delay(10);

    bool reconnected = false;
    for (uint8_t attempt = 1; attempt <= maxPolls && !reconnected; attempt++)
    {
        while (ser.available())
            ser.read();
        ser.print(MOSAIC_ESCAPE_SEQUENCE);

        reconnected = mosaicWaitForPrompt(ser, MOSAIC_PROMPT, TIMEOUT_POLL);
        systemPrintf("  Poll %d/%d: %s\r\n", attempt, maxPolls, reconnected ? "responded" : "no response");
    }

    if (reconnected)
        mosaicKnownBaud = MOSAIC_NORMAL_BAUD;
    else
    {
        systemPrintf("No response at %lu after polling - falling back to a full rescan...\r\n",
                     (unsigned long)MOSAIC_NORMAL_BAUD);
        if (mosaicFindCommandPrompt(ser) == false)
        {
            systemPrintln("Module did not respond after the update.");
            return;
        }
    }

    if (mosaicKnownBaud != MOSAIC_NORMAL_BAUD)
        mosaicTrySetBaud(ser, MOSAIC_NORMAL_BAUD);

    mosaicGetVersion(ser);
}

// Update the mosaic-X5 firmware
// Owns the full update sequence: enters upgrade mode, streams the .suf file
// over WiFi, then closes out the transfer - callers only need to call this
// one function and do not need to know about Begin()/End().
bool mosaicStreamFirmware(char *relativeFirmwareFileLocation)
{
    if (relativeFirmwareFileLocation == nullptr)
    {
        systemPrintln("Firmware file location is null.");
        return false;
    }

    systemPrintln("Starting mosaic-X5 firmware update...");

    firmwareUpdateProgressReset();

    if (mosaicFirmwareUpdateBegin() == false)
    {
        systemPrintln("Failed to enter upgrade mode.");
        return false;
    }

    systemPrintln("Device is in upgrade mode.");
    systemPrintf("Streaming .suf file at %lu baud...\r\n", (unsigned long)mosaicKnownBaud);

    WiFiClientSecure client;
    if (!otaSecurelyConnectGitHub(client))
    {
        systemPrintln("Failed to securely connect to GitHub.");
        mosaicFirmwareUpdateEnd(false);
        return false;
    }

    const char *url = otaGetGithubFileLocation(relativeFirmwareFileLocation);

    HTTPClient http;
    if (!http.begin(client, url))
    {
        systemPrintln("Unable to begin HTTP request.");
        mosaicFirmwareUpdateEnd(false);
        return false;
    }

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK)
    {
        systemPrintf("HTTP GET failed, code: %d\r\n", httpCode);
        http.end();
        mosaicFirmwareUpdateEnd(false);
        return false;
    }

    int contentLength = http.getSize();
    if (contentLength > 0)
        firmwareUpdateBytesToProcess = (uint32_t)contentLength;

    WiFiClient *stream = http.getStreamPtr();
    // static, not stack-local: HTTPClient/WiFiClientSecure's own TLS handshake
    // internals are already fairly stack-hungry, and a buffer this size on the
    // stack blew loopTask's 8KB stack during the GET (confirmed - crashed as
    // "Stack canary watchpoint triggered" right after bumping this from 2048
    // to 4096 as a plain stack array).
    static uint8_t buffer[4096];

    bool success = true;

    while (http.connected() && (contentLength > 0 || contentLength == -1))
    {
        size_t available = stream->available();
        if (available == 0)
        {
            if (!client.connected())
                break;
            // yield(), not delay(1): delay(1) blocks for a full ~1ms RTOS
            // tick regardless of when data actually shows up; yield() just
            // hands off to the scheduler (letting WiFi/lwIP run) and returns
            // as soon as it's rescheduled, so newly-arrived bytes get picked
            // up sooner instead of always waiting out the tick.
            yield();
            continue;
        }

        size_t toRead = min(available, sizeof(buffer));
        int bytesRead = stream->readBytes(buffer, toRead);
        if (bytesRead <= 0)
            break;

        if (mosaicUpdateFirmware(*serialGNSS, buffer, (uint32_t)bytesRead) == false)
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
        systemPrintln("mosaic-X5 update successfully streamed.");
    else
        systemPrintln("mosaic-X5 firmware update failed.");

    systemPrintln("Finalizing transfer...");
    bool updateOk = mosaicFirmwareUpdateEnd(success);

    return updateOk;
}
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// End of mosaic-X5 firmware update functions.
