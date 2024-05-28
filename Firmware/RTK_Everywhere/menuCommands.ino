void menuCommands()
{
    char cmdBuffer[200];
    char responseBuffer[200];
    char valueBuffer[100];
    const int MAX_TOKENS = 10;
    char *tokens[MAX_TOKENS];
    const char *delimiter = ",";

    systemPrintln("COMMAND MODE");
    while (1)
    {
        InputResponse response = getUserInputString(cmdBuffer, sizeof(cmdBuffer), false); // Turn off echo

        if (response != INPUT_RESPONSE_VALID)
            continue;

        if ((strcmp(cmdBuffer, "x") == 0) || (strcmp(cmdBuffer, "exit") == 0))
        {
            systemPrintln("Exiting COMMAND MODE");
            break; // Exit while(1) loop
        }

        else if (strcmp(cmdBuffer, "list") == 0)
        {
            printAvailableSettings();
            continue;
        }

        // Allow serial input to skip the validation step. Used for testing.
        else if (cmdBuffer[0] != '$')
        {
        }

        // Verify command structure
        else
        {
            if (commandValid(cmdBuffer) == false)
            {
                commandSendErrorResponse((char *)"SP", (char *)"Bad command structure or checksum");
                continue;
            }

            // Remove $
            memmove(cmdBuffer, &cmdBuffer[1], sizeof(cmdBuffer) - 1);

            // Change * to , and null terminate on the first CRC character
            cmdBuffer[strlen(cmdBuffer) - 3] = ',';
            cmdBuffer[strlen(cmdBuffer) - 2] = '\0';
        }

        const int MAX_TOKENS = 10;
        char valueBuffer[100];

        char *tokens[MAX_TOKENS];
        int tokenCount = 0;
        int originalLength = strlen(cmdBuffer);

        // We can't use strtok because there may be ',' inside of settings (ie, wifi password: "hello,world")

        tokens[tokenCount++] = &cmdBuffer[0]; // Point at first token

        bool inQuote = false;
        bool inEscape = false;
        for (int spot = 0; spot < originalLength; spot++)
        {
            if (cmdBuffer[spot] == ',' && inQuote == false)
            {
                if (spot < (originalLength - 1))
                {
                    cmdBuffer[spot++] = '\0';
                    tokens[tokenCount++] = &cmdBuffer[spot];
                }
                else
                {
                    // We are at the end of the string, just terminate the last token
                    cmdBuffer[spot] = '\0';
                }

                if (inEscape == true)
                    inEscape = false;
            }

            // Handle escaped quotes
            if (cmdBuffer[spot] == '\\' && inEscape == false)
            {
                // Ignore next character from quote checks
                inEscape = true;
            }
            else if (cmdBuffer[spot] == '\"' && inEscape == true)
                inEscape = false;
            else if (cmdBuffer[spot] == '\"' && inQuote == false && inEscape == false)
                inQuote = true;
            else if (cmdBuffer[spot] == '\"' && inQuote == true)
                inQuote = false;
        }

        if (inQuote == true)
        {
            commandSendErrorResponse((char *)"SP", (char *)"Unclosed quote");
            return;
        }

        // Trim surrounding quotes from any token
        for (int x = 0; x < tokenCount; x++)
        {
            // Remove leading "
            if (tokens[x][0] == '"')
                tokens[x] = &tokens[x][1];

            // Remove trailing "
            if (tokens[x][strlen(tokens[x]) - 1] == '"')
                tokens[x][strlen(tokens[x]) - 1] = '\0';
        }

        // Remove escape characters from within tokens
        // https://stackoverflow.com/questions/53134028/remove-all-from-a-string-in-c
        for (int x = 0; x < tokenCount; x++)
        {
            int k = 0;
            for (int i = 0; tokens[x][i] != '\0'; ++i)
                if (tokens[x][i] != '\\')
                    tokens[x][k++] = tokens[x][i];
            tokens[x][k] = '\0';
        }

        // Valid commands: CMD, GET, SET, EXE,
        if (strcmp(tokens[0], "SPCMD") == 0)
        {
            commandSendOkResponse(tokens[0]);
        }
        else if (strcmp(tokens[0], "SPGET") == 0)
        {
            if (tokenCount != 2)
                commandSendErrorResponse(tokens[0], (char *)"Incorrect number of arguments");
            else
            {
                auto field = tokens[1];

                SettingValueResponse response = getSettingValue(field, valueBuffer);

                if (response == SETTING_KNOWN)
                    commandSendValueResponse(tokens[0], field, valueBuffer); // Send structured response
                else if (response == SETTING_KNOWN_STRING)
                    commandSendStringResponse(tokens[0], field,
                                              valueBuffer); // Wrap the string setting in quotes in the response, no OK
                else
                    commandSendErrorResponse(tokens[0], (char *)"Unknown setting");
            }
        }
        else if (strcmp(tokens[0], "SPSET") == 0)
        {
            if (tokenCount != 3)
                commandSendErrorResponse(tokens[0],
                                         (char *)"Incorrect number of arguments"); // Incorrect number of arguments
            else
            {
                auto field = tokens[1];
                auto value = tokens[2];

                SettingValueResponse response = updateSettingWithValue(field, value);
                if (response == SETTING_KNOWN)
                    commandSendValueOkResponse(tokens[0], field,
                                               value); // Just respond with the setting (not quotes needed)
                else if (response == SETTING_KNOWN_STRING)
                    commandSendStringOkResponse(tokens[0], field,
                                                value); // Wrap the string setting in quotes in the response, add OK
                else
                    commandSendErrorResponse(tokens[0], (char *)"Unknown setting");
            }
        }
        else if (strcmp(tokens[0], "SPEXE") == 0)
        {
            if (tokenCount != 2)
                commandSendErrorResponse(tokens[0],
                                         (char *)"Incorrect number of arguments"); // Incorrect number of arguments
            else
            {
                if (strcmp(tokens[1], "EXIT") == 0)
                {
                    commandSendExecuteOkResponse(tokens[0], tokens[1]);
                    break; // Exit while(1) loop
                }
                else if (strcmp(tokens[1], "APPLY") == 0)
                {
                    commandSendExecuteOkResponse(tokens[0], tokens[1]);
                    // Do an apply...
                }
                else if (strcmp(tokens[1], "SAVE") == 0)
                {
                    recordSystemSettings();
                    commandSendExecuteOkResponse(tokens[0], tokens[1]);
                }
                else if (strcmp(tokens[1], "REBOOT") == 0)
                {
                    commandSendExecuteOkResponse(tokens[0], tokens[1]);
                    delay(50); // Allow for final print
                    ESP.restart();
                }
                else if (strcmp(tokens[1], "LIST") == 0)
                {
                    // Respond with a list of variables, types, and current value
                    printAvailableSettings();

                    commandSendExecuteOkResponse(tokens[0], tokens[1]);
                }
                else
                    commandSendErrorResponse(tokens[0], tokens[1], (char *)"Unknown command");
            }
        }
        else
        {
            commandSendErrorResponse(tokens[0], (char *)"Unknown command");
        }
    } // while(1)

    btPrintEcho = false;
}

// Given a command, send structured OK response
// Ex: SPCMD = "$SPCMD,OK*61"
void commandSendOkResponse(char *command)
{
    // Create string between $ and * for checksum calculation
    char innerBuffer[200];
    sprintf(innerBuffer, "%s,OK", command);
    commandSendResponse(innerBuffer);
}

// Given a command, send response sentence with OK and checksum and <CR><LR>
// Ex: SPEXE,EXIT =
void commandSendExecuteOkResponse(char *command, char *settingName)
{
    // Create string between $ and * for checksum calculation
    char innerBuffer[200];
    sprintf(innerBuffer, "%s,%s,OK", command, settingName);
    commandSendResponse(innerBuffer);
}

// Given a command, and a value, send response sentence with OK and checksum and <CR><LR>
// Ex: SPSET,ntripClientCasterUserPW,thePassword = $SPSET,ntripClientCasterUserPW,"thePassword",OK*2F
void commandSendStringOkResponse(char *command, char *settingName, char *valueBuffer)
{
    // Add escapes for any quotes in valueBuffer
    // https://stackoverflow.com/a/26114476
    const char *escapeChar = "\"";
    char escapedValueBuffer[100];
    size_t bp = 0;

    for (size_t sp = 0; valueBuffer[sp]; sp++)
    {
        if (strchr(escapeChar, valueBuffer[sp]))
            escapedValueBuffer[bp++] = '\\';
        escapedValueBuffer[bp++] = valueBuffer[sp];
    }
    escapedValueBuffer[bp] = 0;

    // Create string between $ and * for checksum calculation
    char innerBuffer[200];
    sprintf(innerBuffer, "%s,%s,\"%s\",OK", command, settingName, escapedValueBuffer);
    commandSendResponse(innerBuffer);
}

// Given a command, and a value, send response sentence with checksum and <CR><LR>
// Ex: $SPGET,ntripClientCasterUserPW*35 = $SPSET,ntripClientCasterUserPW,"thePassword",OK*2F
void commandSendStringResponse(char *command, char *settingName, char *valueBuffer)
{
    // Add escapes for any quotes in valueBuffer
    // https://stackoverflow.com/a/26114476
    const char *escapeChar = "\"";
    char escapedValueBuffer[100];
    size_t bp = 0;

    for (size_t sp = 0; valueBuffer[sp]; sp++)
    {
        if (strchr(escapeChar, valueBuffer[sp]))
            escapedValueBuffer[bp++] = '\\';
        escapedValueBuffer[bp++] = valueBuffer[sp];
    }
    escapedValueBuffer[bp] = 0;

    // Create string between $ and * for checksum calculation
    char innerBuffer[200];
    sprintf(innerBuffer, "%s,%s,\"%s\"", command, settingName, escapedValueBuffer);
    commandSendResponse(innerBuffer);
}

// Given a command, send response sentence with OK and checksum and <CR><LR>
// Ex: observationPositionAccuracy,float,0.5 =
void commandSendExecuteListResponse(const char *settingName, const char *settingType, char *settingValue)
{
    // Create string between $ and * for checksum calculation
    char innerBuffer[200];

    // Put quotes around char settings
    if (strstr(settingType, "char"))
    {
        // Add escapes for any quotes in settingValue
        // https://stackoverflow.com/a/26114476
        const char *escapeChar = "\"";
        char escapedSettingValue[100];
        size_t bp = 0;

        for (size_t sp = 0; settingValue[sp]; sp++)
        {
            if (strchr(escapeChar, settingValue[sp]))
                escapedSettingValue[bp++] = '\\';
            escapedSettingValue[bp++] = settingValue[sp];
        }
        escapedSettingValue[bp] = 0;

        sprintf(innerBuffer, "SPLST,%s,%s,\"%s\"", settingName, settingType, settingValue);
    }
    else
        sprintf(innerBuffer, "SPLST,%s,%s,%s", settingName, settingType, settingValue);

    commandSendResponse(innerBuffer);
}

// Given a command, and a value, send response sentence with checksum and <CR><LR>
// Ex: SPGET,elvMask,0.25 = "$SPGET,elvMask,0.25*07"
void commandSendValueResponse(char *command, char *settingName, char *valueBuffer)
{
    // Create string between $ and * for checksum calculation
    char innerBuffer[200];
    sprintf(innerBuffer, "%s,%s,%s", command, settingName, valueBuffer);
    commandSendResponse(innerBuffer);
}

// Given a command, and a value, send response sentence with OK and checksum and <CR><LR>
// Ex: SPSET,elvMask,0.77 = "$SPSET,elvMask,0.77,OK*3C"
void commandSendValueOkResponse(char *command, char *settingName, char *valueBuffer)
{
    // Create string between $ and * for checksum calculation
    char innerBuffer[200];
    sprintf(innerBuffer, "%s,%s,%s,OK", command, settingName, valueBuffer);
    commandSendResponse(innerBuffer);
}

