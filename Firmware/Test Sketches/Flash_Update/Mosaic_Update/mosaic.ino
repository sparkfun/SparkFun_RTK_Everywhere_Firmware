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

// Increase bootload speed as much as possible because the SUF files are ~22MB.
//
// IMPORTANT: only meaningful on mosaicUpdateSerial() (COM1) - the module's
// dedicated high-throughput data port, confirmed on real hardware to sustain
// multi-Mbps rates. They are NOT meaningful on COM4: raw byte capture
// (mosaicTrySetBaud()'s DEBUG output) proved that raising COM4 produces pure
// line noise even at 921600 - different garbage on every identical retry,
// which is the signature of a physical signal-integrity problem, not a
// protocol rejection or a timing race. COM4 is wired as a low-speed ASCII
// console channel only (see mosaicCommandSerial()) and mosaicFindCommandPrompt()
// deliberately never raises its baud. (mosaicComRates[]/GNSS_Mosaic.h,
// production's own COM-port-baud table, tops out at 921600 - but that table
// is curated for the normal-operation RTCM/NMEA settings menu, which never
// needs more, not a statement of the receiver's absolute protocol ceiling. It
// does not apply here.)
//
// mosaicFindMaxBaudRate() (the 'b' diagnostic) climbs this list ascending, one
// step at a time, to find where the ceiling actually is - appropriate for a
// "how fast can this link go" probe, which needs to see the failure point.
//
// mosaicRaiseBaud() (the real firmware-update path) instead jumps straight to
// the top and works down on failure - see mosaicUpgradeBaudCandidates below.
// The two used to share one ascending-only strategy, because a same-hardware
// test once showed a direct 115200->4000000 jump failing outright (0 bytes on
// every verify attempt) while climbing through these same steps landed on the
// first try every time. That test predates a since-fixed bug where
// mosaicTryBaud()'s own priming command (previously "sdio,%s,CMD,SBF") was
// leaving the port streaming live SBF binary data that interleaved with and
// corrupted the very reply text being searched for - worse at higher baud,
// since more interleaved traffic arrives per unit time. With that fixed (see
// mosaicTryBaud()'s "CMD,None"), a direct jump is worth retrying: it costs
// nothing extra when it works (one command instead of eight), and the
// mosaicUpgradeBaudCandidates fallback below still catches it if it doesn't.
static const uint32_t mosaicBaudSweep[] = {921600,  1000000, 1500000, 2000000, 2500000,
                                            3000000, 3500000, 4000000, 4500000, 5000000};

// mosaicRaiseBaud() tries these in order (fastest first), stopping at the
// first one that's confirmed working - i.e. jump straight to 4000000, and
// only work down through progressively slower rates if that fails. Same
// values as mosaicBaudSweep (minus 4500000/5000000, which the receiver's own
// "$R? setCOMSettings" reply confirmed are invalid enum values - not this
// sketch's baud candidates to skip, an actual protocol rejection), just
// tried in the opposite order for the opposite purpose.
static const uint32_t mosaicUpgradeBaudCandidates[] = {4000000, 3500000, 3000000, 2500000,
                                                       2000000, 1500000, 1000000, 921600};

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

// mosaicFindCommandPrompt()'s patient COM4 boot-wait: COM4_BOOT_RETRIES *
// (COM4_BOOT_CMD_TIMEOUT_MS + COM4_BOOT_ESC_TIMEOUT_MS) is the total ceiling
// (~60s, same as production GNSS_MOSAIC::isPresentOnSerial()'s 25*2500ms) -
// but split into more, shorter attempts so a module that wakes up mid-window
// is noticed sooner, and so the verbose progress printing gives fine-grained
// visibility into an otherwise-silent multi-second wait.
#define COM4_BOOT_RETRIES 40
#define COM4_BOOT_CMD_TIMEOUT_MS 1000UL
#define COM4_BOOT_ESC_TIMEOUT_MS 500UL

// Baud rates the mosaic-X5 command line is known/likely to be running at.
static const uint32_t mosaicBaudCandidates[] = {460800, 921600, 115200, 230400, 9600};

