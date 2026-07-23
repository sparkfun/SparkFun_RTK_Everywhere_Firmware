/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
menuCorrectionsPriorities.ino

  Manage the correction sources for the GNSS.  The menu is used to
  set the priority order of the possible correction sources.

  As new sources become active (receive correction data), a new
  correction source is enabled when the new source priority is higher
  than the current source.  See correctionLastSeen and CorrectionSetSourceId.

  As the time since last received exceeds settings.correctionsSourcesLifetime_s
  seconds the correction source transitions from active to inactive.
  If this source was supplying corrections to the GNSS, correctionIsSourceActive
  searches for the highest priority active correction source.

  The correction stack looks like:

   Network

   Ethernet ---->|           NetworkClient   Corrections
                 |
   WiFi -------->|                  |--> MQTT --->|
                 +--> IP --> TCP -->+             +--> UART --> GNSS
   PPP (LARA) -->|                  |--> NTRIP -->|               ^
                                                  |               |
   Bluetooth ------------------------------------>|               |
                                                  |               |
   ESPNOW --------------------------------------->|               |
                                                  |               |
   PPP B2b E6------------------------------------>|               |
                                                  |               |
   LBAND ---------------------------------------->|               |
                                                  |               |
   LORA on Torch -------------------------------->|               |
                                                  |               |
   USB ------------------------------------------>|               |
                                                                  |
   Serial (Radio Ext) ------------------------------------------->|
                                                                  |
   LoRa on Facet FP (via SW4) ------------------------------------'

    The corrections interface is using:

       * available
       * read
       * write

    On Torch, the LoRa RTCM traffic is routed through the ESP32 so it is easy
    to monitor is data is being received

    On Facet FP, LoRa and Radio Ext are almost the same thing:
    GNSS UART2 is connected to SW4
    SW4 switches between LoRa (UART1) and the 4-pin JST Radio connector
    LoRa baud is 115200
    Radio settings.radioPortBaud defaults to 57600
    On GNSS that support it, we can monitor RTCM data via the UART2 byte counts (MON_COMMS etc.)
    To support CORR_RADIO_LORA and CORR_RADIO_EXT properly and separately is difficult
    For now, it is easier to treat LoRa as if it is Radio Ext
    loraSetupTransmit() and loraSetupReceive() override the GNSS UART2 baud rate and flip SW4
    TODO: improve this...

=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

//----------------------------------------
// Locals
//----------------------------------------
static CORRECTION_ID_T correctionSourceId = CORR_NUM; // ID of correction source (default: None)
static CORRECTION_MASK_T correctionActive;            // Bitmap of active correction sources
static uint32_t correctionLastSeenMsec[CORR_NUM];     // Time when correction was last received

//----------------------------------------
// Support routines
//----------------------------------------

//----------------------------------------
// Set the new source to provide the corrections to the GNSS
// Inputs:
//    id: correctionsSource value, ID of the correction source
//----------------------------------------
void correctionSetSourceId(CORRECTION_ID_T id)
{
    // When the source priority changes, is new prioity the highest
    if ((correctionSourceId >= CORR_NUM) || ((correctionActive & (1 << correctionSourceId)) == 0) ||
        (settings.correctionsSourcesPriority[id] < settings.correctionsSourcesPriority[correctionSourceId]))
    {
        // Display the correction source transition
        if (settings.debugCorrections)
        {
            if ((correctionSourceId <= CORR_NUM) && (correctionActive & (1 << correctionSourceId)))
                systemPrintf("Correction Source: %s --> %s\r\n", correctionsSourceNames[correctionSourceId],
                             correctionsSourceNames[id]);
            else
                systemPrintf("Correction Source: None --> %s\r\n", correctionsSourceNames[id]);
        }

        // Set the new correction source
        correctionSourceId = id;
    }
}