// Given a command, send structured ERROR response
// Response format: $SPxET,[setting name],,ERROR,[Verbose error description]*FF<CR><LF>
// Ex: SPGET,maxHeight,'Unknown setting' = "$SPGET,maxHeight,,ERROR,Unknown setting*58"
void commandSendErrorResponse(char *command, char *settingName, char *errorVerbose)
{
    // Create string between $ and * for checksum calculation
    char innerBuffer[200];
    snprintf(innerBuffer, sizeof(innerBuffer), "%s,%s,,ERROR,%s", command, settingName, errorVerbose);
    commandSendResponse(innerBuffer);
}
// Given a command, send structured ERROR response
// Response format: $SPxET,,,ERROR,[Verbose error description]*FF<CR><LF>
// Ex: SPGET, 'Incorrect number of arguments' = "$SPGET,ERROR,Incorrect number of arguments*1E"
void commandSendErrorResponse(char *command, char *errorVerbose)
{
    // Create string between $ and * for checksum calculation
    char innerBuffer[200];
    snprintf(innerBuffer, sizeof(innerBuffer), "%s,,,ERROR,%s", command, errorVerbose);
    commandSendResponse(innerBuffer);
}

// Given an inner buffer, send response sentence with checksum and <CR><LR>
// Ex: SPGET,elvMask,0.25 = $SPGET,elvMask,0.25*07
void commandSendResponse(char *innerBuffer)
{
    char responseBuffer[200];

    uint8_t calculatedChecksum = 0; // XOR chars between '$' and '*'
    for (int x = 0; x < strlen(innerBuffer); x++)
        calculatedChecksum = calculatedChecksum ^ innerBuffer[x];

    sprintf(responseBuffer, "$%s*%02X\r\n", innerBuffer, calculatedChecksum);

    systemPrint(responseBuffer);
}

// Checks structure of command and checksum
// If valid, returns true
// $SPCMD*49
// $SPGET,elvMask,15*1A
// getUserInputString() has removed any trailing <CR><LF> to the command
bool commandValid(char *commandString)
{
    // Check $/*
    if (commandString[0] != '$')
        return (false);

    if (commandString[strlen(commandString) - 3] != '*')
        return (false);

    // Check checksum is HEX
    char checksumMSN = commandString[strlen(commandString) - 2];
    char checksumLSN = commandString[strlen(commandString) - 1];
    if (isxdigit(checksumMSN) == false || isxdigit(checksumLSN) == false)
        return (false);

    // Convert checksum from ASCII to int
    char checksumChar[3] = {'\0'};
    checksumChar[0] = checksumMSN;
    checksumChar[1] = checksumLSN;
    uint8_t checksum = strtol(checksumChar, NULL, 16);

    // From: http://engineeringnotes.blogspot.com/2015/02/generate-crc-for-nmea-strings-arduino.html
    uint8_t calculatedChecksum = 0; // XOR chars between '$' and '*'
    for (int x = 1; x < strlen(commandString) - 3; x++)
        calculatedChecksum = calculatedChecksum ^ commandString[x];

    if (checksum != calculatedChecksum)
        return (false);

    return (true);
}

// Split a settingName into a truncatedName and a suffix
void commandSplitName(const char *settingName, char *truncatedName, int truncatedNameLen, char *suffix, int suffixLen)
{
    // The settingName contains an underscore at the split point, search
    // for the underscore, the truncatedName is on the left including
    // the underscore and the suffix is on the right.
    const char *underscore = strstr(settingName, "_");
    if (underscore != nullptr)
    {
        // Underscore found, so truncate
        int length = (underscore - settingName) / sizeof(char);
        length++; // Include the underscore
        if (length >= truncatedNameLen)
            length = truncatedNameLen - 1;
        strncpy(truncatedName, settingName, length);
        truncatedName[length] = 0; // Manually NULL-terminate because length < strlen(settingName)
        strncpy(suffix, &settingName[length], suffixLen - 1);
        suffix[suffixLen - 1] = 0;
    }
    else
    {
        strncpy(truncatedName, settingName, truncatedNameLen - 1);
        suffix[0] = 0;
    }
}

// Using the settingName string, return the index of the setting within commandIndex 
int commandLookupSettingName(const char *settingName, char *truncatedName, int truncatedNameLen, char *suffix,
                             int suffixLen)
{
    const char *command;

    // Loop through the valid command entries
    for (int i = 0; i < commandCount; i++)
    {
        // Verify that this command does not get split
        if ((commandIndex[i] >= 0) && (!rtkSettingsEntries[commandIndex[i]].useSuffix))
        {
            command = commandGetName(0, commandIndex[i]);

            // For speed, compare the first letter, then the whole string
            if ((command[0] == settingName[0]) && (strcmp(command, settingName) == 0))
            {
                return commandIndex[i];
            }
        }
    }

    // Split a settingName into a truncatedName and a suffix
    commandSplitName(settingName, truncatedName, truncatedNameLen, suffix, suffixLen);

    // Loop through the settings entries
    // E.g. by storing the previous value of i and starting there.
    // Most of the time, the match will be i+1.
    for (int i = 0; i < commandCount; i++)
    {
        // Verify that this command gets split
        if ((commandIndex[i] >= 0) && rtkSettingsEntries[commandIndex[i]].useSuffix)
        {
            command = commandGetName(0, commandIndex[i]);

            // For speed, compare the first letter, then the whole string
            if ((command[0] == truncatedName[0]) && (strcmp(command, truncatedName) == 0))
            {
                return commandIndex[i];
            }
        }
    }

    // Command not found
    return COMMAND_UNKNOWN;
}

// Check for unknown variables
bool commandCheckForUnknownVariable(const char *settingName, const char **entry, int tableEntries)
{
    const char **tableEnd;

    // Walk table of unused variables - read to avoid errors
    tableEnd = &entry[tableEntries];
    while (entry < tableEnd)
    {
        // Determine if the settingName is found in the table
        if (strcmp(settingName, *entry++) == 0)
            return true;
    }
    return false;
}

