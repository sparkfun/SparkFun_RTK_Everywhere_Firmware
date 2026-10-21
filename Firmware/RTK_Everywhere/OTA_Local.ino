/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
OTA_Local.ino

  Local firmware updates from files served by a phone app, driven over the CLI. The app
  downloads the firmware (over cellular), joins a soft AP started by the device, and serves
  the files over HTTP. The device then downloads and programs them using the same OTA.ino
  code as internet updates.

    1. $SPGET,subsystemVersions
           Chip and firmware version of each subsystem, so the app can pick the files
    2. $SPEXE,UPDATEAP
           Start a WPA2 soft AP with a unique SSID and password for this session. The
           response carries both: $SPEXE,UPDATEAP,"<ssid>","<password>",OK
    3. $SPGET,updateStatus
           Poll until AP_READY, then join the soft AP from the phone
    4. $SPEXE,UPDATEFILE,<subsystem>,<chip>,<url>,<bytes>,<crc32>
           Queue a file served by the phone, one per subsystem
    5. $SPEXE,UPDATESTART
           Download and program the queued files (ESP32 last), then reboot
    6. $SPGET,updateStatus
           Poll for progress: "<state>,<subsystem>,<percent>,<message>"

    $SPEXE,UPDATECANCEL stops the soft AP and forgets the queued files (not while updating).

  The device programs whatever version the app sends, older or newer. The soft AP stops
  after OTA_LOCAL_IDLE_TIMEOUT_MSEC without any of these commands.
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#ifdef COMPILE_FIRMWARE_UPDATE

//----------------------------------------
// Constants
//----------------------------------------

enum OtaLocalState
{
    OTA_LOCAL_IDLE = 0,    // No soft AP
    OTA_LOCAL_AP_STARTING, // Soft AP requested
    OTA_LOCAL_AP_READY,    // Waiting for UPDATEFILE and UPDATESTART
    OTA_LOCAL_UPDATING,    // OTA.ino is downloading and programming the files
    OTA_LOCAL_FAILED,      // Soft AP still running, the app may retry
    OTA_LOCAL_COMPLETE,    // Rebooting
    // Add new states above this line
    OTA_LOCAL_STATE_MAX
};

static const char *const otaLocalStateNames[] = {
    "IDLE", "AP_STARTING", "AP_READY", "UPDATING", "FAILED", "COMPLETE",
};
static_assert(sizeof(otaLocalStateNames) / sizeof(otaLocalStateNames[0]) == OTA_LOCAL_STATE_MAX,
              "Fix otaLocalStateNames to match OtaLocalState");

#define OTA_LOCAL_AP_START_TIMEOUT_MSEC (30 * 1000)
#define OTA_LOCAL_IDLE_TIMEOUT_MSEC (5 * 60 * 1000) // No local update commands: stop the soft AP
#define OTA_LOCAL_URL_LENGTH 128

// Password characters, without look-alikes (0/O, 1/l/I)
static const char otaLocalPasswordCharacters[] = "abcdefghjkmnpqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789";

//----------------------------------------
// Locals
//----------------------------------------

// The CLI (Bluetooth task) makes requests, otaLocalUpdate() (main loop) acts on them
static volatile uint8_t otaLocalState;
static volatile bool otaLocalRequestAp;        // UPDATEAP
static volatile bool otaLocalRequestStart;     // UPDATESTART
static volatile bool otaLocalRequestCancel;    // UPDATECANCEL
static volatile uint32_t otaLocalActivityMsec; // Last local update command, for the idle timeout
static uint32_t otaLocalStateMsec;             // Time of the last state change

// Files queued by UPDATEFILE, copied into otaTarget[] when the update starts
static volatile OTA_SUBSYSTEM_MASK otaLocalQueued;
static char otaLocalUrl[OTA_SUBSYSTEM_MAX][OTA_LOCAL_URL_LENGTH];
static size_t otaLocalFileBytes[OTA_SUBSYSTEM_MAX];
static uint32_t otaLocalCrc[OTA_SUBSYSTEM_MAX];

