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
   LORA ----------------------------------------->|               |
                                                  |               |
   USB ------------------------------------------>|               |
                                                                  |
   Serial (Radio Ext) --------------------------------------------'

  The corrections interface is using:

       * available
       * read
       * write

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
    // Periodically check if data is arriving on the Radio Ext port
    // If needed, fake the arrival of data on the Radio Ext port
    // The code is the same:
    //   On ZED / mosaic, we can detect if the port is active.
    //   On LG290P, we fake the arrival of data if needed.
    static uint32_t lastRadioExtCheck = millis();
    uint32_t radioCheckIntervalMsec = settings.correctionsSourcesLifetime_s * 500; // Check twice per lifetime
    bool setCorrRadioPort = false;

    if ((millis() - lastRadioExtCheck) > radioCheckIntervalMsec)
    {
        // LG290P will return settings.enableExtCorrRadio.
        // ZED / mosaic will return true if settings.enableExtCorrRadio is
        // true and the port is actually active.
        if (gnss->isCorrRadioExtPortActive())
            correctionLastSeen(CORR_RADIO_EXT);

        lastRadioExtCheck = millis();
        setCorrRadioPort = true; // Update the port protocols after updating the sources
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
    // If radioCheckIntervalMsec expired
    if (setCorrRadioPort)
    {
        // Update the input protocols, based on CORR_RADIO_EXT being the active correction source
        gnss->setCorrRadioExtPort(correctionGetSource() == CORR_RADIO_EXT, false); // Don't force
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