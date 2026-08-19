// Write data to the LoRa radio, depends on platform
void loraWrite(uint8_t *data, uint16_t dataLength)
{
    if (productVariant == RTK_TORCH)
    {
        Serial.write(data, dataLength);
        Serial.flush(); // Ensure all data is sent before we switch back to USB
    }
    else if (productVariant == RTK_FACET_FP)
        SerialForLoRa->write(data, dataLength);
}

void loraWrite(uint8_t data)
{
    loraWrite(&data, 1);
}

void loraPrint(const char *data)
{
    if (productVariant == RTK_TORCH)
        Serial.print(data);
    else if (productVariant == RTK_FACET_FP)
        SerialForLoRa->print(data);
}

// Print a message to the primary serial port
// If the port is also being used for bootloader communication (ie on Torch)
// then switch to USB, print a status update, then return to talking to the STM32.
void loraSharedPrintln(const char *toPrint)
{
    if (productVariant == RTK_TORCH)
    {
        Serial.flush(); // Finishing any pending prints to before switching

        muxSelectUsb(); // Reconnect USB to print to terminal
        Serial.println(toPrint);
        Serial.flush();
        muxSelectLoRaCommunication(); // Torch: Disconnect USB, connect to LoRa
    }
    else
    {
        Serial.println(toPrint);
    }
}

void loraPrintf(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    va_list args2;
    va_copy(args2, args);
    char buf[vsnprintf(nullptr, 0, format, args) + 1];

    vsnprintf(buf, sizeof buf, format, args2);

    if (productVariant == RTK_TORCH)
        Serial.printf(buf);
    else if (productVariant == RTK_FACET_FP)
        SerialForLoRa->printf(buf);

    va_end(args);
    va_end(args2);
}

uint16_t loraAvailable()
{
    if (productVariant == RTK_TORCH)
        return (Serial.available());
    else if (productVariant == RTK_FACET_FP)
        return (SerialForLoRa->available());

    systemPrintln("loraAvailable - invalid ProductVariant");
    return 0;
}

uint16_t loraRead()
{
    if (productVariant == RTK_TORCH)
        return (Serial.read());
    else if (productVariant == RTK_FACET_FP)
        return (SerialForLoRa->read());

    systemPrintln("loraRead - invalid ProductVariant");
    return 0;
}

void muxSelectUsb()
{
    if (productVariant == RTK_TORCH)
    {
        pinMode(pin_muxB, OUTPUT); // Make really sure we can control this pin
        digitalWrite(pin_muxA,
                     LOW);           // Control U12: Connect ESP UART1 to UM980 UART3. Control U11: Connect U18-B1 to LoRa UART2
        digitalWrite(pin_muxB, LOW); // Control U18: Connect ESP UART0 to CH340 Serial

        //usbSerialIsSelected = true; // Let other print operations know we are connected to the CH34x
    }
}

// Connect ESP32 to LoRa for regular transmissions on Torch
// On Facet, startLoRaConfigureCommunicationOnFacet() is called separately
void muxSelectLoRaCommunication()
{
    if (productVariant == RTK_TORCH)
    {
        pinMode(pin_muxB, OUTPUT); // Make really sure we can control this pin
        digitalWrite(pin_muxA,
                     LOW);            // Control U12: Connect ESP UART1 to UM980 UART3. Control U11: Connect U18-B1 to LoRa UART2
        digitalWrite(pin_muxB, HIGH); // Control U18: Connect ESP UART0 to U11

        //usbSerialIsSelected = false; // Let other print operations know we are not connected to the CH34x
    }
}

void loraPowerOn()
{
    if (productVariant == RTK_TORCH)
        digitalWrite(pin_loraRadio_power, HIGH); // Power STM32/radio
    else if (productVariant == RTK_FACET_FP)
        gpioExpanderLoraEnable();
}

void loraPowerOff()
{
    if (productVariant == RTK_TORCH || productVariant == RTK_TORCH_X2)
        digitalWrite(pin_loraRadio_power, LOW); // Power off STM32/radio
    else if (productVariant == RTK_FACET_FP)
        gpioExpanderLoraDisable();
}

// Enables BOOT pin
void loraEnableBootloader()
{
    if (productVariant == RTK_TORCH || productVariant == RTK_TORCH_X2)
        digitalWrite(pin_loraRadio_boot, HIGH); // Enter bootload mode
    else if (productVariant == RTK_FACET_FP)
        gpioExpanderLoraBootEnable();
}

// Enables BOOT pin, then resets the STM32
void loraEnterBootloader()
{
    loraEnableBootloader();
    loraReset();
}

// Disables BOOT pin
void loraDisableBootloader()
{
    if (productVariant == RTK_TORCH || productVariant == RTK_TORCH_X2)
        digitalWrite(pin_loraRadio_boot, LOW); // Exit bootload mode
    else if (productVariant == RTK_FACET_FP)
        gpioExpanderLoraBootDisable();
}

// Disables BOOT pin, then resets the STM32
void loraExitBootloader()
{
    loraDisableBootloader();
    loraReset();
}

void loraReset()
{
    // This timing is sensitive. Delay too long after the enable and the bootloader
    // will exit due to timeout.
    if (productVariant == RTK_TORCH || productVariant == RTK_TORCH_X2)
    {
        digitalWrite(pin_loraRadio_reset, LOW);  // Reset STM32/radio
        delay(50);                               // 50 ok, 100 ok
        digitalWrite(pin_loraRadio_reset, HIGH); // Run STM32/radio
        delay(50);                               // 50 ok, 100 ok, 250 too long
    }
    else if (productVariant == RTK_FACET_FP)
    {
        // There is no reset, only a power cycle
        gpioExpanderLoraDisable();
        delay(50); // 50 ok, 100 ok,
        gpioExpanderLoraEnable();
        delay(50); // 50 ok, 100 ok, 250 too long
    }
}

