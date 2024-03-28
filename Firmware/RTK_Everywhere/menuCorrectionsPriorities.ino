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

        systemPrintln();
        systemPrintln("These are the correction sources in order of decreasing priority");
        systemPrintln("Enter the lower case letter to decrease the correction priority");
        systemPrintln("Enter the upper case letter to increase the correction priority");
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
                    systemPrintf("%c%c) %s",lower, upper, correctionsSourceNames[y]);
                    systemPrintln();
                    break;
                }
            }
        }

        systemPrintln();
        systemPrintln("x) Exit");

        byte incoming = getUserInputCharacterNumber();

        if ((incoming >= 'a') && (incoming < ('a' + correctionsSource::CORR_NUM)))
        {
            // Decrease priority - swap the selected correction source with the next lowest
            // But only if not already the lowest priority
            if (settings.correctionsSourcesPriority[(int)(incoming - 'a')] < (correctionsSource::CORR_NUM - 1))
            {
                int makeMeHigher;
                for (int x = 0; x < correctionsSource::CORR_NUM; x++)
                {
                    if (settings.correctionsSourcesPriority[x] == settings.correctionsSourcesPriority[(int)(incoming - 'a')] + 1)
                    {
                        makeMeHigher = x;
                        break;
                    }
                }
                settings.correctionsSourcesPriority[(int)(incoming - 'a')] += 1; // Decrease
                settings.correctionsSourcesPriority[makeMeHigher] -= 1; // Increase
            }
        }
        else if ((incoming >= 'A') && (incoming < ('A' + correctionsSource::CORR_NUM)))
        {
            // Increase priority - swap the selected correction source with the next highest
            // But only if not already priority 0 (highest)
            if (settings.correctionsSourcesPriority[(int)(incoming - 'A')] > 0)
            {
                int makeMeLower;
                for (int x = 0; x < correctionsSource::CORR_NUM; x++)
                {
                    if (settings.correctionsSourcesPriority[x] == settings.correctionsSourcesPriority[(int)(incoming - 'A')] - 1)
                    {
                        makeMeLower = x;
                        break;
                    }
                }
                settings.correctionsSourcesPriority[(int)(incoming - 'A')] -= 1; // Increase
                settings.correctionsSourcesPriority[makeMeLower] += 1; // Decrease
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
    // TODO: this should probably be somewhere else. But not sure where...?
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

typedef struct {
    correctionsSource source;
    unsigned long lastSeen;
} registeredCorrectionsSource;

// Corrections priority
std::vector<registeredCorrectionsSource> registeredCorrectionsSources; // vector (linked list) of registered corrections sources for this device

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
        registeredCorrectionsSource aSource = *it;

        if (aSource.source == theSource)
            return true;
    }
    return false;
}

bool isHighestRegisteredCorrectionsSource(correctionsSource theSource)
{
    correctionsSource highestSource = CORR_NUM;

    for (auto it = registeredCorrectionsSources.begin(); it != registeredCorrectionsSources.end(); it = std::next(it))
    {
        registeredCorrectionsSource aSource = *it;

        if (settings.correctionsSourcesPriority[aSource.source] < highestSource) // Higher
            highestSource = aSource.source;
    }

    if (highestSource == CORR_NUM)
        return false; // This is actually an error. No sources were found...

    return (settings.correctionsSourcesPriority[theSource] <= highestSource); // Highest
}

void updateCorrectionsLastSeen(correctionsSource theSource)
{
    if (!isAResisteredCorrectionsSource(theSource))
    {
        registerCorrectionsSource(theSource);
        return; // registerCorrectionsSource updates lastSeen. We can return
    }

    for (auto it = registeredCorrectionsSources.begin(); it != registeredCorrectionsSources.end(); it = std::next(it))
    {
        registeredCorrectionsSource aSource = *it;

        if (aSource.source == theSource)
        {
            aSource.lastSeen = millis();
            break;
        }
    }
}

void checkRegisteredCorrectionsSources()
{
    // TODO: check when each corrections source was lastSeen. Delete any that have expired
}