// Reported by $SPGET,updateStatus
static const char *volatile otaLocalSubsystem = ""; // Subsystem being updated, or the one that failed
static const char *otaLocalFailedSubsystem;         // First subsystem that failed during this update
static volatile int otaLocalPercent;
static char otaLocalMessage[48];

//----------------------------------------
// Change the local update state. message: nullptr or text without commas for updateStatus
//----------------------------------------
void otaLocalSetState(uint8_t newState, const char *message)
{
    if (settings.debugFirmwareUpdate)
        systemPrintf("Local update: %s --> %s%s%s\r\n", otaLocalStateNames[otaLocalState],
                     otaLocalStateNames[newState], message ? ", " : "", message ? message : "");

    snprintf(otaLocalMessage, sizeof(otaLocalMessage), "%s", message ? message : "");
    otaLocalStateMsec = millis();
    otaLocalState = newState;
}

//----------------------------------------
// Start the soft AP with the local update SSID and password (main loop)
//----------------------------------------
void otaLocalApStart()
{
#ifdef COMPILE_WIFI
    otaLocalApActive = true; // wifiSoftApOn() uses otaLocalApSsid and otaLocalApPassword
    networkSoftApConsumerAdd(NETCONSUMER_LOCAL_UPDATE, __FILE__, __LINE__);
#endif // COMPILE_WIFI
}

//----------------------------------------
// Stop the soft AP and forget the password and queued files (main loop)
//----------------------------------------
void otaLocalApStop()
{
#ifdef COMPILE_WIFI
    if (otaLocalApActive)
        networkSoftApConsumerRemove(NETCONSUMER_LOCAL_UPDATE, __FILE__, __LINE__);
#endif // COMPILE_WIFI
    otaLocalApActive = false;
    memset(otaLocalApPassword, 0, sizeof(otaLocalApPassword));
    otaLocalQueued = 0;
}

//----------------------------------------
// Copy the queued files into otaTarget[] and start OTA.ino's update (main loop)
//----------------------------------------
void otaLocalStartUpdate()
{
    OTA_SUBSYSTEM_MASK queued = otaLocalQueued;

    if (otaState != OTA_STATE_OFF)
    {
        otaLocalSetState(OTA_LOCAL_FAILED, "Another firmware update is running");
        return;
    }

    // Start from clean targets, then fill in the queued files. Everything else is skipped.
    otaCleanup(false);
    for (int subsystem = 0; subsystem < OTA_SUBSYSTEM_MAX; subsystem++)
    {
        OTA_TARGET *target = &otaTarget[subsystem];
        if ((queued & otaGetSubsystemMaskFromSubsystem(subsystem)) == 0)
        {
            target->_requestType = OTA_REQUEST_SKIP_UPDATE;
            continue;
        }

        // otaCleanup() frees the URL using this tag
        target->_url = (char *)rtkMalloc(strlen(otaLocalUrl[subsystem]) + 1, "Target URL");
        if (target->_url == nullptr)
        {
            otaCleanup(false);
            otaLocalSetState(OTA_LOCAL_FAILED, "Out of memory");
            return;
        }
        strcpy(target->_url, otaLocalUrl[subsystem]);
        target->_fileBytes = otaLocalFileBytes[subsystem];
        target->_crc = otaLocalCrc[subsystem];
        target->_requestType = OTA_REQUEST_ALWAYS_UPDATE; // Any version the app sends
        target->_valid = true;
    }
    otaUpdatesFound = queued;

    otaLocalSubsystem = "";
    otaLocalFailedSubsystem = nullptr;
    otaLocalPercent = 0;
    otaLocalSetState(OTA_LOCAL_UPDATING, nullptr);

    // Skip the network wait and the GitHub version check: go straight to the downloads
    otaSetState(OTA_STATE_UPDATE_FIRMWARE);
}