// Given a settingName, and string value, update a given setting
SettingValueResponse updateSettingWithValue(const char *settingName, const char *settingValueStr)
{
    int i;
    char *ptr;
    int qualifier;
    double settingValue;
    char suffix[51];
    char truncatedName[51];
    RTK_Settings_Types type;
    void *var;

    // Convert the value from a string into a number
    settingValue = strtod(settingValueStr, &ptr);
    if (strcmp(settingValueStr, "true") == 0)
        settingValue = 1;
    if (strcmp(settingValueStr, "false") == 0)
        settingValue = 0;

    bool knownSetting = false;
    bool settingIsString = false; // Goes true when setting needs to be surrounded by quotes during command response.
                                  // Generally char arrays but some others.

    // Loop through the valid command entries
    i = commandLookupSettingName(settingName, truncatedName, sizeof(truncatedName), suffix, sizeof(suffix));

    // Determine if settingName is in the command table
    if (i >= 0)
    {
        qualifier = rtkSettingsEntries[i].qualifier;
        type = rtkSettingsEntries[i].type;
        var = rtkSettingsEntries[i].var;
        switch (type)
        {
        default:
            break;
        case _bool: {
            bool *ptr = (bool *)var;
            *ptr = (bool)settingValue;
            knownSetting = true;
        }
        break;
        case _int: {
            int *ptr = (int *)var;
            *ptr = (int)settingValue;
            knownSetting = true;
        }
        break;
        case _float: {
            float *ptr = (float *)var;
            *ptr = (float)settingValue;
            knownSetting = true;
        }
        break;
        case _double: {
            double *ptr = (double *)var;
            *ptr = settingValue;
            knownSetting = true;
        }
        break;
        case _uint8_t: {
            uint8_t *ptr = (uint8_t *)var;
            *ptr = (uint8_t)settingValue;
            knownSetting = true;
        }
        break;
        case _uint16_t: {
            uint16_t *ptr = (uint16_t *)var;
            *ptr = (uint16_t)settingValue;
            knownSetting = true;
        }
        break;
        case _uint32_t: {
            uint32_t *ptr = (uint32_t *)var;
            *ptr = (uint32_t)settingValue;
            knownSetting = true;
        }
        break;
        case _uint64_t: {
            uint64_t *ptr = (uint64_t *)var;
            *ptr = (uint64_t)settingValue;
            knownSetting = true;
        }
        break;
        case _int8_t: {
            int8_t *ptr = (int8_t *)var;
            *ptr = (int8_t)settingValue;
            knownSetting = true;
        }
        break;
        case _int16_t: {
            int16_t *ptr = (int16_t *)var;
            *ptr = (int16_t)settingValue;
            knownSetting = true;
        }
        break;
        case tMuxConn: {
            muxConnectionType_e *ptr = (muxConnectionType_e *)var;
            *ptr = (muxConnectionType_e)settingValue;
            knownSetting = true;
        }
        break;
        case tSysState: {
            SystemState *ptr = (SystemState *)var;
            knownSetting = true;

            // 0 = Rover, 1 = Base, 2 = NTP
            if (settingValue == 2)
            {
                if (productVariant == RTK_EVK)
                    settings.lastState = STATE_NTPSERVER_NOT_STARTED;
                else
                    knownSetting = false;
            }
            if (settingValue == 1)
                settings.lastState = STATE_BASE_NOT_STARTED;
            else
                settings.lastState = STATE_ROVER_NOT_STARTED; // Default
        }
        break;
        case tPulseEdg: {
            pulseEdgeType_e *ptr = (pulseEdgeType_e *)var;
            *ptr = (pulseEdgeType_e)settingValue;
            knownSetting = true;
        }
        break;
        case tBtRadio: {
            BluetoothRadioType_e *ptr = (BluetoothRadioType_e *)var;
            *ptr = (BluetoothRadioType_e)settingValue;
            knownSetting = true;
        }
        break;
        case tPerDisp: {
            PeriodicDisplay_t *ptr = (PeriodicDisplay_t *)var;
            *ptr = (PeriodicDisplay_t)settingValue;
            knownSetting = true;
        }
        break;
        case tCoordInp: {
            CoordinateInputType *ptr = (CoordinateInputType *)var;
            *ptr = (CoordinateInputType)settingValue;
            knownSetting = true;
        }
        break;
        case tCharArry: {
            char *ptr = (char *)var;
            strncpy(ptr, settingValueStr, qualifier);
            // strncpy pads with zeros. No need to add them here for ntpReferenceId
            knownSetting = true;

            // Update the profile name in the file system if necessary
            if (strcmp(settingName, "profileName") == 0)
                setProfileName(profileNumber); // Copy the current settings.profileName into the array of profile names
                                               // at location profileNumber
            settingIsString = true;
        }
        break;
        case _IPString: {
            String tempString = String(settingValueStr);
            IPAddress *ptr = (IPAddress *)var;
            ptr->fromString(tempString);
            knownSetting = true;
            settingIsString = true;
        }
        break;
        case tUbxMsgRt: {
            for (int x = 0; x < qualifier; x++)
            {
                if ((suffix[0] == ubxMessages[x].msgTextName[0]) && (strcmp(suffix, ubxMessages[x].msgTextName) == 0))
                {
                    settings.ubxMessageRates[x] = (uint8_t)settingValue;
                    knownSetting = true;
                    break;
                }
            }
        }
        break;
        case tUbxConst: {
            for (int x = 0; x < qualifier; x++)
            {
                if ((suffix[0] == settings.ubxConstellations[x].textName[0]) &&
                    (strcmp(suffix, settings.ubxConstellations[x].textName) == 0))
                {
                    settings.ubxConstellations[x].enabled = settingValue;
                    knownSetting = true;
                    break;
                }
            }
        }
        break;
        case tEspNowPr: {
            int suffixNum;
            if (sscanf(suffix, "%d", &suffixNum) == 1)
            {
                int mac[6];
                if (sscanf(settingValueStr, "%X:%X:%X:%X:%X:%X", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4],
                           &mac[5]) == 6)
                {
                    for (int i = 0; i < 6; i++)
                        settings.espnowPeers[suffixNum][i] = mac[i];
                    knownSetting = true;
                    settingIsString = true;
                }
            }
        }
        break;
        case tUbMsgRtb: {
            int firstRTCMRecord = zedGetMessageNumberByName("UBX_RTCM_1005");

            for (int x = 0; x < qualifier; x++)
            {
                if ((suffix[0] == ubxMessages[firstRTCMRecord + x].msgTextName[0]) &&
                    (strcmp(suffix, ubxMessages[firstRTCMRecord + x].msgTextName) == 0))
                {
                    settings.ubxMessageRatesBase[x] = (uint8_t)settingValue;
                    knownSetting = true;
                    break;
                }
            }
        }
        break;
        case tWiFiNet: {
            int network;

            if (strstr(suffix, "SSID") != nullptr)
            {
                if (sscanf(suffix, "%dSSID", &network) == 1)
                {
                    strncpy(settings.wifiNetworks[network].ssid, settingValueStr,
                            sizeof(settings.wifiNetworks[0].ssid));
                    knownSetting = true;
                    settingIsString = true;
                }
            }
            else if (strstr(suffix, "Password") != nullptr)
            {
                if (sscanf(suffix, "%dPassword", &network) == 1)
                {
                    strncpy(settings.wifiNetworks[network].password, settingValueStr,
                            sizeof(settings.wifiNetworks[0].password));
                    knownSetting = true;
                    settingIsString = true;
                }
            }
        }
        break;
        case tNSCHost: {
            int server;
            if (sscanf(suffix, "%d", &server) == 1)
            {
                strncpy(&settings.ntripServer_CasterHost[server][0], settingValueStr,
                        sizeof(settings.ntripServer_CasterHost[server]));
                knownSetting = true;
                settingIsString = true;
            }
        }
        break;
        case tNSCPort: {
            int server;
            if (sscanf(suffix, "%d", &server) == 1)
            {
                settings.ntripServer_CasterPort[server] = settingValue;
                knownSetting = true;
            }
        }
        break;
        case tNSCUser: {
            int server;
            if (sscanf(suffix, "%d", &server) == 1)
            {
                strncpy(&settings.ntripServer_CasterUser[server][0], settingValueStr,
                        sizeof(settings.ntripServer_CasterUser[server]));
                knownSetting = true;
                settingIsString = true;
            }
        }
        break;
        case tNSCUsrPw: {
            int server;
            if (sscanf(suffix, "%d", &server) == 1)
            {
                strncpy(&settings.ntripServer_CasterUserPW[server][0], settingValueStr,
                        sizeof(settings.ntripServer_CasterUserPW[server]));
                knownSetting = true;
                settingIsString = true;
            }
        }
        break;
        case tNSMtPt: {
            int server;
            if (sscanf(suffix, "%d", &server) == 1)
            {
                strncpy(&settings.ntripServer_MountPoint[server][0], settingValueStr,
                        sizeof(settings.ntripServer_MountPoint[server]));
                knownSetting = true;
                settingIsString = true;
            }
        }
        break;
        case tNSMtPtPw: {
            int server;
            if (sscanf(suffix, "%d", &server) == 1)
            {
                strncpy(&settings.ntripServer_MountPointPW[server][0], settingValueStr,
                        sizeof(settings.ntripServer_MountPointPW[server]));
                knownSetting = true;
                settingIsString = true;
            }
        }
        break;
        case tUmMRNmea: {
            for (int x = 0; x < qualifier; x++)
            {
                if ((suffix[0] == umMessagesNMEA[x].msgTextName[0]) &&
                    (strcmp(suffix, umMessagesNMEA[x].msgTextName) == 0))
                {
                    settings.um980MessageRatesNMEA[x] = settingValue;
                    knownSetting = true;
                    break;
                }
            }
        }
        break;
        case tUmMRRvRT: {
            for (int x = 0; x < qualifier; x++)
            {
                if ((suffix[0] == umMessagesRTCM[x].msgTextName[0]) &&
                    (strcmp(suffix, umMessagesRTCM[x].msgTextName) == 0))
                {
                    settings.um980MessageRatesRTCMRover[x] = settingValue;
                    knownSetting = true;
                    break;
                }
            }
        }
        break;
        case tUmMRBaRT: {
            for (int x = 0; x < qualifier; x++)
            {
                if ((suffix[0] == umMessagesRTCM[x].msgTextName[0]) &&
                    (strcmp(suffix, umMessagesRTCM[x].msgTextName) == 0))
                {
                    settings.um980MessageRatesRTCMBase[x] = settingValue;
                    knownSetting = true;
                    break;
                }
            }
        }
        break;
        case tUmConst: {
            for (int x = 0; x < qualifier; x++)
            {
                if ((suffix[0] == um980ConstellationCommands[x].textName[0]) &&
                    (strcmp(suffix, um980ConstellationCommands[x].textName) == 0))
                {
                    settings.um980Constellations[x] = settingValue;
                    knownSetting = true;
                    break;
                }
            }
        }
        break;
        case tCorrSPri: {
            for (int x = 0; x < qualifier; x++)
            {
                if ((suffix[0] == correctionsSourceNames[x][0]) && (strcmp(suffix, correctionsSourceNames[x]) == 0))
                {
                    settings.correctionsSourcesPriority[x] = settingValue;
                    knownSetting = true;
                    break;
                }
            }
        }
        break;
        case tRegCorTp: {
            int region;
            if (sscanf(suffix, "%d", &region) == 1)
            {
                strncpy(&settings.regionalCorrectionTopics[region][0], settingValueStr,
                        sizeof(settings.regionalCorrectionTopics[0]));
                knownSetting = true;
            }
        }
        break;
        }
    }

    // Done when the setting is found
    if (knownSetting == true && settingIsString == true)
        return (SETTING_KNOWN_STRING);
    else if (knownSetting == true)
        return (SETTING_KNOWN);

    if (strcmp(settingName, "fixedLatText") == 0)
    {
        double newCoordinate = 0.0;
        CoordinateInputType newCoordinateInputType =
            coordinateIdentifyInputType((char *)settingValueStr, &newCoordinate);
        if (newCoordinateInputType == COORDINATE_INPUT_TYPE_INVALID_UNKNOWN)
            settings.fixedLat = 0.0;
        else
        {
            settings.fixedLat = newCoordinate;
            settings.coordinateInputType = newCoordinateInputType;
        }
        knownSetting = true;
    }
    else if (strcmp(settingName, "fixedLongText") == 0)
    {
        double newCoordinate = 0.0;
        if (coordinateIdentifyInputType((char *)settingValueStr, &newCoordinate) ==
            COORDINATE_INPUT_TYPE_INVALID_UNKNOWN)
            settings.fixedLong = 0.0;
        else
            settings.fixedLong = newCoordinate;
        knownSetting = true;
    }

    else if (strcmp(settingName, "measurementRateHz") == 0)
    {
        gnssSetRate(1.0 / settingValue);

        // This is one of the first settings to be received. If seen, remove the station files.
        removeFile(stationCoordinateECEFFileName);
        removeFile(stationCoordinateGeodeticFileName);
        if (settings.debugWiFiConfig == true)
            systemPrintln("Station coordinate files removed");
        knownSetting = true;
    }

    // navigationRate is calculated using measurementRateHz

    else if (strstr(settingName, "stationECEF") != nullptr)
    {
        replaceCharacter((char *)settingValueStr, ' ', ','); // Replace all ' ' with ',' before recording to file
        recordLineToSD(stationCoordinateECEFFileName, settingValueStr);
        recordLineToLFS(stationCoordinateECEFFileName, settingValueStr);
        if (settings.debugWiFiConfig == true)
            systemPrintf("%s recorded\r\n", settingValueStr);
        knownSetting = true;
    }
    else if (strstr(settingName, "stationGeodetic") != nullptr)
    {
        replaceCharacter((char *)settingValueStr, ' ', ','); // Replace all ' ' with ',' before recording to file
        recordLineToSD(stationCoordinateGeodeticFileName, settingValueStr);
        recordLineToLFS(stationCoordinateGeodeticFileName, settingValueStr);
        if (settings.debugWiFiConfig == true)
            systemPrintf("%s recorded\r\n", settingValueStr);
        knownSetting = true;
    }
    else if (strcmp(settingName, "minCNO") == 0)
    {
        // Note: this sends the Min CNO to the GNSS, as well as saving it in settings... Is this what we want? TODO
        gnssSetMinCno(settingValue);
        knownSetting = true;
    }
    else if (strcmp(settingName, "fixedHAEAPC") == 0)
    {
        // TODO: check this!!
        knownSetting = true;
    }
    else if (strcmp(settingName, "baseTypeFixed") == 0)
    {
        settings.fixedBase = ((settingValue == 1) ? true : false);
        knownSetting = true;
    }
    else if (strcmp(settingName, "fixedBaseCoordinateTypeECEF") == 0)
    {
        settings.fixedBaseCoordinateType = ((settingValue == 1) ? COORD_TYPE_ECEF : COORD_TYPE_GEODETIC);
        knownSetting = true;
    }

    // Special actions
    else if (strcmp(settingName, "enableRCFirmware") == 0)
    {
        enableRCFirmware = settingValue;
        knownSetting = true;
    }
    else if (strcmp(settingName, "firmwareFileName") == 0)
    {
        mountSDThenUpdate(settingValueStr);

        // If update is successful, it will force system reset and not get here.

        if (productVariant == RTK_EVK)
            requestChangeState(STATE_BASE_NOT_STARTED); // If update failed, return to Base mode.
        else
            requestChangeState(STATE_ROVER_NOT_STARTED); // If update failed, return to Rover mode.
        knownSetting = true;
    }
    else if (strcmp(settingName, "factoryDefaultReset") == 0)
    {
        factoryReset(false); // We do not have the sdSemaphore
        knownSetting = true;
    }
    else if (strcmp(settingName, "exitAndReset") == 0)
    {
        // Confirm receipt
        if (settings.debugWiFiConfig == true)
            systemPrintln("Sending reset confirmation");

        sendStringToWebsocket((char *)"confirmReset,1,");
        delay(500); // Allow for delivery

        if (configureViaEthernet)
            systemPrintln("Reset after Configure-Via-Ethernet");
        else
            systemPrintln("Reset after AP Config");

        if (configureViaEthernet)
        {
            ethernetWebServerStopESP32W5500();

            // We need to exit configure-via-ethernet mode.
            // But if the settings have not been saved then lastState will still be STATE_CONFIG_VIA_ETH_STARTED.
            // If that is true, then force exit to Base mode. I think it is the best we can do.
            //(If the settings have been saved, then the code will restart in NTP, Base or Rover mode as desired.)
            if (settings.lastState == STATE_CONFIG_VIA_ETH_STARTED)
            {
                systemPrintln("Settings were not saved. Resetting into Base mode.");
                settings.lastState = STATE_BASE_NOT_STARTED;
                recordSystemSettings();
            }
        }

        ESP.restart();
    }
    else if (strcmp(settingName, "setProfile") == 0)
    {
        // Change to new profile
        if (settings.debugWiFiConfig == true)
            systemPrintf("Changing to profile number %d\r\n", settingValue);
        changeProfileNumber(settingValue);

        // Load new profile into system
        loadSettings();

        // Send new settings to browser. Re-use settingsCSV to avoid stack.
        memset(settingsCSV, 0, AP_CONFIG_SETTING_SIZE); // Clear any garbage from settings array

        createSettingsString(settingsCSV);

        if (settings.debugWiFiConfig == true)
        {
            systemPrintf("Sending profile %d\r\n", settingValue);
            systemPrintf("Profile contents: %s\r\n", settingsCSV);
        }

        sendStringToWebsocket(settingsCSV);
        knownSetting = true;
    }
    else if (strcmp(settingName, "resetProfile") == 0)
    {
        settingsToDefaults(); // Overwrite our current settings with defaults

        recordSystemSettings(); // Overwrite profile file and NVM with these settings

        // Get bitmask of active profiles
        activeProfiles = loadProfileNames();

        // Send new settings to browser. Re-use settingsCSV to avoid stack.
        memset(settingsCSV, 0, AP_CONFIG_SETTING_SIZE); // Clear any garbage from settings array

        createSettingsString(settingsCSV);

        if (settings.debugWiFiConfig == true)
        {
            systemPrintf("Sending reset profile %d\r\n", settingValue);
            systemPrintf("Profile contents: %s\r\n", settingsCSV);
        }

        sendStringToWebsocket(settingsCSV);
        knownSetting = true;
    }
    else if (strcmp(settingName, "forgetEspNowPeers") == 0)
    {
        // Forget all ESP-Now Peers
        for (int x = 0; x < settings.espnowPeerCount; x++)
            espnowRemovePeer(settings.espnowPeers[x]);
        settings.espnowPeerCount = 0;
        knownSetting = true;
    }
    else if (strcmp(settingName, "startNewLog") == 0)
    {
        if (settings.enableLogging == true && online.logging == true)
        {
            endLogging(false, true); //(gotSemaphore, releaseSemaphore) Close file. Reset parser stats.
            beginLogging();          // Create new file based on current RTC.
            setLoggingType();        // Determine if we are standard, PPP, or custom. Changes logging icon accordingly.

            char newFileNameCSV[sizeof("logFileName,") + sizeof(logFileName) + 1];
            snprintf(newFileNameCSV, sizeof(newFileNameCSV), "logFileName,%s,", logFileName);

            sendStringToWebsocket(newFileNameCSV); // Tell the config page the name of the file we just created
        }
        knownSetting = true;
    }
    else if (strcmp(settingName, "checkNewFirmware") == 0)
    {
        if (settings.debugWiFiConfig == true)
            systemPrintln("Checking for new OTA Pull firmware");

        sendStringToWebsocket((char *)"checkingNewFirmware,1,"); // Tell the config page we received their request

        char reportedVersion[20];
        char newVersionCSV[100];

        // Get firmware version from server
        // otaCheckVersion will call wifiConnect if needed
        if (otaCheckVersion(reportedVersion, sizeof(reportedVersion)))
        {
            // We got a version number, now determine if it's newer or not
            char currentVersion[21];
            getFirmwareVersion(currentVersion, sizeof(currentVersion), enableRCFirmware);
            if (isReportedVersionNewer(reportedVersion, currentVersion) == true)
            {
                if (settings.debugWiFiConfig == true)
                    systemPrintln("New version detected");
                snprintf(newVersionCSV, sizeof(newVersionCSV), "newFirmwareVersion,%s,", reportedVersion);
            }
            else
            {
                if (settings.debugWiFiConfig == true)
                    systemPrintln("No new firmware available");
                snprintf(newVersionCSV, sizeof(newVersionCSV), "newFirmwareVersion,CURRENT,");
            }
        }
        else
        {
            // Failed to get version number
            if (settings.debugWiFiConfig == true)
                systemPrintln("Sending error to AP config page");
            snprintf(newVersionCSV, sizeof(newVersionCSV), "newFirmwareVersion,ERROR,");
        }

        sendStringToWebsocket(newVersionCSV);
        knownSetting = true;
    }
    else if (strcmp(settingName, "getNewFirmware") == 0)
    {
        if (settings.debugWiFiConfig == true)
            systemPrintln("Getting new OTA Pull firmware");

        sendStringToWebsocket((char *)"gettingNewFirmware,1,");

        apConfigFirmwareUpdateInProcess = true;
        otaUpdate(); // otaUpdate will call wifiConnect if needed. Also does previouslyConnected check

        // We get here if WiFi failed to connect
        sendStringToWebsocket((char *)"gettingNewFirmware,ERROR,");
        knownSetting = true;
    }

    // Unused variables - read to avoid errors
    else
    {
        const char *table[] = {
            "baseTypeSurveyIn", "enableFactoryDefaults",      "enableFirmwareUpdate", "enableForgetRadios",
            "fileSelectAll",    "fixedBaseCoordinateTypeGeo", "fixedHAEAPC",          "measurementRateSec",
            "nicknameECEF",     "nicknameGeodetic",           "saveToArduino",
        };
        const int tableEntries = sizeof(table) / sizeof(table[0]);

        knownSetting = commandCheckForUnknownVariable(settingName, table, tableEntries);
    }

    // If we've received a setting update for a setting that is not valid to this platform,
    // allow it, but throw a warning.
    // This is often caused by the Web Config page reporting all cells including
    // those that are hidden because of platform limitations
    if (knownSetting == true && settingAvailableOnPlatform(i) == false)
    {
        systemPrintf("Setting '%s' stored but not supported on this platform\r\n", settingName);
    }

    // Last catch
    if (knownSetting == false)
    {
        systemPrintf("Unknown '%s': %0.3lf\r\n", settingName, settingValue);
    }

    if (knownSetting == true && settingIsString == true)
        return (SETTING_KNOWN_STRING);
    else if (knownSetting == true)
        return (SETTING_KNOWN);

    return (SETTING_UNKNOWN);
}