//----------------------------------------
// Determine which correction source should be providing corrections
// Inputs:
//    id: correctionsSource value, ID of the correction source
//    priority: Priority of the correction source
//----------------------------------------
void correctionPriorityUpdateSource(CORRECTION_ID_T id, CORRECTION_ID_T priority)
{
    CORRECTION_MASK_T bitMask;

    // Validate the id value
    if (id >= CORR_NUM)
    {
        systemPrintf("ERROR: correctionPriorityUpdateSource invalid correction id value %d, valid range (0 - %d)!\r\n",
                     id, CORR_NUM - 1);
        return;
    }

    // Validate the id value
    if (priority >= CORR_NUM)
    {
        systemPrintf("ERROR: correctionPriorityUpdateSource invalid priority value %d, valid range (0 - %d)!\r\n", id,
                     CORR_NUM - 1);
        return;
    }

    // Determine if this source is still active
    if (correctionIsSourceActive(id))
    {
        // Update the active source
        correctionSetSourceId(id);
    }
}

//----------------------------------------
// Decrease the correction priority
// Inputs:
//    oldPriority: Priority value of the correctionsSource
//----------------------------------------
void correctionPriorityDecrease(CORRECTION_ID_T oldPriority)
{
    CORRECTION_ID_T id;
    int index;
    CORRECTION_ID_T newPriority;

    // Validate the priority value
    if (oldPriority >= CORR_NUM)
    {
        systemPrintf("ERROR: correctionPriorityDecrease invalid correction id value %d, valid range (0 - %d)!\r\n", id,
                     CORR_NUM - 1);
        return;
    }

    // Get the priorities
    for (index = 0; index < CORR_NUM; index++)
        if (settings.correctionsSourcesPriority[index] == oldPriority)
        {
            id = index;
            break;
        }
    newPriority = oldPriority + 1;

    // Determine if current entry is at the lowest priority
    if ((oldPriority + 1) >= CORR_NUM)
    {
        systemPrint(7);
        return;
    }

    //               Decrease c
    //           Before      After
    //          +-----+     +-----+
    //          |  a  |  0  |  a  |
    //          +-----+     +-----+
    //          |  b  |  1  |  b  |
    //          +-----+     +-----+
    //          |  c  |  2  |  d  |  <---.
    //          +-----+     +-----+      | Switch
    //          |  d  |  3  |  c  |  <---'
    //          +-----+     +-----+
    //          |  e  |  4  |  e  |
    //          +-----+     +-----+
    //          |  f  |  5  |  f  |
    //          +-----+     +-----+
    //          |  g  |  6  |  g  |
    //          +-----+     +-----+
    //
    // Switch the priority values
    for (index = 0; index < CORR_NUM; index++)
        if (settings.correctionsSourcesPriority[index] == newPriority)
        {
            if (settings.debugCorrections)
                systemPrintf("%s: %d --> %d\r\n", correctionsSourceNames[index],
                             settings.correctionsSourcesPriority[index], oldPriority);
            settings.correctionsSourcesPriority[index] = oldPriority;
        }
    if (settings.debugCorrections)
        systemPrintf("%s: %d --> %d\r\n", correctionsSourceNames[id], settings.correctionsSourcesPriority[id],
                     newPriority);
    settings.correctionsSourcesPriority[id] = newPriority;

    // Update the active source
    correctionPriorityUpdateSource(index, oldPriority);
}

//----------------------------------------
// Increase the correction priority
// Inputs:
//    id: correctionsSource value, ID of the correction source
//----------------------------------------
void correctionPriorityIncrease(CORRECTION_ID_T oldPriority)
{
    CORRECTION_ID_T id;
    int index;
    CORRECTION_ID_T newPriority;

    // Validate the id value
    if (oldPriority >= CORR_NUM)
    {
        systemPrintf(
            "ERROR: correctionPriorityIncrease invalid correction priority value %d, valid range (0 - %d)!\r\n", id,
            CORR_NUM - 1);
        return;
    }

    // Get the priorities
    for (index = 0; index < CORR_NUM; index++)
        if (settings.correctionsSourcesPriority[index] == oldPriority)
        {
            id = index;
            break;
        }
    newPriority = oldPriority - 1;

    // Determine if at the highest priority
    if (oldPriority == 0)
    {
        systemPrint(7);
        return;
    }

    //               Increase e
    //           Before      After
    //          +-----+     +-----+
    //          |  a  |  0  |  a  |
    //          +-----+     +-----+
    //          |  b  |  1  |  b  |
    //          +-----+     +-----+
    //          |  c  |  2  |  c  |
    //          +-----+     +-----+
    //          |  d  |  3  |  e  | <---.
    //          +-----+     +-----+     | Switch
    //          |  e  |  4  |  d  | <---'
    //          +-----+     +-----+
    //          |  f  |  5  |  f  |
    //          +-----+     +-----+
    //          |  g  |  6  |  g  |
    //          +-----+     +-----+
    //
    // Switch the priority values
    for (index = 0; index < CORR_NUM; index++)
        if (settings.correctionsSourcesPriority[index] == newPriority)
        {
            if (settings.debugCorrections)
                systemPrintf("%s: %d --> %d\r\n", correctionsSourceNames[index],
                             settings.correctionsSourcesPriority[index], oldPriority);
            settings.correctionsSourcesPriority[index] = oldPriority;
        }
    if (settings.debugCorrections)
        systemPrintf("%s: %d --> %d\r\n", correctionsSourceNames[id], settings.correctionsSourcesPriority[id],
                     newPriority);
    settings.correctionsSourcesPriority[id] = newPriority;

    // Update the active source
    correctionPriorityUpdateSource(id, newPriority);
}