//----------------------------------------
// Run the local update requests and soft AP (main loop)
//----------------------------------------
void otaLocalUpdate()
{
    uint8_t state = otaLocalState;

    // Stop the soft AP on request, or when the app goes quiet
    bool idle = (state == OTA_LOCAL_AP_STARTING) || (state == OTA_LOCAL_AP_READY) || (state == OTA_LOCAL_FAILED);
    bool timedOut = idle && ((millis() - otaLocalActivityMsec) > OTA_LOCAL_IDLE_TIMEOUT_MSEC);
    if ((otaLocalRequestCancel && idle) || timedOut)
    {
        otaLocalRequestAp = false;
        otaLocalRequestStart = false;
        otaLocalRequestCancel = false;
        otaLocalApStop();
        otaLocalSetState(OTA_LOCAL_IDLE, timedOut ? "Timed out" : "Canceled");
        return;
    }
    otaLocalRequestCancel = false;

    switch (state)
    {
    case OTA_LOCAL_AP_STARTING:
        if (otaLocalRequestAp)
        {
            otaLocalRequestAp = false;
            otaLocalStateMsec = millis();
            otaLocalApStart();
        }
        else if (wifiSoftApOnline)
            otaLocalSetState(OTA_LOCAL_AP_READY, nullptr);
        else if ((millis() - otaLocalStateMsec) > OTA_LOCAL_AP_START_TIMEOUT_MSEC)
        {
            otaLocalApStop();
            otaLocalSetState(OTA_LOCAL_IDLE, "Soft AP failed to start");
        }
        break;

    case OTA_LOCAL_AP_READY:
    case OTA_LOCAL_FAILED:
        if (otaLocalRequestStart)
        {
            otaLocalRequestStart = false;
            otaLocalStartUpdate();
        }
        break;

    case OTA_LOCAL_UPDATING:
        // Success reboots from OTA_STATE_REBOOT, so the OTA state machine stopping means failure
        if (otaState == OTA_STATE_OFF)
        {
            char message[sizeof(otaLocalMessage)];
            snprintf(message, sizeof(message), "%s", otaLocalMessage[0] ? otaLocalMessage : "No firmware was updated");
            if (otaLocalFailedSubsystem)
                otaLocalSubsystem = otaLocalFailedSubsystem;
            otaLocalActivityMsec = millis(); // Give the app the full timeout to retry or cancel
            otaLocalSetState(OTA_LOCAL_FAILED, message);
        }
        break;

    default:
        break;
    }
}

//----------------------------------------
// OTA.ino hooks
//----------------------------------------

// True while OTA.ino is performing a local update
bool otaLocalUpdateRunning()
{
    return (otaLocalState == OTA_LOCAL_UPDATING);
}

// OTA.ino is starting to update this subsystem
void otaLocalSubsystemStart(const char *subsystem)
{
    if (otaLocalUpdateRunning() == false)
        return;
    otaLocalSubsystem = subsystem;
    otaLocalPercent = 0;
}

// The update of this subsystem failed. message: text without commas.
void otaLocalSubsystemFailed(const char *subsystem, const char *message)
{
    if ((otaLocalUpdateRunning() == false) || otaLocalFailedSubsystem)
        return; // Report the first failure
    otaLocalFailedSubsystem = subsystem;
    snprintf(otaLocalMessage, sizeof(otaLocalMessage), "%s", message);
}

// Called from firmwareUpdateProgressCallback() (System.ino)
void otaLocalProgress(int percent)
{
    otaLocalPercent = percent;
}

// All updates succeeded and the device is about to reboot
void otaLocalComplete()
{
    if (otaLocalUpdateRunning() == false)
        return;
    otaLocalPercent = 100;
    otaLocalSetState(OTA_LOCAL_COMPLETE, nullptr);
    delay(2000); // The Bluetooth task keeps answering updateStatus polls
}

//----------------------------------------
// CLI (Bluetooth task or serial command mode)
//----------------------------------------

// Build a unique SSID and a random WPA2 password for this session
void otaLocalCreateCredentials()
{
    snprintf(otaLocalApSsid, sizeof(otaLocalApSsid), "RTK Update %s-%04X", serialNumber,
             (unsigned int)(esp_random() & 0xffff));

    const int characters = sizeof(otaLocalPasswordCharacters) - 1;
    for (int index = 0; index < (int)sizeof(otaLocalApPassword) - 1; index++)
        otaLocalApPassword[index] = otaLocalPasswordCharacters[esp_random() % characters];
    otaLocalApPassword[sizeof(otaLocalApPassword) - 1] = 0;
}