// Create a csv string with current settings
// The order of variables matches the order found in settings.h
void createSettingsString(char *newSettings)
{
    char tagText[80];
    char nameText[64];

    newSettings[0] = '\0'; // Erase current settings string

    // System Info
    char apPlatformPrefix[80];
    strncpy(apPlatformPrefix, platformPrefixTable[productVariant], sizeof(apPlatformPrefix));
    stringRecord(newSettings, "platformPrefix", apPlatformPrefix);

    char apRtkFirmwareVersion[86];
    getFirmwareVersion(apRtkFirmwareVersion, sizeof(apRtkFirmwareVersion), true);
    stringRecord(newSettings, "rtkFirmwareVersion", apRtkFirmwareVersion);

    char apGNSSFirmwareVersion[80];
    if (present.gnss_zedf9p)
    {
        snprintf(apGNSSFirmwareVersion, sizeof(apGNSSFirmwareVersion), "ZED-F9P Firmware: %s ID: %s",
                 gnssFirmwareVersion, gnssUniqueId);
    }
    else if (present.gnss_um980)
    {
        snprintf(apGNSSFirmwareVersion, sizeof(apGNSSFirmwareVersion), "UM980 Firmware: %s ID: %s", gnssFirmwareVersion,
                 gnssUniqueId);
    }
    else if (present.gnss_mosaicX5)
    {
        // *** TODO ***
    }
    stringRecord(newSettings, "gnssFirmwareVersion", apGNSSFirmwareVersion);
    stringRecord(newSettings, "gnssFirmwareVersionInt", gnssFirmwareVersionInt);

    char apDeviceBTID[30];
    snprintf(apDeviceBTID, sizeof(apDeviceBTID), "Device Bluetooth ID: %02X%02X", btMACAddress[4], btMACAddress[5]);
    stringRecord(newSettings, "deviceBTID", apDeviceBTID);

    for (int i = 0; i < numRtkSettingsEntries; i++)
    {
        if (rtkSettingsEntries[i].inWebConfig && (settingAvailableOnPlatform(i) == true))
        {
            switch (rtkSettingsEntries[i].type)
            {
            default:
                break;
            case _bool: {
                bool *ptr = (bool *)rtkSettingsEntries[i].var;
                stringRecord(newSettings, rtkSettingsEntries[i].name, *ptr);
            }
            break;
            case _int: {
                int *ptr = (int *)rtkSettingsEntries[i].var;
                stringRecord(newSettings, rtkSettingsEntries[i].name, *ptr);
            }
            break;
            case _float: {
                float *ptr = (float *)rtkSettingsEntries[i].var;
                stringRecord(newSettings, rtkSettingsEntries[i].name, (double)*ptr, rtkSettingsEntries[i].qualifier);
            }
            break;
            case _double: {
                double *ptr = (double *)rtkSettingsEntries[i].var;
                stringRecord(newSettings, rtkSettingsEntries[i].name, *ptr, rtkSettingsEntries[i].qualifier);
            }
            break;
            case _uint8_t: {
                uint8_t *ptr = (uint8_t *)rtkSettingsEntries[i].var;
                stringRecord(newSettings, rtkSettingsEntries[i].name, (int)*ptr);
            }
            break;
            case _uint16_t: {
                uint16_t *ptr = (uint16_t *)rtkSettingsEntries[i].var;
                stringRecord(newSettings, rtkSettingsEntries[i].name, (int)*ptr);
            }
            break;
            case _uint32_t: {
                uint32_t *ptr = (uint32_t *)rtkSettingsEntries[i].var;
                stringRecord(newSettings, rtkSettingsEntries[i].name, *ptr);
            }
            break;
            case _uint64_t: {
                uint64_t *ptr = (uint64_t *)rtkSettingsEntries[i].var;
                stringRecord(newSettings, rtkSettingsEntries[i].name, *ptr);
            }
            break;
            case _int8_t: {
                int8_t *ptr = (int8_t *)rtkSettingsEntries[i].var;
                stringRecord(newSettings, rtkSettingsEntries[i].name, (int)*ptr);
            }
            break;
            case _int16_t: {
                int16_t *ptr = (int16_t *)rtkSettingsEntries[i].var;
                stringRecord(newSettings, rtkSettingsEntries[i].name, (int)*ptr);
            }
            break;
            case tMuxConn: {
                muxConnectionType_e *ptr = (muxConnectionType_e *)rtkSettingsEntries[i].var;
                stringRecord(newSettings, rtkSettingsEntries[i].name, (int)*ptr);
            }
            break;
            case tSysState: {
                SystemState *ptr = (SystemState *)rtkSettingsEntries[i].var;
                stringRecord(newSettings, rtkSettingsEntries[i].name, (int)*ptr);
            }
            break;
            case tPulseEdg: {
                pulseEdgeType_e *ptr = (pulseEdgeType_e *)rtkSettingsEntries[i].var;
                stringRecord(newSettings, rtkSettingsEntries[i].name, (int)*ptr);
            }
            break;
            case tBtRadio: {
                BluetoothRadioType_e *ptr = (BluetoothRadioType_e *)rtkSettingsEntries[i].var;
                stringRecord(newSettings, rtkSettingsEntries[i].name, (int)*ptr);
            }
            break;
            case tPerDisp: {
                PeriodicDisplay_t *ptr = (PeriodicDisplay_t *)rtkSettingsEntries[i].var;
                stringRecord(newSettings, rtkSettingsEntries[i].name, (int)*ptr);
            }
            break;
            case tCoordInp: {
                CoordinateInputType *ptr = (CoordinateInputType *)rtkSettingsEntries[i].var;
                stringRecord(newSettings, rtkSettingsEntries[i].name, (int)*ptr);
            }
            break;
            case tCharArry: {
                char *ptr = (char *)rtkSettingsEntries[i].var;
                stringRecord(newSettings, rtkSettingsEntries[i].name, ptr);
            }
            break;
            case _IPString: {
                IPAddress *ptr = (IPAddress *)rtkSettingsEntries[i].var;
                stringRecord(newSettings, rtkSettingsEntries[i].name, (char *)ptr->toString().c_str());
            }
            break;
            case tUbxMsgRt: {
                // Record message settings
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    char tempString[50];
                    snprintf(tempString, sizeof(tempString), "%s%s,%d,", rtkSettingsEntries[i].name,
                             ubxMessages[x].msgTextName, settings.ubxMessageRates[x]);
                    stringRecord(newSettings, tempString);
                }
            }
            break;
            case tUbxConst: {
                // Record constellation settings
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    char tempString[50];
                    snprintf(tempString, sizeof(tempString), "%s%s,%s,", rtkSettingsEntries[i].name,
                             settings.ubxConstellations[x].textName,
                             settings.ubxConstellations[x].enabled ? "true" : "false");
                    stringRecord(newSettings, tempString);
                }
            }
            break;
            case tEspNowPr: {
                // Record ESP-Now peer MAC addresses
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    char tempString[50]; // espnowPeers_1=B4:C1:33:42:DE:01,
                    snprintf(tempString, sizeof(tempString), "%s%d,%02X:%02X:%02X:%02X:%02X:%02X,",
                             rtkSettingsEntries[i].name, x, settings.espnowPeers[x][0], settings.espnowPeers[x][1],
                             settings.espnowPeers[x][2], settings.espnowPeers[x][3], settings.espnowPeers[x][4],
                             settings.espnowPeers[x][5]);
                    stringRecord(newSettings, tempString);
                }
            }
            break;
            case tUbMsgRtb: {
                // Record message settings

                int firstRTCMRecord = zedGetMessageNumberByName("UBX_RTCM_1005");

                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    char tempString[50];
                    snprintf(tempString, sizeof(tempString), "%s%s,%d,", rtkSettingsEntries[i].name,
                             ubxMessages[firstRTCMRecord + x].msgTextName, settings.ubxMessageRatesBase[x]);
                    stringRecord(newSettings, tempString);
                }
            }
            break;
            case tWiFiNet: {
                // Record WiFi credential table
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    char tempString[100]; // wifiNetwork_0Password=parachutes

                    snprintf(tempString, sizeof(tempString), "%s%dSSID,%s,", rtkSettingsEntries[i].name, x,
                             settings.wifiNetworks[x].ssid);
                    stringRecord(newSettings, tempString);
                    snprintf(tempString, sizeof(tempString), "%s%dPassword,%s,", rtkSettingsEntries[i].name, x,
                             settings.wifiNetworks[x].password);
                    stringRecord(newSettings, tempString);
                }
            }
            break;
            case tNSCHost: {
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    char tempString[50];
                    snprintf(tempString, sizeof(tempString), "%s%d,%s,", rtkSettingsEntries[i].name, x,
                             &settings.ntripServer_CasterHost[x][0]);
                    stringRecord(newSettings, tempString);
                }
            }
            break;
            case tNSCPort: {
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    char tempString[50];
                    snprintf(tempString, sizeof(tempString), "%s%d,%d,", rtkSettingsEntries[i].name, x,
                             settings.ntripServer_CasterPort[x]);
                    stringRecord(newSettings, tempString);
                }
            }
            break;
            case tNSCUser: {
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    char tempString[50];
                    snprintf(tempString, sizeof(tempString), "%s%d,%s,", rtkSettingsEntries[i].name, x,
                             &settings.ntripServer_CasterUser[x][0]);
                    stringRecord(newSettings, tempString);
                }
            }
            break;
            case tNSCUsrPw: {
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    char tempString[50];
                    snprintf(tempString, sizeof(tempString), "%s%d,%s,", rtkSettingsEntries[i].name, x,
                             &settings.ntripServer_CasterUserPW[x][0]);
                    stringRecord(newSettings, tempString);
                }
            }
            break;
            case tNSMtPt: {
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    char tempString[50];
                    snprintf(tempString, sizeof(tempString), "%s%d,%s,", rtkSettingsEntries[i].name, x,
                             &settings.ntripServer_MountPoint[x][0]);
                    stringRecord(newSettings, tempString);
                }
            }
            break;
            case tNSMtPtPw: {
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    char tempString[50];
                    snprintf(tempString, sizeof(tempString), "%s%d,%s,", rtkSettingsEntries[i].name, x,
                             &settings.ntripServer_MountPointPW[x][0]);
                    stringRecord(newSettings, tempString);
                }
            }
            break;
            case tUmMRNmea: {
                // Record UM980 NMEA rates
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    char tempString[50]; // um980MessageRatesNMEA_GPDTM=0.05
                    snprintf(tempString, sizeof(tempString), "%s%s,%0.2f,", rtkSettingsEntries[i].name,
                             umMessagesNMEA[x].msgTextName, settings.um980MessageRatesNMEA[x]);
                    stringRecord(newSettings, tempString);
                }
            }
            break;
            case tUmMRRvRT: {
                // Record UM980 Rover RTCM rates
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    char tempString[50]; // um980MessageRatesRTCMRover_RTCM1001=0.2
                    snprintf(tempString, sizeof(tempString), "%s%s,%0.2f,", rtkSettingsEntries[i].name,
                             umMessagesRTCM[x].msgTextName, settings.um980MessageRatesRTCMRover[x]);
                    stringRecord(newSettings, tempString);
                }
            }
            break;
            case tUmMRBaRT: {
                // Record UM980 Base RTCM rates
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    char tempString[50]; // um980MessageRatesRTCMBase.RTCM1001=0.2
                    snprintf(tempString, sizeof(tempString), "%s%s,%0.2f,", rtkSettingsEntries[i].name,
                             umMessagesRTCM[x].msgTextName, settings.um980MessageRatesRTCMBase[x]);
                    stringRecord(newSettings, tempString);
                }
            }
            break;
            case tUmConst: {
                // Record UM980 Constellations
                // um980Constellations are uint8_t, but here we have to convert to bool (true / false) so the web page
                // check boxes are populated correctly. (We can't make it bool, otherwise the 254 initializer will
                // probably fail...)
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    char tempString[50]; // um980Constellations.GLONASS=true
                    snprintf(tempString, sizeof(tempString), "%s%s,%s,", rtkSettingsEntries[i].name,
                             um980ConstellationCommands[x].textName,
                             ((settings.um980Constellations[x] == 0) ? "false" : "true"));
                    stringRecord(newSettings, tempString);
                }
            }
            break;
            case tCorrSPri: {
                // Record corrections priorities
                for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
                {
                    char tempString[80]; // correctionsPriority.Ethernet_IP_(PointPerfect/MQTT)=99
                    snprintf(tempString, sizeof(tempString), "%s%s,%0d,", rtkSettingsEntries[i].name,
                             correctionsSourceNames[x], settings.correctionsSourcesPriority[x]);
                    stringRecord(newSettings, tempString);
                }
            }
            break;
            case tRegCorTp: {
                for (int r = 0; r < rtkSettingsEntries[i].qualifier; r++)
                {
                    char tempString[50];
                    snprintf(tempString, sizeof(tempString), "%s%d,%s,", rtkSettingsEntries[i].name, r,
                             &settings.regionalCorrectionTopics[r][0]);
                    stringRecord(newSettings, tempString);
                }
            }
            break;
            }
        }
    }

    stringRecord(newSettings, "baseTypeSurveyIn", !settings.fixedBase);
    stringRecord(newSettings, "baseTypeFixed", settings.fixedBase);

    if (settings.fixedBaseCoordinateType == COORD_TYPE_ECEF)
    {
        stringRecord(newSettings, "fixedBaseCoordinateTypeECEF", true);
        stringRecord(newSettings, "fixedBaseCoordinateTypeGeo", false);
    }
    else
    {
        stringRecord(newSettings, "fixedBaseCoordinateTypeECEF", false);
        stringRecord(newSettings, "fixedBaseCoordinateTypeGeo", true);
    }

    stringRecord(newSettings, "measurementRateHz", 1.0 / gnssGetRateS(), 2); // 2 = decimals to print

    // System state at power on. Convert various system states to either Rover or Base or NTP.
    int lastState; // 0 = Rover, 1 = Base, 2 = NTP
    if (present.ethernet_ws5500 == true)
    {
        lastState = 1; // Default Base
        if (settings.lastState >= STATE_ROVER_NOT_STARTED && settings.lastState <= STATE_ROVER_RTK_FIX)
            lastState = 0;
        if (settings.lastState >= STATE_NTPSERVER_NOT_STARTED && settings.lastState <= STATE_NTPSERVER_SYNC)
            lastState = 2;
    }
    else
    {
        lastState = 0; // Default Rover
        if (settings.lastState >= STATE_BASE_NOT_STARTED && settings.lastState <= STATE_BASE_FIXED_TRANSMITTING)
            lastState = 1;
    }
    stringRecord(newSettings, "lastState", lastState);

    stringRecord(
        newSettings, "profileName",
        profileNames[profileNumber]); // Must come before profile number so AP config page JS has name before number
    stringRecord(newSettings, "profileNumber", profileNumber);
    for (int index = 0; index < MAX_PROFILE_COUNT; index++)
    {
        snprintf(tagText, sizeof(tagText), "profile%dName", index);
        snprintf(nameText, sizeof(nameText), "%d: %s", index + 1, profileNames[index]);
        stringRecord(newSettings, tagText, nameText);
    }

    // Drop downs on the AP config page expect a value, whereas bools get stringRecord as true/false
    if (settings.wifiConfigOverAP == true)
        stringRecord(newSettings, "wifiConfigOverAP", 1); // 1 = AP mode, 0 = WiFi
    else
        stringRecord(newSettings, "wifiConfigOverAP", 0); // 1 = AP mode, 0 = WiFi

    // Single variables needed on Config page
    stringRecord(newSettings, "minCNO", gnssGetMinCno());
    stringRecord(newSettings, "enableRCFirmware", enableRCFirmware);

    // Add SD Characteristics
    char sdCardSizeChar[20];
    String cardSize;
    stringHumanReadableSize(cardSize, sdCardSize);
    cardSize.toCharArray(sdCardSizeChar, sizeof(sdCardSizeChar));
    char sdFreeSpaceChar[20];
    String freeSpace;
    stringHumanReadableSize(freeSpace, sdFreeSpace);
    freeSpace.toCharArray(sdFreeSpaceChar, sizeof(sdFreeSpaceChar));

    stringRecord(newSettings, "sdFreeSpace", sdFreeSpaceChar);
    stringRecord(newSettings, "sdSize", sdCardSizeChar);
    stringRecord(newSettings, "sdMounted", online.microSD);

    // Add Device ID used for corrections
    char hardwareID[15];
    snprintf(hardwareID, sizeof(hardwareID), "%02X%02X%02X%02X%02X%02X%02X", btMACAddress[0], btMACAddress[1],
             btMACAddress[2], btMACAddress[3], btMACAddress[4], btMACAddress[5], productVariant);
    stringRecord(newSettings, "hardwareID", hardwareID);

    // Add Days Remaining for corrections
    char apDaysRemaining[20];
    if (strlen(settings.pointPerfectCurrentKey) > 0)
    {
        int daysRemaining = daysFromEpoch(settings.pointPerfectNextKeyStart + settings.pointPerfectNextKeyDuration + 1);
        snprintf(apDaysRemaining, sizeof(apDaysRemaining), "%d", daysRemaining);
    }
    else
        snprintf(apDaysRemaining, sizeof(apDaysRemaining), "No Keys");

    stringRecord(newSettings, "daysRemaining", apDaysRemaining);

    // Current coordinates come from HPPOSLLH call back
    stringRecord(newSettings, "geodeticLat", gnssGetLatitude(), haeNumberOfDecimals);
    stringRecord(newSettings, "geodeticLon", gnssGetLongitude(), haeNumberOfDecimals);
    stringRecord(newSettings, "geodeticAlt", gnssGetAltitude(), 3);

    double ecefX = 0;
    double ecefY = 0;
    double ecefZ = 0;

    geodeticToEcef(gnssGetLatitude(), gnssGetLongitude(), gnssGetAltitude(), &ecefX, &ecefY, &ecefZ);

    stringRecord(newSettings, "ecefX", ecefX, 3);
    stringRecord(newSettings, "ecefY", ecefY, 3);
    stringRecord(newSettings, "ecefZ", ecefZ, 3);

    // Radio / ESP-Now settings
    char radioMAC[18]; // Send radio MAC
    snprintf(radioMAC, sizeof(radioMAC), "%02X:%02X:%02X:%02X:%02X:%02X", wifiMACAddress[0], wifiMACAddress[1],
             wifiMACAddress[2], wifiMACAddress[3], wifiMACAddress[4], wifiMACAddress[5]);
    stringRecord(newSettings, "radioMAC", radioMAC);

    stringRecord(newSettings, "logFileName", logFileName);

    // Add battery level and icon file name
    if (online.batteryFuelGauge == false) // Product has no battery
    {
        stringRecord(newSettings, "batteryIconFileName", (char *)"src/BatteryBlank.png");
        stringRecord(newSettings, "batteryPercent", (char *)" ");
    }
    else
    {
        // Determine battery icon
        int iconLevel = 0;
        if (batteryLevelPercent < 25)
            iconLevel = 0;
        else if (batteryLevelPercent < 50)
            iconLevel = 1;
        else if (batteryLevelPercent < 75)
            iconLevel = 2;
        else // batt level > 75
            iconLevel = 3;

        char batteryIconFileName[sizeof("src/Battery2_Charging.png__")]; // sizeof() includes 1 for \0 termination

        if (isCharging())
            snprintf(batteryIconFileName, sizeof(batteryIconFileName), "src/Battery%d_Charging.png", iconLevel);
        else
            snprintf(batteryIconFileName, sizeof(batteryIconFileName), "src/Battery%d.png", iconLevel);

        stringRecord(newSettings, "batteryIconFileName", batteryIconFileName);

        // Determine battery percent
        char batteryPercent[15];
        int tempLevel = batteryLevelPercent;
        if (tempLevel > 100)
            tempLevel = 100;

        if (isCharging())
            snprintf(batteryPercent, sizeof(batteryPercent), "+%d%%", tempLevel);
        else
            snprintf(batteryPercent, sizeof(batteryPercent), "%d%%", tempLevel);
        stringRecord(newSettings, "batteryPercent", batteryPercent);
    }

    // Add ECEF station data to the end of settings
    for (int index = 0; index < COMMON_COORDINATES_MAX_STATIONS; index++) // Arbitrary 50 station limit
    {
        // stationInfo example: LocationA,-1280206.568,-4716804.403,4086665.484
        char stationInfo[100];

        // Try SD, then LFS
        if (getFileLineSD(stationCoordinateECEFFileName, index, stationInfo, sizeof(stationInfo)) ==
            true) // fileName, lineNumber, array, arraySize
        {
            trim(stationInfo); // Remove trailing whitespace

            if (settings.debugWiFiConfig == true)
                systemPrintf("ECEF SD station %d - found: %s\r\n", index, stationInfo);

            replaceCharacter(stationInfo, ',', ' '); // Change all , to ' ' for easier parsing on the JS side
            snprintf(tagText, sizeof(tagText), "stationECEF%d", index);
            stringRecord(newSettings, tagText, stationInfo);
        }
        else if (getFileLineLFS(stationCoordinateECEFFileName, index, stationInfo, sizeof(stationInfo)) ==
                 true) // fileName, lineNumber, array, arraySize
        {
            trim(stationInfo); // Remove trailing whitespace

            if (settings.debugWiFiConfig == true)
                systemPrintf("ECEF LFS station %d - found: %s\r\n", index, stationInfo);

            replaceCharacter(stationInfo, ',', ' '); // Change all , to ' ' for easier parsing on the JS side
            snprintf(tagText, sizeof(tagText), "stationECEF%d", index);
            stringRecord(newSettings, tagText, stationInfo);
        }
        else
        {
            // We could not find this line
            break;
        }
    }

    // Add Geodetic station data to the end of settings
    for (int index = 0; index < COMMON_COORDINATES_MAX_STATIONS; index++) // Arbitrary 50 station limit
    {
        // stationInfo example: LocationA,40.09029479,-105.18505761,1560.089
        char stationInfo[100];

        // Try SD, then LFS
        if (getFileLineSD(stationCoordinateGeodeticFileName, index, stationInfo, sizeof(stationInfo)) ==
            true) // fileName, lineNumber, array, arraySize
        {
            trim(stationInfo); // Remove trailing whitespace

            if (settings.debugWiFiConfig == true)
                systemPrintf("Geo SD station %d - found: %s\r\n", index, stationInfo);

            replaceCharacter(stationInfo, ',', ' '); // Change all , to ' ' for easier parsing on the JS side
            snprintf(tagText, sizeof(tagText), "stationGeodetic%d", index);
            stringRecord(newSettings, tagText, stationInfo);
        }
        else if (getFileLineLFS(stationCoordinateGeodeticFileName, index, stationInfo, sizeof(stationInfo)) ==
                 true) // fileName, lineNumber, array, arraySize
        {
            trim(stationInfo); // Remove trailing whitespace

            if (settings.debugWiFiConfig == true)
                systemPrintf("Geo LFS station %d - found: %s\r\n", index, stationInfo);

            replaceCharacter(stationInfo, ',', ' '); // Change all , to ' ' for easier parsing on the JS side
            snprintf(tagText, sizeof(tagText), "stationGeodetic%d", index);
            stringRecord(newSettings, tagText, stationInfo);
        }
        else
        {
            // We could not find this line
            break;
        }
    }

    strcat(newSettings, "\0");
    systemPrintf("newSettings len: %d\r\n", strlen(newSettings));

    // systemPrintf("newSettings: %s\r\n", newSettings); // newSettings is >10k. Sending to systemPrint causes stack
    // overflow
    for (int x = 0; x < strlen(newSettings); x++) // Print manually
        systemWrite(newSettings[x]);

    systemPrintln();
}

