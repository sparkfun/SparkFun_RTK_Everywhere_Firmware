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