//----------------------------------------
// Correction API
//----------------------------------------

#ifdef  COMPILE_MENU_CORRECTIONS

//----------------------------------------
// Set the priority of all correction sources
// Note: this sets the priority of all possible sources, not just the ones available / in use
//----------------------------------------
void menuCorrectionsPriorities()
{
    if (!correctionPriorityValidation())
    {
        systemPrintln();
        systemPrintln("Corrections priorities are invalid. Restoring the defaults");
    }

    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: Corrections Priorities");

        systemPrint("1) Correction source lifetime in seconds: ");
        systemPrintln(settings.correctionsSourcesLifetime_s);

        systemPrintln();
        systemPrintln("These are the correction sources in order of decreasing priority");
        systemPrintln("Enter the uppercase letter to increase the correction priority");
        systemPrintln("Enter the lowercase letter to decrease the correction priority");
        systemPrintln();

        correctionDisplayPriorityTable(true);

        systemPrintln();
        systemPrintln("x) Exit");

        byte incoming = getUserInputCharacterNumber();

        if (incoming == 1)
            getNewSetting("Enter new correction source lifetime in seconds (5-120): ", 5, 120,
                          &settings.correctionsSourcesLifetime_s);

        // Check for priority decrease
        else if ((incoming >= 'a') && (incoming < ('a' + CORR_NUM)))
            correctionPriorityDecrease(settings.correctionsSourcesPriority[incoming - 'a']);

        // Check for priority increase
        else if ((incoming >= 'A') && (incoming < ('A' + CORR_NUM)))
            correctionPriorityIncrease(settings.correctionsSourcesPriority[incoming - 'A']);

        else if (incoming == 'x')
            break;
        else if (incoming == INPUT_RESPONSE_GETCHARACTERNUMBER_EMPTY)
            break;
        else if (incoming == INPUT_RESPONSE_GETCHARACTERNUMBER_TIMEOUT)
            break;
        else
            printUnknown(incoming);
    }

    clearBuffer(); // Empty buffer of any newline chars
}

#endif  // COMPILE_MENU_CORRECTIONS

//----------------------------------------
// Display the correction priority table
// Inputs:
//    menu: True if displaying in menuCorrections, false otherwise
//----------------------------------------
void correctionDisplayPriorityTable(bool menu)
{
    //                         "a / A) "
    const char *blankString = "       ";
    uint32_t currentMsec;
    CORRECTION_ID_T id;
    char menuString[strlen(blankString) + 1];
    CORRECTION_ID_T priority;
    uint32_t seconds;
    uint32_t milliseconds;
    CORRECTION_ID_T priorityToId[CORR_NUM];

    // Sort the priority table
    for (id = 0; id < CORR_NUM; id++)
        priorityToId[settings.correctionsSourcesPriority[id]] = id;

    // Display the table header
    menuString[0] = 0;
    if (menu)
        strcpy(menuString, blankString);
    systemPrintf("%sPriority   Status     Last Seen     Source\r\n", menuString);
    systemPrintf("%s--------  --------  -------------   ------\r\n", menuString);

    // Walk the priority table in decending priority order
    currentMsec = millis();
    for (priority = 0; priority < CORR_NUM; priority++)
    {
        // Get the source at this priority
        id = priorityToId[priority];

        // Add the menu entry selection
        if (menu)
        {
            menuString[0] = 'A' + id;
            menuString[2] = '/';
            menuString[4] = 'a' + id;
            menuString[5] = ')';
        }

        // Compute the last seen time
        milliseconds = currentMsec - correctionLastSeenMsec[id];
        seconds = milliseconds / MILLISECONDS_IN_A_SECOND;
        milliseconds -= seconds * MILLISECONDS_IN_A_SECOND;

        // Display the priority table
        if ((id < CORR_NUM) && correctionIsSourceActive(id))
            systemPrintf("%s %c %2d     active     %4d.%03d Sec   %s\r\n", menuString,
                         (correctionSourceId == id) ? '*' : ' ', settings.correctionsSourcesPriority[id], seconds,
                         milliseconds, correctionsSourceNames[id]);
        else if (id < CORR_NUM)
            systemPrintf("%s   %2d     inactive                  %s\r\n", menuString,
                         settings.correctionsSourcesPriority[id], correctionsSourceNames[id]);
        else
            systemPrintf("%s   %2d     inactive                  %s (%d)\r\n", menu ? blankString : "", -1, "Unknown",
                         id);
    }
}