// Baud rate found by the most recent successful mosaicFindCommandPrompt()
// call. Tried first on subsequent calls so a normal call doesn't have to
// re-scan mosaicBaudCandidates every time.
static uint32_t mosaicKnownBaud = 0;

// Forces the next mosaicFindCommandPrompt() call to do a full rescan instead
// of trying mosaicKnownBaud first. mosaicKnownBaud is file-scope static, so
// this wrapper (a function, unlike a plain variable, gets an auto-generated
// prototype from the Arduino build) is how other tabs invalidate it - e.g.
// after a hardware reset/power-cycle of the module.
void mosaicForceRescan()
{
    mosaicKnownBaud = 0;
}

// Drains ser's RX buffer, then keeps draining for as long as new bytes keep
// arriving, only stopping once quietMs has passed with nothing new. A single
// "while (ser.available()) ser.read();" pass only clears what's buffered at
// that exact instant - it can race with a byte still in flight (e.g. a
// trailing/leftover fragment of the PREVIOUS exchange's reply that hasn't
// finished arriving yet), leaving it to be read moments later by whichever
// mosaicWaitForPrompt() call runs next. Confirmed on real hardware: a
// "COMSettings"-wait capture consisted of nothing but a stray "COM1>" (no
// "COMSettings" substring at all) followed by total silence for the rest of
// the timeout - consistent with a stale prompt fragment being mistaken for
// the start of a real reply, not a garbled/absent one.
static void mosaicFlushSerial(HardwareSerial &ser, unsigned long quietMs = 5)
{
    unsigned long lastByteMillis = millis();
    while ((millis() - lastByteMillis) < quietMs)
    {
        if (ser.available())
        {
            ser.read();
            lastByteMillis = millis();
        }
        else
            yield();
    }
}

// Prints buf as visible ASCII where possible and \xNN for everything else
// (control chars, high-bit garbage from a baud mismatch, or simply nothing at
// all - an empty dump means truly zero bytes arrived, not a formatting quirk).
// Used to see exactly what the module said - or didn't - instead of
// collapsing every failure into a single "not confirmed" bucket.
static void mosaicPrintRawBytes(const char *label, const char *buf, size_t len)
{
    systemPrintf("  DEBUG %s (%u bytes): \"", label, (unsigned)len);
    for (size_t i = 0; i < len; i++)
    {
        uint8_t c = (uint8_t)buf[i];
        if (c >= 0x20 && c < 0x7F)
            systemPrintf("%c", c);
        else
            systemPrintf("\\x%02X", c);
    }
    systemPrintln("\"");
}

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

// Which physical mosaic COM port ser is wired to - derived from the object
// identity (does ser refer to serial2GNSS, the Facet mosaic COM4 link?)
// rather than productVariant, so these helpers give the right answer no
// matter which of mosaicCommandSerial() (COM4, Facet mosaic only) or
// mosaicUpdateSerial() (COM1, both platforms) a caller passed in. On FP,
// serial2GNSS is never allocated, so this always resolves to COM1 there.
static const char *mosaicPortNameFor(HardwareSerial &ser)
{
    return (&ser == serial2GNSS) ? "COM4" : "COM1";
}

static const char *mosaicPromptFor(HardwareSerial &ser)
{
    return (&ser == serial2GNSS) ? "COM4>" : "COM1>";
}