// The following functions query the STM32's running application firmware for its version
// over the same UART, using the LoRa radio's AT command set (as opposed to the binary
// bootloader protocol used above). Adapted from RTK_Everywhere's LoRa.ino.
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-

char loraFirmwareVersionStr[25] = {'\0'}; // eg "3.0.1"
int loraFirmwareVersionInt = 0;           // eg 301

// Reads incoming bytes looking for "version:x.y.z" in the response to AT+V?.
// Parses and stores the version into loraFirmwareVersionStr/loraFirmwareVersionInt if found.
bool loraWaitForVersionResponse(unsigned long timeoutMs)
{
    const int responseLen = 48; // Enough to capture "version:x.y.z" and nearby response text
    char response[responseLen];
    int responseSpot = 0;

    unsigned long startTime = millis();
    while ((millis() - startTime) < timeoutMs)
    {
        if (loraAvailable())
        {
            if (responseSpot == responseLen - 1)
            {
                for (int i = 1; i < responseLen; i++)
                    response[i - 1] = response[i]; // Shift the FIFO along by 1
                responseSpot--;
            }
            response[responseSpot++] = loraRead();
            response[responseSpot] = 0;

            if (strstr(response, "version:"))
            {
                // Read in the rest of the response
                delay(10);
                while (loraAvailable())
                {
                    char incoming = loraRead();
                    if (responseSpot < (responseLen - 1))
                        response[responseSpot++] = incoming;
                }
                response[responseSpot] = 0;

                char *versionPtr = strstr(response, "version:");
                versionPtr += strlen("version:");
                while ((*versionPtr == ' ') || (*versionPtr == '\t'))
                    versionPtr++;

                int versionSpot = 0;
                while ((versionPtr[versionSpot] >= '0' && versionPtr[versionSpot] <= '9') ||
                       (versionPtr[versionSpot] == '.'))
                {
                    if (versionSpot >= (int)(sizeof(loraFirmwareVersionStr) - 1))
                        break;
                    loraFirmwareVersionStr[versionSpot] = versionPtr[versionSpot];
                    versionSpot++;
                }
                loraFirmwareVersionStr[versionSpot] = 0;

                int verMajor = 0;
                int verMinor = 0;
                int verPatch = 0;
                if (sscanf(loraFirmwareVersionStr, "%d.%d.%d", &verMajor, &verMinor, &verPatch) == 3)
                    loraFirmwareVersionInt = (verMajor * 100) + (verMinor * 10) + (verPatch);

                return true;
            }
        }
        else
            yield(); // Feed the idle/watchdog task while waiting on the UART
    }
    return false;
}

// Enters AT command mode on the STM32's application firmware and queries it with AT+V?.
// Re-inits the UART to 8N1 in case a prior bootload left it at the bootloader's 8E1 framing.
// On the Torch, USB and LoRa share a UART, so USB is briefly disconnected during the query.
bool loraEnterCommandMode()
{
    loraFirmwareVersionStr[0] = 0; // Clear any previously cached version before re-querying
    loraFirmwareVersionInt = 0;

    if (productVariant == RTK_TORCH)
    {
        Serial.flush(); // Finish printing any pending output before reconfiguring the UART

        Serial.end(); // Must end before begin, otherwise UART settings are corrupted
        Serial.begin(115200);
    }
    else if (productVariant == RTK_FACET_FP)
    {
        beginUart2Serial(); // Init the UART if not already initialized.
        SerialForLoRa->begin(115200, SERIAL_8N1, pin_IMU_RX, pin_IMU_TX);
    }

    loraPowerOn(); // Regardless of previous state, turn on the STM32
    delay(50);     // Give LoRa radio time to power stabilize

    loraReset(); // Make sure the STM32 is running its application, not sitting in the bootloader

    muxSelectLoRaCommunication(); // Torch: Disconnect USB, connect the LoRa radio to ESP32 UART0.
    if (productVariant == RTK_FACET_FP)
        gpioExpanderSelectLoraConfigure(); // Connect ESP32 UART2 to LoRa UART2

    delay(100); // Wait for incoming serial to settle
    while (loraAvailable())
        loraRead(); // Discard any junk left in the buffer

    // Query the version. If there's no reply, the STM32 may already be sitting in command mode
    // from a previous session - nudge it with +++ and try again.
    loraPrint("AT+V?\r\n");
    bool gotResponse = loraWaitForVersionResponse(2000);

    if (gotResponse == false)
    {
        loraPrint("+++\r\n");
        delay(100); // Allow STM32 time to enter command mode
        loraPrint("AT+V?\r\n");
        gotResponse = loraWaitForVersionResponse(2000);
    }

    muxSelectUsb(); // Torch: Reconnect USB to print to terminal

    if (gotResponse == false)
        systemPrintln("LoRa Error: Unable to enter command mode");

    return gotResponse;
}

// Queries the STM32 LoRa firmware version and prints it. Call at startup and again after a
// successful firmware update to confirm the new version took effect.
bool loraGetVersion()
{
    if (loraEnterCommandMode() == false)
        return false;

    systemPrintf("LoRa firmware: %s\r\n", loraFirmwareVersionStr);
    return true;
}