//----------------------------------------
// Get the name of a correction source
// Inputs:
//    id: correctionsSource value, ID of the correction source
// Outputs:
//    Returns the address of a zero terminated constant name string or
//    nullptr when id is invalid
//----------------------------------------
const char *correctionGetName(CORRECTION_ID_T id)
{
    if (id == CORR_NUM)
        return "None";

    // Validate the id value
    if (id > CORR_NUM)
    {
        systemPrintf("ERROR: correctionGetName invalid correction id value %d, valid range (0 - %d)!\r\n", id,
                     CORR_NUM - 1);
        return nullptr;
    }

    // Return the name of the correction source
    return correctionsSourceNames[id];
}

//----------------------------------------
// Get the priority of a correction source
// Inputs:
//    id: correctionsSource value, ID of the correction source
// Outputs:
//    Returns the priority of the source or
//    CORR_NUM when id is invalid
//----------------------------------------
CORRECTION_ID_T correctionGetPriority(CORRECTION_ID_T id)
{
    // Validate the id value
    if (id >= CORR_NUM)
    {
        systemPrintf("ERROR: correctionGetPriority invalid correction id value %d, valid range (0 - %d)!\r\n", id,
                     CORR_NUM - 1);
        return CORR_NUM;
    }

    // Return the priority of the correction source
    return settings.correctionsSourcesPriority[id];
}

//----------------------------------------
// Get the ID of the source providing corrections
// Outputs:
//    Returns the correctionsSource ID providing corrections
//----------------------------------------
CORRECTION_ID_T correctionGetSource()
{
    return correctionSourceId;
}

//----------------------------------------
// Get the name of the source providing corrections
// Outputs:
//    Returns the correctionsSource ID providing corrections
//----------------------------------------
const char *correctionGetSourceName()
{
    const char *name;

    name = correctionGetName(correctionSourceId);
    if (!name)
        name = "None";
    return name;
}

//----------------------------------------
// Determine if the correction source is active
// Inputs:
//    id: correctionsSource value, ID of the correction source
// Outputs:
//    Returns true if the corrections source is active
//----------------------------------------
bool correctionIsSourceActive(CORRECTION_ID_T id)
{
    CORRECTION_MASK_T bitMask;
    uint32_t currentMsec;
    int index;
    CORRECTION_ID_T newPriority;
    CORRECTION_ID_T newSource;
    uint32_t timeoutMsec;

    // Validate the id value
    if (id >= CORR_NUM)
    {
        systemPrintf("ERROR: correctionIsSourceActive invalid correction id value %d, valid range (0 - %d)!\r\n", id,
                     CORR_NUM - 1);
        return false;
    }

    currentMsec = millis();
    timeoutMsec = settings.correctionsSourcesLifetime_s * 1000;

    // Determine if corrections were received recently
    bitMask = 1 << id;
    if ((currentMsec - correctionLastSeenMsec[id]) >= timeoutMsec)
    {
        // Corrections source is actually inactive
        correctionActive &= ~bitMask;

        // Update last seen time to support 32-bit roll over of millis()
        correctionLastSeenMsec[id] = currentMsec - timeoutMsec;

        // Update the active source
        if (correctionSourceId == id)
        {
            // Search the active sources for the highest priority
            newPriority = CORR_NUM;
            newSource = CORR_NUM;
            for (index = 0; index < CORR_NUM; index++)
                if ((correctionActive & (1 << index)) && (settings.correctionsSourcesPriority[index] < newPriority))
                {
                    newPriority = settings.correctionsSourcesPriority[index];
                    newSource = index;
                }

            // Update the correction source
            correctionSourceId = newSource;

            // Display the correction transition
            if (settings.debugCorrections)
            {
                if (newSource < CORR_NUM)
                    systemPrintf("Correction Source: %s --> %s\r\n", correctionsSourceNames[id],
                                 correctionsSourceNames[newSource]);
                else
                    systemPrintf("Correction Source: %s --> None\r\n", correctionsSourceNames[id]);
            }
        }
    }

    // Determine if the correction source is active
    return (correctionActive & bitMask);
}