// Switch to baud, then retry the escape+prompt a few times. The module
// streams NMEA/SBF continuously once booted (confirmed via raw-byte capture
// elsewhere in this codebase - it is never silent), so a single attempt can
// land mid-sentence and miss the prompt within its own timeout.
//
// attempts/cmdTimeoutMs/escTimeoutMs default to a quick 3-try/1000ms budget,
// suitable once the module is already known to be up (e.g. mosaicTrySetBaud
// re-verifying after a baud change). Callers that may be racing the module's
// own boot (see mosaicFindCommandPrompt()) pass a much more patient budget.
//
// verbose prints one line per attempt (millis() timestamp, which of the two
// sub-probes - "sdio...CMD,None"->"DataInOut" and escape->prompt - answered)
// so a slow or failed connection shows exactly where time went instead of
// looking like a silent hang. Only the initial boot-time connection passes
// true; the quick re-verify paths (mosaicTrySetBaud, etc.) would just be
// noise at their much larger attempt counts under normal operation.
static bool mosaicTryBaud(HardwareSerial &ser, uint32_t baud, char *response, size_t responseSize,
                           uint8_t attempts = 3, uint32_t cmdTimeoutMs = TIMEOUT_POLL,
                           uint32_t escTimeoutMs = TIMEOUT_POLL, bool verbose = false)
{
    ser.updateBaudRate(baud);
    delay(10);

    const char *portName = mosaicPortNameFor(ser);
    const char *prompt = mosaicPromptFor(ser);

    for (uint8_t attempt = 0; attempt < attempts; attempt++)
    {
        mosaicFlushSerial(ser);

        // Any mosaic COM port can be left streaming SBF/NMEA/RTCM rather than
        // sitting at its command prompt - forcing CMD-only I/O first is what
        // actually produces the prompt on the next escape sequence. The plain
        // escape attempt right below still runs regardless of whether
        // "DataInOut" was seen.
        //
        // TxDataType is "None", NOT "SBF": ASCII command replies/prompts are a
        // separate mechanism from the port's configured data-output stream,
        // so disabling data output here loses nothing - but leaving it as
        // "SBF" (copied from production GNSS_MOSAIC::isPresentOnSerial(),
        // where it's deliberately needed to fetch a ReceiverSetup block
        // afterward) actively re-enables a competing binary stream on every
        // single attempt. Real capture on real hardware: a "COMSettings"-wait
        // response started with a clean "COM1>" prompt immediately followed
        // by "$@..." - the literal 2-byte SBF sync pattern - i.e. real SBF
        // blocks interleaved with our command replies, not electrical noise.
        // That's the same shape as most of the "garbled bytes" seen
        // throughout this file's DEBUG output, and almost certainly why
        // raising baud sometimes failed with zero or scrambled verify bytes:
        // the higher the baud, the more interleaved SBF traffic arrives in
        // the same time window, the more likely it swamps the exact
        // substring match mosaicWaitForPrompt() is looking for.
        char sdioCmd[24];
        snprintf(sdioCmd, sizeof(sdioCmd), "sdio,%s,CMD,None\n\r", portName);
        ser.print(sdioCmd);
        bool sawDataInOut = mosaicWaitForPrompt(ser, "DataInOut", cmdTimeoutMs, response, responseSize);

        bool sawPrompt = false;
        if (sawDataInOut)
        {
            ser.print(MOSAIC_ESCAPE_SEQUENCE);
            sawPrompt = mosaicWaitForPrompt(ser, prompt, escTimeoutMs, response, responseSize);
        }

        if (!sawPrompt)
        {
            ser.print(MOSAIC_ESCAPE_SEQUENCE);
            sawPrompt = mosaicWaitForPrompt(ser, prompt, escTimeoutMs, response, responseSize);
        }

        if (verbose)
            systemPrintf("    [%lums] attempt %d/%d @ %lu baud: DataInOut %s, %s %s\r\n", millis(), attempt + 1,
                         attempts, (unsigned long)baud, sawDataInOut ? "seen" : "not seen", prompt,
                         sawPrompt ? "seen" : "not seen");

        if (sawPrompt)
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

    // Facet mosaic's COM4 (mosaicCommandSerial()) is 115200 by hardware
    // default and NOTHING in this sketch ever raises it (COM4 physically
    // can't hold a higher rate, confirmed by garbage-byte capture in
    // mosaicTrySetBaud()'s DEBUG output), so unlike COM1, its baud never
    // changes. Scanning mosaicBaudCandidates on
    // it would just burn through the X5's boot window (documented ~10s
    // typical, can run longer) on rates that can never apply. Production
    // GNSS_Mosaic.ino::isPresent() instead polls patiently at the one true
    // rate rather than soft-resetting a module that may simply still be
    // booting. Mirror that here - but with a shorter per-attempt command
    // timeout (COM4_BOOT_CMD_TIMEOUT_MS 1000ms vs production's 2000ms) and
    // proportionally more attempts, for the same ~60s total ceiling at finer
    // granularity: once the module actually wakes up, we notice up to 1.5s
    // late instead of up to 2.5s late, and the verbose=true below prints
    // every attempt so a slow or failed boot is visible in real time instead
    // of a silent multi-second gap that looks identical to a hang.
    if (strcmp(mosaicPortNameFor(ser), "COM4") == 0)
    {
        unsigned long startMillis = millis();
        systemPrintf("[%lums] Checking communication at 115200 (module may still be booting, this can take up to "
                     "a minute)...\r\n",
                     startMillis);
        if (mosaicTryBaud(ser, 115200, response, responseSize, COM4_BOOT_RETRIES, COM4_BOOT_CMD_TIMEOUT_MS,
                          COM4_BOOT_ESC_TIMEOUT_MS, true))
        {
            systemPrintf("  OK at 115200 baud after %lums.\r\n", millis() - startMillis);
            mosaicKnownBaud = 115200;
            return true;
        }
        systemPrintf("  No response at 115200 baud after %lums.\r\n", millis() - startMillis);
        return false;
    }

    // COM1 (mosaicUpdateSerial(), on both platforms) IS raised during a
    // firmware update, so unlike COM4 its baud can genuinely be anything in
    // mosaicBaudCandidates - including a rate left over from a previous
    // session's mosaicTrySetBaud() that this sketch's own verification of it
    // failed to catch (the module's internal switch has been observed to
    // take effect even when the "COMSettings" reply is missed - see
    // mosaicTrySetBaud()'s doc comment). Ideally this whole scan is skipped
    // entirely: see mosaicSyncUpdatePortBaud(), called right after the COM4
    // version check in setup() to deterministically set COM1's baud via COM4
    // instead of leaving it to be rediscovered here. This scan is what runs
    // if that sync was never done (FP - see below) or didn't stick.
    unsigned long scanStartMillis = millis();
    for (uint8_t i = 0; i < (sizeof(mosaicBaudCandidates) / sizeof(mosaicBaudCandidates[0])); i++)
    {
        systemPrintf("[%lums] Checking communication at %d...\r\n", millis(), mosaicBaudCandidates[i]);

        if (mosaicTryBaud(ser, mosaicBaudCandidates[i], response, responseSize, 3, TIMEOUT_POLL, TIMEOUT_POLL, true))
        {
            systemPrintf("  OK at %d baud after %lums.\r\n", mosaicBaudCandidates[i], millis() - scanStartMillis);
            mosaicKnownBaud = mosaicBaudCandidates[i];
            return true;
        }
        systemPrintf("  No response at %d baud.\r\n", mosaicBaudCandidates[i]);
    }

    // Last resort: a rate this sketch asked for (mosaicBaudSweep, via
    // mosaicRaiseBaud()/mosaicFindMaxBaudRate()) that isn't in mosaicBaudCandidates.
    for (uint8_t i = 0; i < (sizeof(mosaicBaudSweep) / sizeof(mosaicBaudSweep[0])); i++)
    {
        systemPrintf("[%lums] Checking communication at %lu (recovering from a previous baud change)...\r\n",
                     millis(), (unsigned long)mosaicBaudSweep[i]);
        if (mosaicTryBaud(ser, mosaicBaudSweep[i], response, responseSize, 3, TIMEOUT_POLL, TIMEOUT_POLL, true))
        {
            systemPrintf("  OK at %lu baud after %lums.\r\n", (unsigned long)mosaicBaudSweep[i],
                         millis() - scanStartMillis);
            mosaicKnownBaud = mosaicBaudSweep[i];
            return true;
        }
        systemPrintf("  No response at %lu baud.\r\n", (unsigned long)mosaicBaudSweep[i]);
    }
    return false;
}

/*
 * mosaicSyncUpdatePortBaud()
 *
 * Facet mosaic only (requires serial2GNSS/COM4, which FP doesn't have).
 * Deterministically sets mosaicUpdateSerial() (COM1)'s baud via COM4 instead
 * of leaving it to be rediscovered later by mosaicFindCommandPrompt()'s
 * multi-candidate scan.
 *
 * Why this matters: COM4 is already fully awake and proven reliable by the
 * time this runs (right after the initial version check in setup()) - and
 * "scs" takes an explicit target port, so it can configure COM1 regardless
 * of what COM1 is currently doing or what baud it's left over at from a
 * previous session (this sketch never persists a raised baud - see
 * mosaicTrySetBaud() - so COM1 can be sitting at 115200, 230400, or whatever
 * an earlier run's raise-and-partial-verify left it at). Without this,
 * mosaicEnterBootloaderMode()'s first call to mosaicFindCommandPrompt() has
 * to blindly scan mosaicBaudCandidates (460800, 921600, 115200, 230400,
 * 9600) - observed on real hardware taking 4 wrong guesses before landing on
 * the right one. Calling this instead turns that scan into a single command.
 *
 * Verifies twice, for different reasons: sees "COMSettings" on COM4 (proves
 * the receiver accepted and parsed the command) AND then a direct
 * mosaicTryBaud() on COM1 itself at the new rate (proves the switch actually
 * took - mosaicTrySetBaud()'s doc comment notes the module has been observed
 * to switch even when the COM4 reply is missed, and the reverse - a reply
 * seen without the switch sticking - isn't ruled out either, so only the
 * direct check is trustworthy on its own).
 *
 * On success, sets mosaicKnownBaud so the very next mosaicFindCommandPrompt()
 * call (from mosaicEnterBootloaderMode()) hits its fast path and skips
 * scanning entirely. On failure, prints why and leaves mosaicKnownBaud alone
 * so that scan still runs as a fallback - never worse than not calling this.
 *
 * Returns true if COM1 was confirmed at baud.
 */
bool mosaicSyncUpdatePortBaud(uint32_t baud)
{
    if (serial2GNSS == nullptr)
        return false;

    HardwareSerial *updateSerial = mosaicUpdateSerial();
    const char *updatePortName = mosaicUpdatePortName();

    char cmd[48];
    snprintf(cmd, sizeof(cmd), "scs,%s,baud%lu,bits8,No,bit1,none\n\r", updatePortName, (unsigned long)baud);

    systemPrintf("Pre-syncing %s to %lu baud via %s (skips COM1 baud-guessing later)...\r\n", updatePortName,
                 (unsigned long)baud, mosaicCommandPortName());

    mosaicFlushSerial(*serial2GNSS);
    serial2GNSS->print(cmd);

    bool comSettingsSeen = mosaicWaitForPrompt(*serial2GNSS, "COMSettings", TIMEOUT_POLL);

    if (mosaicTryBaud(*updateSerial, baud, nullptr, 0))
    {
        systemPrintf("  %s confirmed at %lu baud.\r\n", updatePortName, (unsigned long)baud);
        mosaicKnownBaud = baud;
        return true;
    }

    systemPrintf("  %s did not respond at %lu baud (COM4 %s the scs command) - falling back to scanning when "
                 "needed.\r\n",
                 updatePortName, (unsigned long)baud, comSettingsSeen ? "confirmed" : "did not confirm");
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

    mosaicFlushSerial(ser);
    ser.print("lif,Identification\n\r");

    static char response[1024 * 4]; // XML is ~3k
    if (mosaicWaitForPrompt(ser, mosaicPromptFor(ser), TIMEOUT_IDENTIFICATION, response, sizeof(response)) == false)
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
 * Asks the module to switch its COM port to candidate via "scs" (Set COM
 * Settings) and confirms we can still talk to it there before accepting it.
 * The command must name the SAME physical port ser is wired to
 * (mosaicPortNameFor(ser) - always COM1 on FP; COM1 or COM4 on Facet mosaic
 * depending on which serial object is passed): "scs" takes an explicit port
 * argument, so it can just as easily reconfigure a port other than the one
 * that sent it. Naming the wrong port here would raise a
 * baud rate nothing below verifies or streams over, while the port actually
 * in use (ser) stays at its old baud - so every verification attempt would
 * fail and the update would silently never get faster than mosaicKnownBaud's
 * starting rate. Command syntax ("scs,COMx,baudNNNNNN,bits8,No,bit1,none")
 * and the "COMSettings" reply are the same pattern production GNSS_Mosaic.ino
 * uses for other COM ports; only 921600 is confirmed there, so candidates
 * above that are opportunistic.
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
    const char *portName = mosaicPortNameFor(ser);

    char cmd[48];
    snprintf(cmd, sizeof(cmd), "scs,%s,baud%lu,bits8,No,bit1,none\n\r", portName, (unsigned long)candidate);

    systemPrintf("Attempting to raise %s to %lu baud...\r\n", portName, (unsigned long)candidate);
    mosaicPrintRawBytes("TX", cmd, strlen(cmd));

    mosaicFlushSerial(ser);
    ser.print(cmd);

    static char commandSettingsResponse[128];
    bool confirmed = mosaicWaitForPrompt(ser, "COMSettings", TIMEOUT_POLL, commandSettingsResponse,
                                         sizeof(commandSettingsResponse));
    mosaicPrintRawBytes("RX while waiting for COMSettings reply (still at old baud)", commandSettingsResponse,
                        strlen(commandSettingsResponse));

    bool responding = false;
    static char verifyResponse[128];
    for (uint8_t attempt = 0; attempt < 5 && !responding; attempt++)
    {
        responding = mosaicTryBaud(ser, candidate, verifyResponse, sizeof(verifyResponse));
        systemPrintf("  DEBUG verify attempt %d at %lu baud: %s\r\n", attempt + 1, (unsigned long)candidate,
                     responding ? "responded" : "no response");
        if (!responding)
            mosaicPrintRawBytes("RX during verify attempt (now at new baud)", verifyResponse, strlen(verifyResponse));
    }

    if (responding)
    {
        systemPrintf("  %s now running at %lu baud.\r\n", portName, (unsigned long)candidate);
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
 * First, an unconditional hop to 921600 - not for its own sake (it's below
 * every candidate in mosaicUpgradeBaudCandidates), but because it's been
 * confirmed instant and clean from a 115200 baseline on every test run so
 * far, every single time. Landing there first turns the jump to 4000000
 * from ~34.7x the starting baud into ~4.3x, in case the sheer SIZE of a
 * single relative jump is a real contributing factor alongside (not just
 * instead of) the SBF-interleaving bug already fixed in mosaicTryBaud().
 * If this hop itself fails, mosaicKnownBaud is left wherever
 * mosaicTrySetBaud()'s own recovery landed it (typically back at the
 * original baseline) and the loop below simply proceeds from there - never
 * worse than skipping this hop entirely.
 *
 * Then: jumps straight to mosaicUpgradeBaudCandidates[0] (4000000) via
 * mosaicTrySetBaud(); if that fails, works down through progressively slower
 * candidates, stopping at the first one that's confirmed working. See
 * mosaicUpgradeBaudCandidates' comment for why a direct jump is worth trying
 * again despite an earlier same-hardware test showing it fail outright: that
 * failure has since been tied to a since-fixed bug (mosaicTryBaud()'s own
 * priming command polluting the link with live SBF traffic), not necessarily
 * the size of the jump itself - hence hedging with the 921600 hop above too.
 *
 * Each candidate that fails leaves mosaicKnownBaud back at the last confirmed
 * rate (mosaicTrySetBaud() reconnects internally), so the next, slower
 * candidate is attempted from a known-good starting point rather than
 * compounding failures.
 *
 * Returns true if the baud was raised at least one step above where it started.
 */
static bool mosaicRaiseBaud(HardwareSerial &ser)
{
    uint32_t startingBaud = mosaicKnownBaud;

    if (mosaicKnownBaud < 921600)
        mosaicTrySetBaud(ser, 921600);

    for (uint8_t i = 0; i < (sizeof(mosaicUpgradeBaudCandidates) / sizeof(mosaicUpgradeBaudCandidates[0])); i++)
    {
        uint32_t candidate = mosaicUpgradeBaudCandidates[i];
        if (candidate <= mosaicKnownBaud)
            continue; // Not actually faster than where we already are - skip it

        if (mosaicTrySetBaud(ser, candidate))
            break; // Found a working rate - stop here, no need to try anything slower
    }

    return mosaicKnownBaud > startingBaud;
}

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
    const char *portName = mosaicPortNameFor(ser);
    systemPrintf("=== Finding max %s baud rate ===\r\n", portName);

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

    systemPrintf("=== Max confirmed %s baud rate: %lu ===\r\n", portName, (unsigned long)mosaicKnownBaud);
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
    HardwareSerial *updateSerial = mosaicUpdateSerial();

    if (mosaicFindCommandPrompt(*updateSerial) == false)
        return false;

    uint32_t baudBeforeRaise = mosaicKnownBaud;
    bool raised = mosaicRaiseBaud(*updateSerial);
    uint32_t raisedBaud = mosaicKnownBaud;

    systemPrintln("Requesting firmware upgrade mode...");
    updateSerial->print(MOSAIC_FW_UPDATE_TRIGGER_CMD);

    if (mosaicWaitForPrompt(*updateSerial, MOSAIC_SUF_READY_TEXT, TIMEOUT_BOOTLOADER_ENTRY) == false)
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
        updateSerial->updateBaudRate(baudBeforeRaise);
        mosaicKnownBaud = baudBeforeRaise;
        delay(10);

        if (mosaicWaitForPrompt(*updateSerial, MOSAIC_SUF_READY_TEXT, TIMEOUT_BOOTLOADER_ENTRY) == false)
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
 * this polls for it once per second, up to TIMEOUT_POST_UPDATE_BOOT total, at
 * MOSAIC_NORMAL_BAUD (460800) - confirmed on real hardware to be the rate a
 * completed update boots back up at on mosaicUpdateSerial() (COM1, on both
 * platforms - see that function), regardless of what (possibly much higher)
 * baud the transfer itself ran at; polling at the transfer baud here
 * previously just wasted the whole window on doomed attempts. Falls back to
 * a full mosaicFindCommandPrompt() scan only if that doesn't pan out within
 * the poll window. Once reconnected, ensures COM1 is left at MOSAIC_NORMAL_BAUD.
 *
 * Queries and prints the (hopefully new) firmware version once reconnected.
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
        mosaicFlushSerial(ser);
        ser.print(MOSAIC_ESCAPE_SEQUENCE);

        reconnected = mosaicWaitForPrompt(ser, mosaicPromptFor(ser), TIMEOUT_POLL);
        systemPrintf("  Poll %d/%d: %s\r\n", attempt, maxPolls, reconnected ? "responded" : "no response");
    }

    if (reconnected)
        mosaicKnownBaud = MOSAIC_NORMAL_BAUD;
    else
    {
        systemPrintf("No response at %lu after polling - falling back to a full rescan...\r\n",
                     (unsigned long)MOSAIC_NORMAL_BAUD);
        reconnected = mosaicFindCommandPrompt(ser);
    }

    if (reconnected && mosaicKnownBaud != MOSAIC_NORMAL_BAUD)
        mosaicTrySetBaud(ser, MOSAIC_NORMAL_BAUD);

    if (reconnected == false)
    {
        systemPrintln("Module did not respond after the update.");
        return;
    }

    mosaicGetVersion(ser);
}

// Update the mosaic-X5 firmware
// Owns the full update sequence: enters upgrade mode, streams the .suf file
// over WiFi, then closes out the transfer - callers only need to call this
// one function and do not need to know about Begin()/End().
bool mosaicStreamFirmware(const char *relativeFirmwareFileLocation)
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

        if (mosaicUpdateFirmware(*mosaicUpdateSerial(), buffer, (uint32_t)bytesRead) == false)
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