// Parse a whole decimal or 0x-prefixed hex number. Returns false if there is anything else.
bool otaLocalParseNumber(const char *string, uint32_t &value)
{
    char *end;
    value = strtoul(string, &end, 0);
    return (end != string) && (*end == '\0');
}

// Queue a file: <subsystem>,<chip>,<url>,<bytes>,<crc32>. Returns an error message or nullptr.
const char *otaLocalQueueFile(char **args)
{
    static char error[48];
    int subsystem;
    uint32_t fileBytes;
    uint32_t crc;

    for (subsystem = 0; subsystem < OTA_SUBSYSTEM_MAX; subsystem++)
        if (strcmp(args[0], otaSubsystem[subsystem]) == 0)
            break;
    if (subsystem >= OTA_SUBSYSTEM_MAX)
        return "Unknown subsystem";

    // Guard against sending one chip's firmware to another chip
    const OTA_SUBSYSTEM_INFO *subsystemInfo = otaGetSubsystemInfo(subsystem);
    if (subsystemInfo == nullptr)
        return "Subsystem not present";
    const char *chip = otaGetChipNameFromChipId(subsystemInfo->_chip);
    if (strcmp(args[1], chip) != 0)
    {
        snprintf(error, sizeof(error), "Device has %s", chip);
        return error;
    }
    if ((subsystemInfo->_firmwareUpdate == nullptr) && (subsystemInfo->_streamFirmware == nullptr))
    {
        snprintf(error, sizeof(error), "Updates not supported for %s", chip);
        return error;
    }

    if ((strncmp(args[2], "http://", 7) != 0) || (strlen(args[2]) >= OTA_LOCAL_URL_LENGTH))
        return "Bad URL";
    if ((otaLocalParseNumber(args[3], fileBytes) == false) || (fileBytes == 0))
        return "Bad file size";
    if (otaLocalParseNumber(args[4], crc) == false)
        return "Bad CRC";

    strcpy(otaLocalUrl[subsystem], args[2]);
    otaLocalFileBytes[subsystem] = fileBytes;
    otaLocalCrc[subsystem] = crc;
    otaLocalQueued |= otaGetSubsystemMaskFromSubsystem(subsystem);
    return nullptr;
}