//----------------------------------------
// Update the time when the correction was last seen
// Inputs:
//    id: correctionsSource value, ID of the correction source
// Outputs:
//    Returns true if this source providing corrections and false otherwise
//----------------------------------------
bool correctionLastSeen(CORRECTION_ID_T id)
{
    uint32_t currentMsec;
    CORRECTION_MASK_T bitMask;

    // Validate the id value
    if (id >= CORR_NUM)
    {
        systemPrintf("ERROR: correctionLastSeen invalid correction id value %d, valid range (0 - %d)!\r\n", id,
                     CORR_NUM - 1);
        return false;
    }

    // Remember the time of this data
    currentMsec = millis();
    correctionLastSeenMsec[id] = currentMsec;

    // Determine if this source was idle before
    bitMask = 1 << id;
    if ((correctionActive & bitMask) == 0)
    {
        // Mark this source as active
        correctionActive |= bitMask;

        // Determine if this should be the correction source
        correctionSetSourceId(id);
    }

    // Determine if this source is currently providing corrections
    return (correctionSourceId == id);
}

//----------------------------------------
// Validate the correction priority table
// Output:
//   Returns true if the table was valid, false if table was initialized
//----------------------------------------
bool correctionPriorityValidation()
{
    CORRECTION_MASK_T bitMask;
    bool fixPriorityList;
    int id;
    CORRECTION_MASK_T priorityMask;
    CORRECTION_MASK_T validMask;

    // Walk the list of correction priorites
    fixPriorityList = false;
    for (id = 0; id < CORR_NUM; id++)
    {
        // Validate the priority number
        if (settings.correctionsSourcesPriority[id] >= CORR_NUM)
            // Invalid priority number
            break;

        // Update the priority values seen
        bitMask = 1 << id;
        validMask |= bitMask;
        priorityMask |= 1 << settings.correctionsSourcesPriority[id];
    }

    // Determine if the priority table is valid
    if ((id < CORR_NUM) || (priorityMask != validMask))
    {
        // Invalid priority table, initialize it to the default priorities
        for (id = 0; id < CORR_NUM; id++)
            settings.correctionsSourcesPriority[id] = id;

        // Tell caller the table was invalid
        return false;
    }

    // Tell the caller the table was valid
    return true;
}

