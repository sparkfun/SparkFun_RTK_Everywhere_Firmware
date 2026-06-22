// Callback for all firmware update targets. Called with the number of bytes written to flash so far. Used to track and print progress.
void firmwareUpdateProgressCallback(uint16_t bytesProcessed)
{
    const uint8_t progressBarWidth = 20;

    firmwareUpdateBytesProcessed += bytesProcessed;

    uint32_t progressPercent = 0;
    if (firmwareUpdateBytesToProcess > 0)
        progressPercent = (firmwareUpdateBytesProcessed * 100UL) / firmwareUpdateBytesToProcess;

    if (progressPercent > 100)
        progressPercent = 100;

    uint8_t filled = (progressPercent * progressBarWidth) / 100;

    Serial.print("Update Progress: [");
    for (uint8_t i = 0; i < progressBarWidth; i++)
        Serial.print(i < filled ? '#' : '-');

    Serial.print("] ");
    Serial.print(progressPercent);
    Serial.println("%");
}