// Add record with int
void stringRecord(char *settingsCSV, const char *id, int settingValue)
{
    char record[100];
    snprintf(record, sizeof(record), "%s,%d,", id, settingValue);
    strcat(settingsCSV, record);
}

// Add record with uint32_t
void stringRecord(char *settingsCSV, const char *id, uint32_t settingValue)
{
    char record[100];
    snprintf(record, sizeof(record), "%s,%d,", id, settingValue);
    strcat(settingsCSV, record);
}

// Add record with double
void stringRecord(char *settingsCSV, const char *id, double settingValue, int decimalPlaces)
{
    char format[10];
    snprintf(format, sizeof(format), "%%0.%dlf", decimalPlaces); // Create '%0.09lf'

    char formattedValue[20];
    snprintf(formattedValue, sizeof(formattedValue), format, settingValue);

    char record[100];
    snprintf(record, sizeof(record), "%s,%s,", id, formattedValue);
    strcat(settingsCSV, record);
}

// Add record with bool
void stringRecord(char *settingsCSV, const char *id, bool settingValue)
{
    char temp[10];
    if (settingValue == true)
        strcpy(temp, "true");
    else
        strcpy(temp, "false");

    char record[100];
    snprintf(record, sizeof(record), "%s,%s,", id, temp);
    strcat(settingsCSV, record);
}