//----------------------------------------
// Determine which correction source should be providing corrections
//----------------------------------------
void correctionUpdateSource()
{
    // Periodically check if data is arriving on the external corrections port
    // If needed, fake the arrival of data on the corrections port
    // The code is the same:
    //   On ZED / mosaic, we can detect if the port is active.
    //   On LG290P, we fake the arrival of data if needed.
    //   On Facet FPL, we fake the arrival of correction data
    //     again to prevent a timeout and maintain the port protocol
    static uint32_t lastExternalCorrectionsCheck = millis(); // We can wait...
    // settings.correctionsSourcesLifetime_s is in the range 5-120
    // To keep life simple and improve the user experience, check every 2 seconds
    uint32_t correctionsCheckIntervalMsec = 2000;
    bool correctionsMayNeedUpdating = false;

    if ((millis() - lastExternalCorrectionsCheck) > correctionsCheckIntervalMsec)
    {
        // What needs to happen here?
        // We want to find out if external radio or LoRa corrections are active
        // If corrections are active, update correctionLastSeen with CORR_RADIO_LORA or
        // CORR_RADIO_EXT as appropriate
        // Call GNSS::isExternalCorrectionActive every correctionsCheckIntervalMsec
        // LG290P will return true if corrections are enabled
        // ZED / mosaic will return true if corrections are enabled and the port is actually active.
        // The GNSS does not know the source of the corrections
        // gnssExternalCorrectionsSelected returns true if corrections have been selected
        // and the type via the lora reference
        if (gnss->isExternalCorrectionActive(getGnssExternalCorrectionsPort()))
        {
            // If the corrections are active, then update correctionLastSeen with the source
            // If corrections are not active, don't update - allowing the source to timeout
            bool lora;
            if(gnssExternalCorrectionsSelected(lora))
                correctionLastSeen(lora ? CORR_RADIO_LORA : CORR_RADIO_EXT);
        }

        lastExternalCorrectionsCheck = millis();
        correctionsMayNeedUpdating = true; // Update the port protocols after updating the sources
    }

    // Now update the sources
    CORRECTION_ID_T id;

    for (id = 0; id < CORR_NUM; id++)
        correctionPriorityUpdateSource(id, settings.correctionsSourcesPriority[id]);

    // Display the current correction source
    if (PERIODIC_DISPLAY(PD_CORRECTION_SOURCE) && !inMainMenu)
    {
        PERIODIC_CLEAR(PD_CORRECTION_SOURCE);
        systemPrintf("Correction Source: %s\r\n", correctionGetSourceName());

        // systemPrintf("%s\r\n", PERIODIC_SETTING(PD_RING_BUFFER_MILLIS) ? "Active" : "Inactive");
    }

    // Now that the sources have been updated
    // If correctionsCheckIntervalMsec expired
    if (correctionsMayNeedUpdating)
    {
        // On Facet FP, SW4 to connects LoRa or External Radio to the GNSS UART2
        // But, setting SW4 is looked after by the loraState state machine
        // Here we are only concerned about the port protocol:
        // disable RTCM if CORR_RADIO_EXT / CORR_RADIO_LORA is not the highest priority;
        // ensure RTCM is enabled if the priority of CORR_RADIO_EXT / CORR_RADIO_LORA is
        // higher than that of the current source.
    
        bool lora;
        if(gnssExternalCorrectionsSelected(lora))
        {
            // Update the input protocols, based on the active correction source
            // *** setExternalCorrections will only communicate with the GNSS if things have changed ***

            // If no correction source is active, ensure the protocol is enabled
            if (correctionGetSource() >= CORR_NUM)
            {
                gnss->setExternalCorrections(getGnssExternalCorrectionsPort(), true,
                    false, "correctionUpdateSource no active source"); // Don't force
            }
            // Else if the priority of LoRa is higher than the priority of the active correction source
            // then ensure the protocol is enabled
            // Remember that 0 is the highest priority
            // Use <= because the current source could be LoRa
            else if (lora && (correctionGetPriority(CORR_RADIO_LORA) <= correctionGetPriority(correctionGetSource())))
            {
                gnss->setExternalCorrections(getGnssExternalCorrectionsPort(), true,
                    false, "correctionUpdateSource lora priority"); // Don't force
            }
            // Else if the priority of External Radio is higher than the priority of the active correction source
            // then ensure the protocol is enabled
            // Remember that 0 is the highest priority
            // Use <= because the current source could be External Radio
            else if (!lora && (correctionGetPriority(CORR_RADIO_EXT) <= correctionGetPriority(correctionGetSource())))
            {
                gnss->setExternalCorrections(getGnssExternalCorrectionsPort(), true,
                    false, "correctionUpdateSource radio ext priority"); // Don't force
            }
            // Else disable the protocol to disable the corrections
            else
            {
                gnss->setExternalCorrections(getGnssExternalCorrectionsPort(), false,
                    false, "correctionUpdateSource no priority"); // Don't force
            }
        }
        else
        {
            // External corrections not selected. Ensure the protocol is disabled
            gnss->setExternalCorrections(getGnssExternalCorrectionsPort(), false,
                false, "correctionUpdateSource not selected"); // Don't force
        }
    }
}

