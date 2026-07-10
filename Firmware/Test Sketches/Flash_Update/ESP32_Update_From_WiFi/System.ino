// Callback for all firmware update targets. Called with the number of bytes written to flash so far. Used to track and print progress.
void firmwareUpdateProgressCallback(uint16_t bytesProcessed)
{
    const uint8_t progressBarWidth = 20;
    static uint8_t lastUpdatePercent = 0;

    firmwareUpdateBytesProcessed += bytesProcessed;

    uint32_t progressPercent = 0;
    if (firmwareUpdateBytesToProcess > 0)
        progressPercent = (firmwareUpdateBytesProcessed * 100UL) / firmwareUpdateBytesToProcess;

    if (progressPercent > 100)
        progressPercent = 100;

    uint8_t filled = (progressPercent * progressBarWidth) / 100;

    // Don't update unless there is a change
    if (progressPercent == lastUpdatePercent)
        return;

    lastUpdatePercent = progressPercent;

    systemPrint("Update Progress: [");
    for (uint8_t i = 0; i < progressBarWidth; i++)
        systemWrite(i < filled ? '#' : '-');

    systemPrint("] ");
    systemPrint(progressPercent);
    systemPrintln("%");
}