// Add string. Provide your own commas!
void stringRecord(char *settingsCSV, const char *id)
{
    strcat(settingsCSV, id);
}

// Add record with string
void stringRecord(char *settingsCSV, const char *id, char *settingValue)
{
    char record[100];
    snprintf(record, sizeof(record), "%s,%s,", id, settingValue);
    strcat(settingsCSV, record);
}

// Add record with uint64_t
void stringRecord(char *settingsCSV, const char *id, uint64_t settingValue)
{
    char record[100];
    snprintf(record, sizeof(record), "%s,%lld,", id, settingValue);
    strcat(settingsCSV, record);
}

// Add record with int
void stringRecordN(char *settingsCSV, const char *id, int n, int settingValue)
{
    char record[100];
    snprintf(record, sizeof(record), "%s_%d,%d,", id, n, settingValue);
    strcat(settingsCSV, record);
}

// Add record with string
void stringRecordN(char *settingsCSV, const char *id, int n, char *settingValue)
{
    char record[100];
    snprintf(record, sizeof(record), "%s_%d,%s,", id, n, settingValue);
    strcat(settingsCSV, record);
}

void writeToString(char *settingValueStr, bool value)
{
    sprintf(settingValueStr, "%s", value ? "true" : "false");
}

void writeToString(char *settingValueStr, int value)
{
    sprintf(settingValueStr, "%d", value);
}
void writeToString(char *settingValueStr, uint64_t value)
{
    sprintf(settingValueStr, "%" PRIu64, value);
}

void writeToString(char *settingValueStr, uint32_t value)
{
    sprintf(settingValueStr, "%" PRIu32, value);
}

void writeToString(char *settingValueStr, double value)
{
    sprintf(settingValueStr, "%f", value);
}
void writeToString(char *settingValueStr, double value, int decimalPoints)
{
    sprintf(settingValueStr, "%0.*f", decimalPoints, value);
}

void writeToString(char *settingValueStr, char *value)
{
    strcpy(settingValueStr, value);
}

