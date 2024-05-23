// Set the priority of all correction sources
// Note: this sets the priority of all possible sources, not just the ones available / in use
void menuCorrectionsPriorities()
{
    if (!validateCorrectionPriorities())
    {
        systemPrintln();
        systemPrintln("Menu: Corrections Priorities");

        systemPrintln();
        systemPrintln("Corrections priorities are invalid. Restoring the defaults");

        initializeCorrectionPriorities();
    }

    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: Corrections Priorities");

        systemPrint("1) Correction source lifetime in seconds: ");
        systemPrintln(settings.correctionsSourcesLifetime_s);

        systemPrintln();
        systemPrintln("These are the correction sources in order of decreasing priority");
        systemPrintln("Enter the upper case letter to increase the correction priority");
        systemPrintln("Enter the lower case letter to decrease the correction priority");
        systemPrintln();

        // Priority 0 is the highest
        for (int x = 0; x < correctionsSource::CORR_NUM; x++)
        {
            for (int y = 0; y < correctionsSource::CORR_NUM; y++)
            {
                if (settings.correctionsSourcesPriority[y] == x)
                {
                    char lower = 'a' + y;
                    char upper = 'A' + y;
                    systemPrintf("%c / %c) %s", upper, lower, correctionsSourceNames[y]);
                    systemPrintln();
                    break;
                }
            }
        }

        systemPrintln();
        systemPrintln("x) Exit");

        byte incoming = getUserInputCharacterNumber();

        if (incoming == 1)
        {
            getNewSetting("Enter new correction source lifetime in seconds (5-120): ", 5, 120,
                          &settings.correctionsSourcesLifetime_s);
        }
        else if ((incoming >= 'a') && (incoming < ('a' + correctionsSource::CORR_NUM)))
        {
            // Decrease priority - swap the selected correction source with the next lowest
            // But only if not already the lowest priority
            if (settings.correctionsSourcesPriority[(int)(incoming - 'a')] < (correctionsSource::CORR_NUM - 1))
            {
                int makeMeHigher = -1;
                for (int x = 0; x < correctionsSource::CORR_NUM; x++)
                {
                    if (settings.correctionsSourcesPriority[x] ==
                        settings.correctionsSourcesPriority[(int)(incoming - 'a')] + 1)
                    {
                        makeMeHigher = x;
                        break;
                    }
                }
                if (makeMeHigher >= 0)
                {
                    settings.correctionsSourcesPriority[(int)(incoming - 'a')] += 1; // Decrease
                    settings.correctionsSourcesPriority[makeMeHigher] -= 1;          // Increase
                }
            }
        }
        else if ((incoming >= 'A') && (incoming < ('A' + correctionsSource::CORR_NUM)))
        {
            // Increase priority - swap the selected correction source with the next highest
            // But only if not already priority 0 (highest)
            if (settings.correctionsSourcesPriority[(int)(incoming - 'A')] > 0)
            {
                int makeMeLower = -1;
                for (int x = 0; x < correctionsSource::CORR_NUM; x++)
                {
                    if (settings.correctionsSourcesPriority[x] ==
                        settings.correctionsSourcesPriority[(int)(incoming - 'A')] - 1)
                    {
                        makeMeLower = x;
                        break;
                    }
                }
                if (makeMeLower >= 0)
                {
                    settings.correctionsSourcesPriority[(int)(incoming - 'A')] -= 1; // Increase
                    settings.correctionsSourcesPriority[makeMeLower] += 1;           // Decrease
                }
            }
        }

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

bool validateCorrectionPriorities()
{
    // Check priorities have been initialized
    if (settings.correctionsSourcesPriority[0] == -1)
        return false;

    // Validate correction priorities by checking each appears only once
    int foundSources[correctionsSource::CORR_NUM];
    for (int x = 0; x < correctionsSource::CORR_NUM; x++)
        foundSources[x] = 0;
    for (int x = 0; x < correctionsSource::CORR_NUM; x++)
    {
        for (int y = 0; y < correctionsSource::CORR_NUM; y++)
        {
            if (settings.correctionsSourcesPriority[y] == x)
            {
                foundSources[x]++;
                break;
            }
        }
    }
    for (int x = 0; x < correctionsSource::CORR_NUM; x++)
        if (foundSources[x] != 1)
            return false;
    return true;
}

void initializeCorrectionPriorities()
{
    for (int s = 0; s < correctionsSource::CORR_NUM; s++)
        settings.correctionsSourcesPriority[s] = s;
}

// Corrections Priorities Support

void clearRegisteredCorrectionsSources()
{
    registeredCorrectionsSources.clear();
}

void registerCorrectionsSource(correctionsSource newSource)
{
    registeredCorrectionsSource it;
    it.source = newSource;
    it.lastSeen = millis();
    registeredCorrectionsSources.push_back(it);
}

bool isAResisteredCorrectionsSource(correctionsSource theSource)
{
    for (auto it = registeredCorrectionsSources.begin(); it != registeredCorrectionsSources.end(); it = std::next(it))
    {
        if (it->source == theSource)
            return true; // Return true if theSource is found in the vector of registered sources
    }
    return false;
}

bool isHighestRegisteredCorrectionsSource(correctionsSource theSource)
{
    int highestPriority = (int)CORR_NUM;

    // Find the registered correction source with the highest priority
    for (auto it = registeredCorrectionsSources.begin(); it != registeredCorrectionsSources.end(); it = std::next(it))
    {
        if (settings.correctionsSourcesPriority[(int)it->source] < highestPriority) // Higher
            highestPriority = settings.correctionsSourcesPriority[(int)it->source];
    }

    if (highestPriority == (int)CORR_NUM)
        return false; // This is actually an error. No sources were found...

    return (settings.correctionsSourcesPriority[(int)theSource] <=
            highestPriority); // Return true if theSource is highest
}

// Update when this corrections source was lastSeen. (Re)register it if required.
void updateCorrectionsLastSeen(correctionsSource theSource)
{
    if (theSource >= CORR_NUM)
    {
        systemPrintln("ERROR : updateCorrectionsLastSeen invalid corrections source");
        return;
    }

    if (!isAResisteredCorrectionsSource(theSource))
    {
        registerCorrectionsSource(theSource);
        return; // registerCorrectionsSource updates lastSeen. We can return
    }

    for (auto it = registeredCorrectionsSources.begin(); it != registeredCorrectionsSources.end(); it = std::next(it))
    {
        if (it->source == theSource)
        {
            it->lastSeen = millis();
            return;
        }
    }
}

// Check when each corrections source was lastSeen. Erase any that have expired
void checkRegisteredCorrectionsSources()
{
    for (auto it = registeredCorrectionsSources.begin(); it != registeredCorrectionsSources.end(); it = std::next(it))
    {
        if ((millis() - (settings.correctionsSourcesLifetime_s * 1000)) > it->lastSeen)
        {
            registeredCorrectionsSources.erase(it);
            it = std::prev(it);
        }
    }
}