//----------------------------------------
// Runtime verification of table sizes and data types
//----------------------------------------
void correctionVerifyTables()
{
    // Verify CORRECTION_ID_T is able to have a bit for each correction source
    if (CORR_NUM > (sizeof(CORRECTION_MASK_T) << 3))
        reportFatalError("Increase size of CORRECTION_MASK_T to provide a bit for each correctionsSource");

    // Verify that the tables are of equal size to prevent bad references
    if (correctionsSourceNamesEntries != CORR_NUM)
        reportFatalError("Fix correctionsSourceNamesEntries to match correctionsSource");
}

// Called when the GNSS detects a PPP signal. This is used to mark PPP as a corrections source.
void markPppCorrectionsPresent()
{
    // The GNSS is reporting that PPP is detected/converged.
    // Determine if PPP is the correction source to use
    if (correctionLastSeen(CORR_PPP_HAS_B2B))
    {
        if (settings.debugCorrections == true && !inMainMenu)
            systemPrintln("PPP Signal detected. Using corrections.");
    }
    else
    {
        if (settings.debugCorrections == true && !inMainMenu)
            systemPrintln("PPP signal detected, but it is not the top priority");
    }    
}

// Return true if external corrections (external radio or LoRa) are enabled
// Set lora true if external corrections are LoRa
// If both are enabled on Facet FP, LoRa always wins regardless of the corrections priority
// since the loraState machine will go into receive/transmit if LoRa is enabled
bool gnssExternalCorrectionsSelected(bool &lora)
{
    if ((productVariant == RTK_FACET_FP) && settings.enableLora) // On Facet FP, LoRa always wins
    {
        lora = true;
        return true;
    }

    if (settings.enableExtCorrRadio)
    {
        lora = false;
        return true;
    }

    return false;
}

// Return the baud rate for the GNSS Radio port
// Usually this is settings.radioPortBaud but on Facet FP we need to allow LoRa to override
uint32_t getBaudRateForGnssRadio()
{
    if (present.loraDedicatedUart == true)
    {
        bool lora;
        if (gnssExternalCorrectionsSelected(lora) && lora)
            return 115200; // Facet FP LoRa UART baud is fixed at 115200
    }

    return settings.radioPortBaud;
}

// Return the GNSS external corrections port (UART) for this platform
uint8_t getGnssExternalCorrectionsPort()
{
    uint8_t radioUart = 0;

    if (present.gnss_lg290p)
    {
        if (productVariant == RTK_POSTCARD)
        {
            // UART3 of the LG290P is connected to the locking JST connector labled RADIO
            radioUart = 3;
        }
        else if (productVariant == RTK_FACET_FP)
        {
            // UART2 of the GNSS is connected to SW4, which is connected to LoRa UART0
            radioUart = 2;
        }
        else if (productVariant == RTK_TORCH_X2)
        {
            // UART1 of the LG290P is connected to SW, which is connected to ESP32 UART0
            // Not really used at this time but available for configuration
            radioUart = 1;
        }
        else
            systemPrintln("getGnssExternalCorrectionsPort: Uncaught LG290P platform");
    }
    else if (present.gnss_mosaicX5)
    {
        if (productVariant == RTK_FACET_FP)
        {
            // UART2 of the GNSS is connected to SW4, which is connected to LoRa UART1
            radioUart = 2;
        }
        else if (productVariant == RTK_FACET_MOSAIC)
        {
            // COM2 of the GNSS is connected to the Radio port
            radioUart = 2;
        }
        else
            systemPrintln("getGnssExternalCorrectionsPort: Uncaught mosaicX5 platform");
    }
    else if (present.gnss_um980)
    {
        if (productVariant == RTK_TORCH)
        {
            // UART3 of the GNSS is connected to ESP32. ESP32 provides corrections
            radioUart = 3;
        }
        else
            systemPrintln("getGnssExternalCorrectionsPort: Uncaught UM980 platform");
    }
    else if (present.gnss_zedf9p || present.gnss_zedx20p)
    {
        if (productVariant == RTK_FACET_FP)
        {
            // UART2 of the GNSS is connected to SW4, which is connected to LoRa UART1
            radioUart = 2;
        }
        else if (productVariant == RTK_EVK)
        {
            // UART2 of the GNSS is accessible via the IO screw terminals
            radioUart = 2;
        }
        else
            systemPrintln("getGnssExternalCorrectionsPort: Uncaught ZED platform");
    }
    else
        systemPrintln("getGnssExternalCorrectionsPort: Uncaught GNSS");

    return radioUart;
}