// Handle the local update SPEXE actions. Returns false if tokens[1] is not one of them.
bool otaLocalExecute(char **tokens, int tokenCount, t_cliResult &result)
{
    const char *command = tokens[0];
    const char *action = tokens[1];
    const char *error = nullptr;
    uint8_t state = otaLocalState;
    bool updating = (state == OTA_LOCAL_UPDATING) || (state == OTA_LOCAL_COMPLETE);
    bool apUp = (state == OTA_LOCAL_AP_READY) || (state == OTA_LOCAL_FAILED);
    int argumentsNeeded;

    if (strcmp(action, "UPDATEFILE") == 0)
        argumentsNeeded = 7;
    else if ((strcmp(action, "UPDATEAP") == 0) || (strcmp(action, "UPDATESTART") == 0) ||
             (strcmp(action, "UPDATECANCEL") == 0))
        argumentsNeeded = 2;
    else
        return false;

    otaLocalActivityMsec = millis();
    result = CLI_OK;

    if (tokenCount != argumentsNeeded)
        error = "Incorrect number of arguments";

    else if (strcmp(action, "UPDATEAP") == 0)
    {
        if (updating)
            error = "Update in progress";
        else if (state == OTA_LOCAL_IDLE)
        {
#ifdef COMPILE_WIFI
            // An open soft AP (Web Config, TCP or UDP server, Base Caster) would keep its settings
            if (wifiSoftApRunning)
                error = "Soft AP in use";
            else if (otaState != OTA_STATE_OFF)
                error = "Firmware update in progress";
            else
            {
                otaLocalCreateCredentials();
                otaLocalQueued = 0;
                otaLocalRequestAp = true;
                otaLocalSetState(OTA_LOCAL_AP_STARTING, nullptr);
            }
#else  // COMPILE_WIFI
            error = "WiFi not supported";
#endif // COMPILE_WIFI
        }

        // Also answers a repeated UPDATEAP with the current credentials
        if (error == nullptr)
        {
            char response[120];
            snprintf(response, sizeof(response), "%s,%s,\"%s\",\"%s\",OK", command, action, otaLocalApSsid,
                     otaLocalApPassword);
            commandSendResponse(response);
            forceMenuExit = true; // Leave serial command mode so the main loop starts the soft AP
            result = CLI_EXIT;
            return true;
        }
    }

    else if (strcmp(action, "UPDATEFILE") == 0)
    {
        if (updating)
            error = "Update in progress";
        else if (apUp == false)
            error = "Soft AP not ready";
        else
            error = otaLocalQueueFile(&tokens[2]);
    }

    else if (strcmp(action, "UPDATESTART") == 0)
    {
        if (updating)
            error = "Update in progress";
        else if (apUp == false)
            error = "Soft AP not ready";
        else if (otaLocalQueued == 0)
            error = "No files queued";
        else if (wifiSoftApConnected == false)
            error = "Phone not connected to the soft AP";
        else
        {
            otaLocalRequestStart = true;
            forceMenuExit = true; // Leave serial command mode so the main loop runs the update
            result = CLI_EXIT;
        }
    }

    else // UPDATECANCEL
    {
        if (updating)
            error = "Update in progress";
        else if (state != OTA_LOCAL_IDLE)
        {
            otaLocalRequestCancel = true;
            forceMenuExit = true; // Leave serial command mode so the main loop stops the soft AP
            result = CLI_EXIT;
        }
    }

    if (error)
    {
        commandSendExecuteErrorResponse(command, action, error);
        result = CLI_ERROR;
    }
    else
        commandSendExecuteOkResponse(command, action);
    return true;
}

// Chip and version of each subsystem: "ESP32:ESP32:3.1.0.0;GNSS:LG290P:2.1.0.0;..." A debug
// build adds "-rc", and "unknown" means the chip did not report a version.
void otaLocalGetVersions(char *buffer, size_t bufferLength)
{
    size_t length = 0;

    buffer[0] = 0;
    for (int subsystem = 0; subsystem < OTA_SUBSYSTEM_MAX; subsystem++)
    {
        const OTA_SUBSYSTEM_INFO *subsystemInfo = otaGetSubsystemInfo(subsystem);
        if (subsystemInfo == nullptr)
            continue;

        char version[24] = "unknown";
        int major, minor, patch, revision, releaseCandidate;
        if (subsystemInfo->_getVersion &&
            subsystemInfo->_getVersion(major, minor, patch, revision, releaseCandidate))
            snprintf(version, sizeof(version), "%d.%d.%d.%d%s", major, minor, patch, revision,
                     releaseCandidate ? "-rc" : "");

        length += snprintf(&buffer[length], bufferLength - length, "%s%s:%s:%s", length ? ";" : "",
                           otaSubsystem[subsystem], otaGetChipNameFromChipId(subsystemInfo->_chip), version);
        if (length >= bufferLength)
            break; // Truncated
    }
}

// Handle the local update SPGET fields. Returns false if field is not one of them.
bool otaLocalGet(const char *command, const char *field)
{
    char value[140];

    if (strcmp(field, "updateStatus") == 0)
    {
        if (otaLocalState != OTA_LOCAL_IDLE)
            otaLocalActivityMsec = millis(); // The app is still there
        snprintf(value, sizeof(value), "%s,%s,%d,%s", otaLocalStateNames[otaLocalState], otaLocalSubsystem,
                 otaLocalPercent, otaLocalMessage);
    }
    else if (strcmp(field, "subsystemVersions") == 0)
        otaLocalGetVersions(value, sizeof(value));
    else
        return false;

    // Quoted like a string setting (the values contain commas), without escapes: no quotes inside
    char response[200];
    snprintf(response, sizeof(response), "%s,%s,\"%s\"", command, field, value);
    commandSendResponse(response);
    return true;
}

#endif // COMPILE_FIRMWARE_UPDATE