// Lookup table for a settingName
// Given a settingName, create a string with setting value
// Used in conjunction with the command line interface
// The order of variables matches the order found in settings.h
SettingValueResponse getSettingValue(const char *settingName, char *settingValueStr)
{
    int i;
    int qualifier;
    char suffix[51];
    char truncatedName[51];
    RTK_Settings_Types type;
    void *var;

    bool knownSetting = false;
    bool settingIsString = false; // Goes true when setting needs to be surrounded by quotes during command response.
                                  // Generally char arrays but some others.

    // Loop through the valid command entries
    i = commandLookupSettingName(settingName, truncatedName, sizeof(truncatedName), suffix, sizeof(suffix));

    // Determine if settingName is in the command table
    if (i >= 0)
    {
        qualifier = rtkSettingsEntries[i].qualifier;
        type = rtkSettingsEntries[i].type;
        var = rtkSettingsEntries[i].var;
        switch (type)
        {
        default:
            break;
        case _bool: {
            bool *ptr = (bool *)var;
            writeToString(settingValueStr, *ptr);
            knownSetting = true;
        }
        break;
        case _int: {
            int *ptr = (int *)var;
            writeToString(settingValueStr, *ptr);
            knownSetting = true;
        }
        break;
        case _float: {
            float *ptr = (float *)var;
            writeToString(settingValueStr, (double)*ptr, qualifier);
            knownSetting = true;
        }
        break;
        case _double: {
            double *ptr = (double *)var;
            writeToString(settingValueStr, *ptr, qualifier);
            knownSetting = true;
        }
        break;
        case _uint8_t: {
            uint8_t *ptr = (uint8_t *)var;
            writeToString(settingValueStr, (int)*ptr);
            knownSetting = true;
        }
        break;
        case _uint16_t: {
            uint16_t *ptr = (uint16_t *)var;
            writeToString(settingValueStr, (int)*ptr);
            knownSetting = true;
        }
        break;
        case _uint32_t: {
            uint32_t *ptr = (uint32_t *)var;
            writeToString(settingValueStr, *ptr);
            knownSetting = true;
        }
        break;
        case _uint64_t: {
            uint64_t *ptr = (uint64_t *)var;
            writeToString(settingValueStr, *ptr);
            knownSetting = true;
        }
        break;
        case _int8_t: {
            int8_t *ptr = (int8_t *)var;
            writeToString(settingValueStr, (int)*ptr);
            knownSetting = true;
        }
        break;
        case _int16_t: {
            int16_t *ptr = (int16_t *)var;
            writeToString(settingValueStr, (int)*ptr);
            knownSetting = true;
        }
        break;
        case tMuxConn: {
            muxConnectionType_e *ptr = (muxConnectionType_e *)var;
            writeToString(settingValueStr, (int)*ptr);
            knownSetting = true;
        }
        break;
        case tSysState: {
            SystemState *ptr = (SystemState *)var;
            writeToString(settingValueStr, (int)*ptr);
            knownSetting = true;
        }
        break;
        case tPulseEdg: {
            pulseEdgeType_e *ptr = (pulseEdgeType_e *)var;
            writeToString(settingValueStr, (int)*ptr);
            knownSetting = true;
        }
        break;
        case tBtRadio: {
            BluetoothRadioType_e *ptr = (BluetoothRadioType_e *)var;
            writeToString(settingValueStr, (int)*ptr);
            knownSetting = true;
        }
        break;
        case tPerDisp: {
            PeriodicDisplay_t *ptr = (PeriodicDisplay_t *)var;
            writeToString(settingValueStr, (int)*ptr);
            knownSetting = true;
        }
        break;
        case tCoordInp: {
            CoordinateInputType *ptr = (CoordinateInputType *)var;
            writeToString(settingValueStr, (int)*ptr);
            knownSetting = true;
        }
        break;
        case tCharArry: {
            char *ptr = (char *)var;
            writeToString(settingValueStr, ptr);
            knownSetting = true;
            settingIsString = true;
        }
        break;
        case _IPString: {
            IPAddress *ptr = (IPAddress *)var;
            writeToString(settingValueStr, (char *)ptr->toString().c_str());
            knownSetting = true;
            settingIsString = true;
        }
        break;
        case tUbxMsgRt: {
            for (int x = 0; x < qualifier; x++)
            {
                if ((suffix[0] == ubxMessages[x].msgTextName[0]) && (strcmp(suffix, ubxMessages[x].msgTextName) == 0))
                {
                    writeToString(settingValueStr, settings.ubxMessageRates[x]);
                    knownSetting = true;
                    break;
                }
            }
        }
        break;
        case tUbxConst: {
            for (int x = 0; x < qualifier; x++)
            {
                if ((suffix[0] == settings.ubxConstellations[x].textName[0]) &&
                    (strcmp(suffix, settings.ubxConstellations[x].textName) == 0))
                {
                    writeToString(settingValueStr, settings.ubxConstellations[x].enabled);
                    knownSetting = true;
                    break;
                }
            }
        }
        break;
        case tEspNowPr: {
            int suffixNum;
            if (sscanf(suffix, "%d", &suffixNum) == 1)
            {
                char peer[18];
                snprintf(peer, sizeof(peer), "%X:%X:%X:%X:%X:%X", settings.espnowPeers[suffixNum][0],
                         settings.espnowPeers[suffixNum][1], settings.espnowPeers[suffixNum][2],
                         settings.espnowPeers[suffixNum][3], settings.espnowPeers[suffixNum][4],
                         settings.espnowPeers[suffixNum][5]);
                writeToString(settingValueStr, peer);
                knownSetting = true;
                settingIsString = true;
            }
        }
        break;
        case tUbMsgRtb: {
            int firstRTCMRecord = zedGetMessageNumberByName("UBX_RTCM_1005");

            for (int x = 0; x < qualifier; x++)
            {
                if ((suffix[0] == ubxMessages[firstRTCMRecord + x].msgTextName[0]) &&
                    (strcmp(suffix, ubxMessages[firstRTCMRecord + x].msgTextName) == 0))
                {
                    writeToString(settingValueStr, settings.ubxMessageRatesBase[x]);
                    knownSetting = true;
                    break;
                }
            }
        }
        break;
        case tWiFiNet: {
            int network;

            if (strstr(suffix, "SSID") != nullptr)
            {
                if (sscanf(suffix, "%dSSID", &network) == 1)
                {
                    writeToString(settingValueStr, settings.wifiNetworks[network].ssid);
                    knownSetting = true;
                    settingIsString = true;
                }
            }
            else if (strstr(suffix, "Password") != nullptr)
            {
                if (sscanf(suffix, "%dPassword", &network) == 1)
                {
                    writeToString(settingValueStr, settings.wifiNetworks[network].password);
                    knownSetting = true;
                    settingIsString = true;
                }
            }
        }
        break;
        case tNSCHost: {
            int server;
            if (sscanf(suffix, "%d", &server) == 1)
            {
                writeToString(settingValueStr, settings.ntripServer_CasterHost[server]);
                knownSetting = true;
                settingIsString = true;
            }
        }
        break;
        case tNSCPort: {
            int server;
            if (sscanf(suffix, "%d", &server) == 1)
            {
                writeToString(settingValueStr, settings.ntripServer_CasterPort[server]);
                knownSetting = true;
            }
        }
        break;
        case tNSCUser: {
            int server;
            if (sscanf(suffix, "%d", &server) == 1)
            {
                writeToString(settingValueStr, settings.ntripServer_CasterUser[server]);
                knownSetting = true;
                settingIsString = true;
            }
        }
        break;
        case tNSCUsrPw: {
            int server;
            if (sscanf(suffix, "%d", &server) == 1)
            {
                writeToString(settingValueStr, settings.ntripServer_CasterUserPW[server]);
                knownSetting = true;
                settingIsString = true;
            }
        }
        break;
        case tNSMtPt: {
            int server;
            if (sscanf(suffix, "%d", &server) == 1)
            {
                writeToString(settingValueStr, settings.ntripServer_MountPoint[server]);
                knownSetting = true;
                settingIsString = true;
            }
        }
        break;
        case tNSMtPtPw: {
            int server;
            if (sscanf(suffix, "%d", &server) == 1)
            {
                writeToString(settingValueStr, settings.ntripServer_MountPointPW[server]);
                knownSetting = true;
                settingIsString = true;
            }
        }
        break;
        case tUmMRNmea: {
            for (int x = 0; x < qualifier; x++)
            {
                if ((suffix[0] == umMessagesNMEA[x].msgTextName[0]) &&
                    (strcmp(suffix, umMessagesNMEA[x].msgTextName) == 0))
                {
                    writeToString(settingValueStr, settings.um980MessageRatesNMEA[x]);
                    knownSetting = true;
                    break;
                }
            }
        }
        break;
        case tUmMRRvRT: {
            for (int x = 0; x < qualifier; x++)
            {
                if ((suffix[0] == umMessagesRTCM[x].msgTextName[0]) &&
                    (strcmp(suffix, umMessagesRTCM[x].msgTextName) == 0))
                {
                    writeToString(settingValueStr, settings.um980MessageRatesRTCMRover[x]);
                    knownSetting = true;
                    break;
                }
            }
        }
        break;
        case tUmMRBaRT: {
            for (int x = 0; x < qualifier; x++)
            {
                if ((suffix[0] == umMessagesRTCM[x].msgTextName[0]) &&
                    (strcmp(suffix, umMessagesRTCM[x].msgTextName) == 0))
                {
                    writeToString(settingValueStr, settings.um980MessageRatesRTCMBase[x]);
                    knownSetting = true;
                    break;
                }
            }
        }
        break;
        case tUmConst: {
            for (int x = 0; x < qualifier; x++)
            {
                if ((suffix[0] == um980ConstellationCommands[x].textName[0]) &&
                    (strcmp(suffix, um980ConstellationCommands[x].textName) == 0))
                {
                    writeToString(settingValueStr, settings.um980Constellations[x]);
                    knownSetting = true;
                    break;
                }
            }
        }
        break;
        case tCorrSPri: {
            for (int x = 0; x < qualifier; x++)
            {
                if ((suffix[0] == correctionsSourceNames[x][0]) && (strcmp(suffix, correctionsSourceNames[x]) == 0))
                {
                    writeToString(settingValueStr, settings.correctionsSourcesPriority[x]);
                    knownSetting = true;
                    break;
                }
            }
        }
        break;
        case tRegCorTp: {
            int region;
            if (sscanf(suffix, "%d", &region) == 1)
            {
                writeToString(settingValueStr, settings.regionalCorrectionTopics[region]);
                knownSetting = true;
                settingIsString = true;
            }
        }
        break;
        }
    }

    // Done if the settingName was found
    if (knownSetting == true && settingIsString == true)
        return (SETTING_KNOWN_STRING);
    else if (knownSetting == true)
        return (SETTING_KNOWN);

    // Report deviceID over CLI - Useful for label generation
    if (strcmp(settingName, "deviceId") == 0)
    {
        char hardwareID[15];
        snprintf(hardwareID, sizeof(hardwareID), "%02X%02X%02X%02X%02X%02X%02X", btMACAddress[0], btMACAddress[1],
                 btMACAddress[2], btMACAddress[3], btMACAddress[4], btMACAddress[5], productVariant);

        writeToString(settingValueStr, hardwareID);
        knownSetting = true;
        settingIsString = true;
    }

    // Special actions
    else if (strcmp(settingName, "enableRCFirmware") == 0)
    {
        writeToString(settingValueStr, enableRCFirmware);
        knownSetting = true;
    }

    // Unused variables - read to avoid errors
    // TODO: check this! Is this really what we want?
    else
    {
        const char *table[] = {
            "baseTypeFixed",
            "baseTypeSurveyIn",
            "checkNewFirmware",
            "enableFactoryDefaults",
            "enableFirmwareUpdate",
            "enableForgetRadios",
            "exitAndReset",
            "factoryDefaultReset",
            "fileSelectAll",
            "firmwareFileName",
            "fixedBaseCoordinateTypeECEF",
            "fixedBaseCoordinateTypeGeo",
            "fixedHAEAPC",
            "fixedLatText",
            "fixedLongText",
            "forgetEspNowPeers",
            "getNewFirmware",
            "measurementRateHz",
            "measurementRateSec",
            "minCNO",
            "nicknameECEF",
            "nicknameGeodetic",
            "resetProfile",
            "saveToArduino",
            "setProfile",
            "startNewLog",
            "stationGeodetic",
        };
        const int tableEntries = sizeof(table) / sizeof(table[0]);

        knownSetting = commandCheckForUnknownVariable(settingName, table, tableEntries);
    }

    if (knownSetting == false)
    {
        if (settings.debugWiFiConfig)
            systemPrintf("getSettingValue() Unknown setting: %s\r\n", settingName);
    }

    if (knownSetting == true && settingIsString == true)
        return (SETTING_KNOWN_STRING);
    else if (knownSetting == true)
        return (SETTING_KNOWN);

    return (SETTING_UNKNOWN);
}

