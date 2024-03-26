// Set the baud rates for the radio and data ports
void menuCorrectionsPriorities()
{
    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: Corrections Priorities");

        systemPrintln();
        systemPrintln("These are the correction sources in order of decreasing priority");
        systemPrintln("Enter the lower case letter to decrease the correction priority");
        systemPrintln("Enter the upper case letter to increase the correction priority");
        systemPrintln();
        
        char lower = 'a';
        for (int x = 0; x < correctionsSource::CORR_NUM; x++)
        {
            for (int y = 0; y < correctionsSource::CORR_NUM; y++)
            {
                if (settings.correctionsSourcesPriority[y] == x)
                {
                    if (x < (correctionsSource::CORR_NUM - 1))
                        systemPrintf("%c",lower);
                    else
                        systemPrint(" "); // Can't decrease the lowest
                    if (x > 0)
                        systemPrintf("%c",lower + 'A' - 'a');
                    else
                        systemPrint(" "); // Can't increase the highest
                    systemPrint(") ");
                    systemPrintln(correctionsSourceNames[y]);
                    break;
                }
            }
            lower += 1;
        }

        systemPrintln();
        systemPrintln("x) Exit");

        byte incoming = getUserInputCharacterNumber();

        if ((incoming >= 'a') && (incoming < ('a' + correctionsSource::CORR_NUM - 1)))
        {
            // Decrease priority - swap the selected correction source with the next lowest
            int makeMeLower;
            for (int x = 0; x < correctionsSource::CORR_NUM; x++)
            {
                if (settings.correctionsSourcesPriority[x] == (int)(incoming - 'a'))
                {
                    makeMeLower = x;
                    break;
                }
            }
            int makeMeHigher;
            for (int x = 0; x < correctionsSource::CORR_NUM; x++)
            {
                if (settings.correctionsSourcesPriority[x] == (int)(1 + incoming - 'a'))
                {
                    makeMeHigher = x;
                    break;
                }
            }
            settings.correctionsSourcesPriority[makeMeLower] += 1;
            settings.correctionsSourcesPriority[makeMeHigher] -= 1;
        }
        else if ((incoming > 'A') && (incoming <= ('A' + correctionsSource::CORR_NUM - 1)))
        {
            // Increase priority - swap the selected correction source with the next highest
            int makeMeHigher;
            for (int x = 0; x < correctionsSource::CORR_NUM; x++)
            {
                if (settings.correctionsSourcesPriority[x] == (int)(incoming - 'A'))
                {
                    makeMeHigher = x;
                    break;
                }
            }
            int makeMeLower;
            for (int x = 0; x < correctionsSource::CORR_NUM; x++)
            {
                if (settings.correctionsSourcesPriority[x] == (int)(incoming - ('A' + 1)))
                {
                    makeMeLower = x;
                    break;
                }
            }
            settings.correctionsSourcesPriority[makeMeLower] += 1;
            settings.correctionsSourcesPriority[makeMeHigher] -= 1;
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

