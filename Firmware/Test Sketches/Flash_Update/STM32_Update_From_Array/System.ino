// Resest the system
void systemReset()
{
    Serial.println("System reset");
    Serial.flush();
    ESP.restart();
}

// Print the error message every 15 seconds
void reportFatalError(const char *errorMsg)
{
    uint32_t currentMsec;
    static uint32_t lastDisplayMsec;

    // Empty the FIFO of any incoming data
    serialInputClear(&Serial);

    lastDisplayMsec = millis() - MILLISECONDS_IN_A_DAY;
    while (1)
    {
        currentMsec = millis();
        if ((currentMsec - lastDisplayMsec) >= (15 * MILLISECONDS_IN_A_SECOND))
        {
            lastDisplayMsec = currentMsec;

            // Periodically display the halted message
            systemPrintf("HALTED: ");
            systemPrint(errorMsg);
            systemPrintln();
        }

        // Allow carriage return to reset the system
        if (Serial.available() && (Serial.read() == '\r'))
            systemReset();
    }
}