// List available settings, their type in CSV, and value
// See issue: https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/issues/190
void commandList(int i)
{
    char settingName[100];
    char settingType[100];
    char settingValue[100];

    switch (rtkSettingsEntries[i].type)
    {
    default:
        break;
    case _bool: {
        getSettingValue(rtkSettingsEntries[i].name, settingValue);
        commandSendExecuteListResponse(rtkSettingsEntries[i].name, "bool", settingValue);
    }
    break;
    case _int: {
        getSettingValue(rtkSettingsEntries[i].name, settingValue);
        commandSendExecuteListResponse(rtkSettingsEntries[i].name, "int", settingValue);
    }
    break;
    case _float: {
        getSettingValue(rtkSettingsEntries[i].name, settingValue);
        commandSendExecuteListResponse(rtkSettingsEntries[i].name, "float", settingValue);
    }
    break;
    case _double: {
        getSettingValue(rtkSettingsEntries[i].name, settingValue);
        commandSendExecuteListResponse(rtkSettingsEntries[i].name, "double", settingValue);
    }
    break;
    case _uint8_t: {
        getSettingValue(rtkSettingsEntries[i].name, settingValue);
        commandSendExecuteListResponse(rtkSettingsEntries[i].name, "uint8_t", settingValue);
    }
    break;
    case _uint16_t: {
        getSettingValue(rtkSettingsEntries[i].name, settingValue);
        commandSendExecuteListResponse(rtkSettingsEntries[i].name, "uint16_t", settingValue);
    }
    break;
    case _uint32_t: {
        getSettingValue(rtkSettingsEntries[i].name, settingValue);
        commandSendExecuteListResponse(rtkSettingsEntries[i].name, "uint32_t", settingValue);
    }
    break;
    case _uint64_t: {
        getSettingValue(rtkSettingsEntries[i].name, settingValue);
        commandSendExecuteListResponse(rtkSettingsEntries[i].name, "uint64_t", settingValue);
    }
    break;
    case _int8_t: {
        getSettingValue(rtkSettingsEntries[i].name, settingValue);
        commandSendExecuteListResponse(rtkSettingsEntries[i].name, "int8_t", settingValue);
    }
    break;
    case _int16_t: {
        getSettingValue(rtkSettingsEntries[i].name, settingValue);
        commandSendExecuteListResponse(rtkSettingsEntries[i].name, "int16_t", settingValue);
    }
    break;
    case tMuxConn: {
        getSettingValue(rtkSettingsEntries[i].name, settingValue);
        commandSendExecuteListResponse(rtkSettingsEntries[i].name, "muxConnectionType_e", settingValue);
    }
    break;
    case tSysState: {
        getSettingValue(rtkSettingsEntries[i].name, settingValue);
        commandSendExecuteListResponse(rtkSettingsEntries[i].name, "SystemState", settingValue);
    }
    break;
    case tPulseEdg: {
        getSettingValue(rtkSettingsEntries[i].name, settingValue);
        commandSendExecuteListResponse(rtkSettingsEntries[i].name, "pulseEdgeType_e", settingValue);
    }
    break;
    case tBtRadio: {
        getSettingValue(rtkSettingsEntries[i].name, settingValue);
        commandSendExecuteListResponse(rtkSettingsEntries[i].name, "BluetoothRadioType_e", settingValue);
    }
    break;
    case tPerDisp: {
        getSettingValue(rtkSettingsEntries[i].name, settingValue);
        commandSendExecuteListResponse(rtkSettingsEntries[i].name, "PeriodicDisplay_t", settingValue);
    }
    break;
    case tCoordInp: {
        getSettingValue(rtkSettingsEntries[i].name, settingValue);
        commandSendExecuteListResponse(rtkSettingsEntries[i].name, "CoordinateInputType", settingValue);
    }
    break;
    case tCharArry: {
        snprintf(settingType, sizeof(settingType), "char[%d]", rtkSettingsEntries[i].qualifier);
        getSettingValue(rtkSettingsEntries[i].name, settingValue);
        commandSendExecuteListResponse(rtkSettingsEntries[i].name, settingType, settingValue);
    }
    break;
    case _IPString: {
        getSettingValue(rtkSettingsEntries[i].name, settingValue);
        commandSendExecuteListResponse(rtkSettingsEntries[i].name, "IPAddress", settingValue);
    }
    break;
    case tUbxMsgRt: {
        // Record message settings
        for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
        {
            snprintf(settingName, sizeof(settingName), "%s%s", rtkSettingsEntries[i].name, ubxMessages[x].msgTextName);

            getSettingValue(settingName, settingValue);
            commandSendExecuteListResponse(settingName, "uint8_t", settingValue);
        }
    }
    break;
    case tUbxConst: {
        // Record constellation settings
        for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
        {
            snprintf(settingName, sizeof(settingName), "%s%s", rtkSettingsEntries[i].name,
                     settings.ubxConstellations[x].textName);

            getSettingValue(settingName, settingValue);
            commandSendExecuteListResponse(settingName, "bool", settingValue);
        }
    }
    break;
    case tEspNowPr: {
        // Record ESP-Now peer MAC addresses
        for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
        {
            snprintf(settingType, sizeof(settingType), "uint8_t[%d]", sizeof(settings.espnowPeers[0]));
            snprintf(settingName, sizeof(settingName), "%s%d", rtkSettingsEntries[i].name, x);

            getSettingValue(settingName, settingValue);
            commandSendExecuteListResponse(settingName, settingType, settingValue);
        }
    }
    break;
    case tUbMsgRtb: {
        // Record message settings
        int firstRTCMRecord = zedGetMessageNumberByName("UBX_RTCM_1005");

        for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
        {
            snprintf(settingName, sizeof(settingName), "%s%s", rtkSettingsEntries[i].name,
                     ubxMessages[firstRTCMRecord + x].msgTextName);

            getSettingValue(settingName, settingValue);
            commandSendExecuteListResponse(settingName, "uint8_t", settingValue);
        }
    }
    break;
    case tWiFiNet: {
        // Record WiFi credential table
        for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
        {
            snprintf(settingType, sizeof(settingType), "char[%d]", sizeof(settings.wifiNetworks[0].password));
            snprintf(settingName, sizeof(settingName), "%s%dPassword", rtkSettingsEntries[i].name, x);

            getSettingValue(settingName, settingValue);
            commandSendExecuteListResponse(settingName, settingType, settingValue);

            snprintf(settingType, sizeof(settingType), "char[%d]", sizeof(settings.wifiNetworks[0].ssid));
            snprintf(settingName, sizeof(settingName), "%s%dSSID", rtkSettingsEntries[i].name, x);

            getSettingValue(settingName, settingValue);
            commandSendExecuteListResponse(settingName, settingType, settingValue);
        }
    }
    break;
    case tNSCHost: {
        for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
        {
            snprintf(settingType, sizeof(settingType), "char[%d]", sizeof(settings.ntripServer_CasterHost[x]));
            snprintf(settingName, sizeof(settingName), "%s%d", rtkSettingsEntries[i].name, x);

            getSettingValue(settingName, settingValue);
            commandSendExecuteListResponse(settingName, settingType, settingValue);
        }
    }
    break;
    case tNSCPort: {
        for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
        {
            snprintf(settingName, sizeof(settingName), "%s%d", rtkSettingsEntries[i].name, x);

            getSettingValue(settingName, settingValue);
            commandSendExecuteListResponse(settingName, "uint16_t", settingValue);
        }
    }
    break;
    case tNSCUser: {
        for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
        {
            snprintf(settingType, sizeof(settingType), "char[%d]", sizeof(settings.ntripServer_CasterUser[x]));
            snprintf(settingName, sizeof(settingName), "%s%d", rtkSettingsEntries[i].name, x);

            getSettingValue(settingName, settingValue);
            commandSendExecuteListResponse(settingName, settingType, settingValue);
        }
    }
    break;
    case tNSCUsrPw: {
        for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
        {
            snprintf(settingType, sizeof(settingType), "char[%d]", sizeof(settings.ntripServer_CasterUserPW[x]));
            snprintf(settingName, sizeof(settingName), "%s%d", rtkSettingsEntries[i].name, x);

            getSettingValue(settingName, settingValue);
            commandSendExecuteListResponse(settingName, settingType, settingValue);
        }
    }
    break;
    case tNSMtPt: {
        for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
        {
            snprintf(settingType, sizeof(settingType), "char[%d]", sizeof(settings.ntripServer_MountPoint[x]));
            snprintf(settingName, sizeof(settingName), "%s%d", rtkSettingsEntries[i].name, x);

            getSettingValue(settingName, settingValue);
            commandSendExecuteListResponse(settingName, settingType, settingValue);
        }
    }
    break;
    case tNSMtPtPw: {
        for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
        {
            snprintf(settingType, sizeof(settingType), "char[%d]", sizeof(settings.ntripServer_MountPointPW[x]));
            snprintf(settingName, sizeof(settingName), "%s%d", rtkSettingsEntries[i].name, x);

            getSettingValue(settingName, settingValue);
            commandSendExecuteListResponse(settingName, settingType, settingValue);
        }
    }
    break;
    case tUmMRNmea: {
        // Record UM980 NMEA rates
        for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
        {
            snprintf(settingName, sizeof(settingName), "%s%s", rtkSettingsEntries[i].name,
                     umMessagesNMEA[x].msgTextName);

            getSettingValue(settingName, settingValue);
            commandSendExecuteListResponse(settingName, "float", settingValue);
        }
    }
    break;
    case tUmMRRvRT: {
        // Record UM980 Rover RTCM rates
        for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
        {
            snprintf(settingName, sizeof(settingName), "%s%s", rtkSettingsEntries[i].name,
                     umMessagesRTCM[x].msgTextName);

            getSettingValue(settingName, settingValue);
            commandSendExecuteListResponse(settingName, "float", settingValue);
        }
    }
    break;
    case tUmMRBaRT: {
        // Record UM980 Base RTCM rates
        for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
        {
            snprintf(settingName, sizeof(settingName), "%s%s", rtkSettingsEntries[i].name,
                     umMessagesRTCM[x].msgTextName);

            getSettingValue(settingName, settingValue);
            commandSendExecuteListResponse(settingName, "float", settingValue);
        }
    }
    break;
    case tUmConst: {
        // Record UM980 Constellations
        for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
        {
            snprintf(settingName, sizeof(settingName), "%s%s", rtkSettingsEntries[i].name,
                     um980ConstellationCommands[x].textName);

            getSettingValue(settingName, settingValue);
            commandSendExecuteListResponse(settingName, "uint8_t", settingValue);
        }
    }
    break;
    case tCorrSPri: {
        // Record corrections priorities
        for (int x = 0; x < rtkSettingsEntries[i].qualifier; x++)
        {
            snprintf(settingName, sizeof(settingName), "%s%s", rtkSettingsEntries[i].name, correctionsSourceNames[x]);

            getSettingValue(settingName, settingValue);
            commandSendExecuteListResponse(settingName, "int", settingValue);
        }
    }
    break;
    case tRegCorTp: {
        for (int r = 0; r < rtkSettingsEntries[i].qualifier; r++)
        {
            snprintf(settingType, sizeof(settingType), "char[%d]", sizeof(settings.regionalCorrectionTopics[0]));
            snprintf(settingName, sizeof(settingName), "%s%d", rtkSettingsEntries[i].name, r);

            getSettingValue(settingName, settingValue);
            commandSendExecuteListResponse(settingName, settingType, settingValue);
        }
    }
    break;
    }
}

const char *commandGetName(int stringIndex, int rtkIndex)
{
    int number;
    static char temp[2][16];

    // Display the command from rtkSettingsEntries
    if (rtkIndex >= 0)
        return rtkSettingsEntries[rtkIndex].name;

    // Display the profile name - used in Profiles
    else if (rtkIndex >= -MAX_PROFILE_COUNT)
    {
        number = -rtkIndex - 1;
        snprintf(&temp[stringIndex][0], sizeof(temp[0]), "profile%dName", number);
        return &temp[stringIndex][0];
    }

    // Display the current Profile Number - used in Profiles
    else if (rtkIndex == COMMAND_PROFILE_NUMBER)
        return "profileNumber";

    // Display the device ID - used in PointPerfect
    return "deviceId";
}

// Determine if the command is available on this platform
bool commandAvailableOnPlatform(int i)
{
    do
    {
        // Determine if this command is supported by the command processor
        if (rtkSettingsEntries[i].inCommands)
        {
            // Verify that the command is available on the platform
            if ((productVariant == RTK_EVK) && rtkSettingsEntries[i].platEvk)
                break;
            if ((productVariant == RTK_FACET_V2) && rtkSettingsEntries[i].platFacetV2)
                break;
            if ((productVariant == RTK_FACET_MOSAIC) && rtkSettingsEntries[i].platFacetMosaic)
                break;
            if ((productVariant == RTK_TORCH) && rtkSettingsEntries[i].platTorch)
                break;
        }
        return false;
    } while (0);
    return true;
}

// Determine if the setting is available on this platform
bool settingAvailableOnPlatform(int i)
{
    if ((productVariant == RTK_EVK) && rtkSettingsEntries[i].platEvk)
        return (true);
    if ((productVariant == RTK_FACET_V2) && rtkSettingsEntries[i].platFacetV2)
        return (true);
    if ((productVariant == RTK_FACET_MOSAIC) && rtkSettingsEntries[i].platFacetMosaic)
        return (true);
    if ((productVariant == RTK_TORCH) && rtkSettingsEntries[i].platTorch)
        return (true);
    return false;
}

// Allocate and fill the commandIndex table
bool commandIndexFill()
{
    int i;
    const char *iCommandName;
    int j;
    const char *jCommandName;
    int length;
    int16_t temp;

    // Count the commands
    commandCount = 0;
    for (i = 0; i < numRtkSettingsEntries; i++)
    {
        if (commandAvailableOnPlatform(i))
            commandCount += 1;
    }
    commandCount += COMMAND_COUNT - 1;

    // Allocate the command array
    length = commandCount * sizeof(*commandIndex);
    commandIndex = (int16_t *)malloc(length);
    if (!commandIndex)
    {
        // Failed to allocate the commandIndex
        systemPrintln("ERROR: Failed to allocate commandIndex!");
        return false;
    }

    // Initialize commandIndex with index values into rtkSettingsEntries
    commandCount = 0;
    for (i = 0; i < numRtkSettingsEntries; i++)
        if (commandAvailableOnPlatform(i))
            commandIndex[commandCount++] = i;

    // Add the man-machine interface commands to the list
    for (i = 1; i < COMMAND_COUNT; i++)
        commandIndex[commandCount++] = -i;

    // Sort the commands
    for (i = 0; i < commandCount - 1; i++)
    {
        iCommandName = commandGetName(0, commandIndex[i]);
        for (j = i + 1; j < commandCount; j++)
        {
            jCommandName = commandGetName(1, commandIndex[j]);

            // Determine if the commands are out of order
            if (strncasecmp(iCommandName, jCommandName, strlen(iCommandName) + 1) > 0)
            {
                // Out of order, switch the two entries
                temp = commandIndex[i];
                commandIndex[i] = commandIndex[j];
                commandIndex[j] = temp;

                // Get the updated names
                iCommandName = commandGetName(0, commandIndex[i]);
                jCommandName = commandGetName(1, commandIndex[j]);
            }
        }
    }
    return true;
}

// List available settings, their type in CSV, and value
void printAvailableSettings()
{
    // Display the commands
    for (int i = 0; i < commandCount; i++)
    {
        // Display the command from rtkSettingsEntries
        if (commandIndex[i] >= 0)
            commandList(commandIndex[i]);

        // Below are commands formed specifically for the Man-Machine-Interface
        // Display the profile name - used in Profiles
        else if (commandIndex[i] >= -MAX_PROFILE_COUNT)
        {
            int index;
            const char *settingName;
            char settingType[100];

            settingName = commandGetName(0, commandIndex[i]);
            index = -commandIndex[i] - 1;
            snprintf(settingType, sizeof(settingType), "char[%d]", sizeof(profileNames[0]));
            commandSendExecuteListResponse(settingName, settingType, profileNames[index]);
        }

        // Display the current Profile Number - used in Profiles
        else if (commandIndex[i] == COMMAND_PROFILE_NUMBER)
        {
            char settingValue[100];

            snprintf(settingValue, sizeof(settingValue), "%d", profileNumber);
            commandSendExecuteListResponse("profileNumber", "uint8_t", settingValue);
        }

        // Display the device ID - used in PointPerfect
        else if (commandIndex[i] == COMMAND_DEVICE_ID)
        {
            char hardwareID[15];
            char settingType[100];

            snprintf(hardwareID, sizeof(hardwareID), "%02X%02X%02X%02X%02X%02X%02X", btMACAddress[0], btMACAddress[1],
                     btMACAddress[2], btMACAddress[3], btMACAddress[4], btMACAddress[5], productVariant);
            snprintf(settingType, sizeof(settingType), "char[%d]", sizeof(hardwareID));
            commandSendExecuteListResponse("deviceId", settingType, hardwareID);
        }
    }
    systemPrintln();
}
