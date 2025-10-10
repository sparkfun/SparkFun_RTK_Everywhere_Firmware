/*------------------------------------------------------------------------------
Tasks.ino

  This module implements the high-frequency tasks made by xTaskCreate() and any
  low-frequency tasks that are called by Ticker.

                                   GNSS
                                    |
                                    v
                           .--------+--------.
                           |                 |
                           v                 v
                          UART      or      I2C
                           |                 |
                           |                 |
                           '------->+<-------'
                                    |
                                    | gnssReadTask
                                    |    waitForPreamble
                                    |        ...
                                    |    processUart1Message
                                    |
                                    v
                               Ring Buffer
                                    |
                                    | handleGnssDataTask
                                    |
                                    v
            .---------------+-------+--------+---------------+
            |               |                |               |
            |               |                |               |
            v               v                v               v
        Bluetooth      TCP Client     TCP/UDP Server      SD Card

------------------------------------------------------------------------------*/

//----------------------------------------
// Macros
//----------------------------------------

#define WRAP_OFFSET(offset, increment, arraySize) \
    {                                             \
        offset += increment;                      \
        if (offset >= arraySize)                  \
            offset -= arraySize;                  \
    }

//----------------------------------------
// Constants
//----------------------------------------

enum RingBufferConsumers
{
    RBC_BLUETOOTH = 0,
    RBC_TCP_CLIENT,
    RBC_TCP_SERVER,
    RBC_SD_CARD,
    RBC_UDP_SERVER,
    RBC_USB_SERIAL,
    // Insert new consumers here
    RBC_MAX
};

const char *const ringBufferConsumer[] = {
    "Bluetooth",
    "TCP Client",
    "TCP Server",
    "SD Card",
    "UDP Server",
    "USB Serial",
};

const int ringBufferConsumerEntries = sizeof(ringBufferConsumer) / sizeof(ringBufferConsumer[0]);

// Define the index values into the parserTable
#define RTK_NMEA_PARSER_INDEX 0
#define RTK_UNICORE_HASH_PARSER_INDEX 1
#define RTK_RTCM_PARSER_INDEX 2
#define RTK_UBLOX_PARSER_INDEX 3
#define RTK_UNICORE_BINARY_PARSER_INDEX 4

// List the parsers to be included
SEMP_PARSE_ROUTINE const parserTable[] = {
    sempNmeaPreamble,
    sempUnicoreHashPreamble,
    sempRtcmPreamble,
    sempUbloxPreamble,
    sempUnicoreBinaryPreamble,
};
const int parserCount = sizeof(parserTable) / sizeof(parserTable[0]);

// List the names of the parsers
const char *const parserNames[] = {
    "NMEA",
    "Unicore Hash_(#)",
    "RTCM",
    "u-Blox",
    "Unicore Binary",
};
const int parserNameCount = sizeof(parserNames) / sizeof(parserNames[0]);

// We need a separate parsers for the mosaic-X5: to allow SBF to be separated from L-Band SPARTN;
// and to allow encapsulated NMEA and RTCMv3 to be parsed without upsetting the SPARTN parser.
SEMP_PARSE_ROUTINE const sbfParserTable[] = {sempSbfPreamble};
const int sbfParserCount = sizeof(sbfParserTable) / sizeof(sbfParserTable[0]);
const char *const sbfParserNames[] = {
    "SBF",
};
const int sbfParserNameCount = sizeof(sbfParserNames) / sizeof(sbfParserNames[0]);

SEMP_PARSE_ROUTINE const spartnParserTable[] = {sempSpartnPreamble};
const int spartnParserCount = sizeof(spartnParserTable) / sizeof(spartnParserTable[0]);
const char *const spartnParserNames[] = {
    "SPARTN",
};
const int spartnParserNameCount = sizeof(spartnParserNames) / sizeof(spartnParserNames[0]);

SEMP_PARSE_ROUTINE const rtcmParserTable[] = { sempRtcmPreamble };
const int rtcmParserCount = sizeof(rtcmParserTable) / sizeof(rtcmParserTable[0]);
const char *const rtcmParserNames[] = { "RTCM" };
const int rtcmParserNameCount = sizeof(rtcmParserNames) / sizeof(rtcmParserNames[0]);

//----------------------------------------
// Locals
//----------------------------------------

volatile static RING_BUFFER_OFFSET dataHead; // Head advances as data comes in from GNSS's UART
volatile int32_t availableHandlerSpace;      // settings.gnssHandlerBufferSize - usedSpace
volatile const char *slowConsumer;

// Buffer the incoming Bluetooth stream so that it can be passed in bulk over I2C
uint8_t bluetoothOutgoingToGnss[100];
uint16_t bluetoothOutgoingToGnssHead;
unsigned long lastGnssSend; // Timestamp of the last time we sent RTCM to GNSS

// Ring buffer tails
static RING_BUFFER_OFFSET btRingBufferTail;  // BT Tail advances as it is sent over BT
static RING_BUFFER_OFFSET sdRingBufferTail;  // SD Tail advances as it is recorded to SD
static RING_BUFFER_OFFSET usbRingBufferTail; // USB Tail advances as it is sent over USB serial

// Ring buffer offsets
static uint16_t rbOffsetHead;

//----------------------------------------
// Task routines
//----------------------------------------

// If the phone has any new data (NTRIP RTCM, etc), read it in over Bluetooth and pass it along to GNSS
// Scan for escape characters to enter the config menu
void btReadTask(void *e)
{
    int rxBytes;

    unsigned long btLastByteReceived = 0; // Track when the last BT transmission was received.
    const long btMinEscapeTime =
        2000;                          // Bluetooth serial traffic must stop this amount before an escape char is recognized
    uint8_t btEscapeCharsReceived = 0; // Used to enter remote command mode

    // Start notification
    task.btReadTaskRunning = true;
    if (settings.printTaskStartStop)
        systemPrintln("Task btReadTask started");

    // Run task until a request is raised
    task.btReadTaskStopRequest = false;
    while (task.btReadTaskStopRequest == false)
    {
        // Display an alive message
        if (PERIODIC_DISPLAY(PD_TASK_BLUETOOTH_READ) && !inMainMenu)
        {
            PERIODIC_CLEAR(PD_TASK_BLUETOOTH_READ);
            systemPrintln("btReadTask running");
        }

        // Receive RTCM corrections or UBX config messages over bluetooth and pass them along to GNSS
        rxBytes = 0;
        if (bluetoothGetState() == BT_CONNECTED)
        {
            while (btPrintEcho == false && (bluetoothRxDataAvailable() > 0))
            {
                // Check stream for command characters
                byte incoming = bluetoothRead();
                rxBytes += 1;

                if (incoming == btEscapeCharacter)
                {
                    // Ignore escape characters received within 2 seconds of serial traffic
                    // Allow escape characters received within the first 2 seconds of power on
                    if (((millis() - btLastByteReceived) > btMinEscapeTime) || (millis() < btMinEscapeTime))
                    {
                        btEscapeCharsReceived++;
                        if (btEscapeCharsReceived == btMaxEscapeCharacters)
                        {
                            printEndpoint = PRINT_ENDPOINT_ALL;
                            systemPrintln("Echoing all serial to BT device");
                            btPrintEcho = true;

                            btEscapeCharsReceived = 0;
                            btLastByteReceived = millis();
                        }
                    }
                    else
                    {
                        // Ignore this escape character, pass it along to the output
                        addToGnssBuffer(btEscapeCharacter);
                    }
                }


                else // This character is not a command character, pass along to GNSS
                {
                    // Pass any escape characters that turned out to not be a complete escape sequence
                    while (btEscapeCharsReceived-- > 0)
                    {
                        addToGnssBuffer(btEscapeCharacter);
                    }

                    // Pass byte to GNSS receiver or to system
                    // TODO - control if this RTCM source should be listened to or not

                    // UART RX can be corrupted by UART TX
                    // See issue: https://github.com/sparkfun/SparkFun_RTK_Firmware/issues/469
                    // serialGNSS->write(incoming);
                    addToGnssBuffer(incoming);

                    btLastByteReceived = millis();
                    btEscapeCharsReceived = 0; // Update timeout check for escape char and partial frame

                    bluetoothIncomingRTCM = true;

                    // Record the arrival of RTCM from the Bluetooth connection (a phone or tablet is providing the RTCM
                    // via NTRIP). This resets the RTCM timeout used on the L-Band.
                    rtcmLastPacketReceived = millis();

                } // End just a character in the stream

            } // End btPrintEcho == false && bluetoothRxDataAvailable()

            if (PERIODIC_DISPLAY(PD_BLUETOOTH_DATA_RX) && !inMainMenu)
            {
                PERIODIC_CLEAR(PD_BLUETOOTH_DATA_RX);
                systemPrintf("Bluetooth received %d bytes\r\n", rxBytes);
            }
        } // End bluetoothGetState() == BT_CONNECTED

        if (bluetoothOutgoingToGnssHead > 0 && ((millis() - lastGnssSend) > 100))
        {
            sendGnssBuffer(); // Send any outstanding RTCM
        }

        if ((settings.enableTaskReports == true) && (!inMainMenu))
            systemPrintf("SerialWriteTask High watermark: %d\r\n", uxTaskGetStackHighWaterMark(nullptr));

        feedWdt();
        taskYIELD();
    } // End while(true)

    // Stop notification
    if (settings.printTaskStartStop)
        systemPrintln("Task btReadTask stopped");
    task.btReadTaskRunning = false;
    vTaskDelete(NULL);
}

// Add byte to buffer that will be sent to GNSS
// We cannot write single characters to the ZED over I2C (as this will change the address pointer)
void addToGnssBuffer(uint8_t incoming)
{
    bluetoothOutgoingToGnss[bluetoothOutgoingToGnssHead] = incoming;

    bluetoothOutgoingToGnssHead++;
    if (bluetoothOutgoingToGnssHead == sizeof(bluetoothOutgoingToGnss))
    {
        sendGnssBuffer();
    }
}

// Push the buffered data in bulk to the GNSS
void sendGnssBuffer()
{
    if (correctionLastSeen(CORR_BLUETOOTH))
    {
        sempParseNextBytes(rtcmParse, bluetoothOutgoingToGnss, bluetoothOutgoingToGnssHead); // Parse the data for RTCM1005/1006
        if (gnss->pushRawData(bluetoothOutgoingToGnss, bluetoothOutgoingToGnssHead))
        {
            if ((settings.debugCorrections || PERIODIC_DISPLAY(PD_GNSS_DATA_TX)) && !inMainMenu)
            {
                PERIODIC_CLEAR(PD_GNSS_DATA_TX);
                systemPrintf("Sent %d BT bytes to GNSS\r\n", bluetoothOutgoingToGnssHead);
            }
        }
    }
    else
    {
        if ((settings.debugCorrections || PERIODIC_DISPLAY(PD_GNSS_DATA_TX)) && !inMainMenu)
        {
            PERIODIC_CLEAR(PD_GNSS_DATA_TX);
            systemPrintf("%d BT bytes NOT sent due to priority\r\n", bluetoothOutgoingToGnssHead);
        }
    }

    // No matter the response, wrap the head and reset the timer
    bluetoothOutgoingToGnssHead = 0;
    lastGnssSend = millis();
}

// Normally a delay(1) will feed the WDT but if we don't want to wait that long, this feeds the WDT without delay
void feedWdt()
{
    vTaskDelay(1);
}

//----------------------------------------------------------------------
// The ESP32<->GNSS serial connection is default 230,400bps to facilitate
// 10Hz fix rate with PPP Logging Defaults (NMEAx5 + RXMx2) messages enabled.
// ESP32's UART used for GNSS  is begun with settings.uartReceiveBufferSize size buffer. The circular buffer
// is 1024*6. At approximately 46.1K characters/second, a 6144 * 2
// byte buffer should hold 267ms worth of serial data. Assuming SD writes are
// 250ms worst case, we should record incoming all data. Bluetooth congestion
// or conflicts with the SD card semaphore should clear within this time.
//
// Ring buffer empty when all the tails == dataHead
//
//        +---------+
//        |         |
//        |         |
//        |         |
//        |         |
//        +---------+ <-- dataHead, btRingBufferTail, sdRingBufferTail, etc.
//
// Ring buffer contains data when any tail != dataHead
//
//        +---------+
//        |         |
//        |         |
//        | yyyyyyy | <-- dataHead
//        | xxxxxxx | <-- btRingBufferTail (1 byte in buffer)
//        +---------+ <-- sdRingBufferTail (2 bytes in buffer)
//
//        +---------+
//        | yyyyyyy | <-- btRingBufferTail (1 byte in buffer)
//        | xxxxxxx | <-- sdRingBufferTail (2 bytes in buffer)
//        |         |
//        |         |
//        +---------+ <-- dataHead
//
// Maximum ring buffer fill is settings.gnssHandlerBufferSize - 1
//----------------------------------------------------------------------

// Read bytes from GNSS into ESP32 circular buffer
// If data is coming in at 230,400bps = 23,040 bytes/s = one byte every 0.043ms
// If SD blocks for 150ms (not extraordinary) that is 3,488 bytes that must be buffered
// The ESP32 Arduino FIFO is ~120 bytes by default but overridden to 50 bytes (see pinGnssUartTask() and
// uart_set_rx_full_threshold()). We use this task to harvest from FIFO into circular buffer during SD write blocking
// time.
void gnssReadTask(void *e)
{
    // Start notification
    task.gnssReadTaskRunning = true;
    if (settings.printTaskStartStop)
        systemPrintln("Task gnssReadTask started");

    // Initialize the main parser
    rtkParse = sempBeginParser(parserTable, parserCount, parserNames, parserNameCount,
                               0,                   // Scratchpad bytes
                               3000,                // Buffer length
                               processUart1Message, // eom Call Back
                               "rtkParse");         // Parser Name
    if (!rtkParse)
        reportFatalError("Failed to initialize the parser");

    if (settings.debugGnss)
        sempEnableDebugOutput(rtkParse);

    bool sbfParserNeeded = present.gnss_mosaicX5;
    bool spartnParserNeeded = present.gnss_mosaicX5 && (productVariant != RTK_FLEX);

    if (sbfParserNeeded)
    {
        // Initialize the SBF parser for the mosaic-X5
        sbfParse = sempBeginParser(sbfParserTable, sbfParserCount, sbfParserNames, sbfParserNameCount,
                                   0,                      // Scratchpad bytes
                                   sempGnssReadBufferSize, // Buffer length - 3000 isn't enough!
                                   processUart1SBF,        // eom Call Back - in mosaic.ino
                                   "sbfParse");            // Parser Name
        if (!sbfParse)
            reportFatalError("Failed to initialize the SBF parser");

        // Uncomment the next line to enable SBF parser debug
        // But be careful - you get a lot of "SEMP: Sbf SBF, 0x0002 (2) bytes, invalid preamble2"
        // if (settings.debugGnss) sempEnableDebugOutput(sbfParse);

        if (spartnParserNeeded)
        {
            // Any data which is not SBF will be passed to the SPARTN parser via the invalid data callback
            sempSbfSetInvalidDataCallback(sbfParse, processNonSBFData);

            // Initialize the SPARTN parser for the mosaic-X5
            spartnParse = sempBeginParser(spartnParserTable, spartnParserCount, spartnParserNames, spartnParserNameCount,
                                        0,                  // Scratchpad bytes
                                        1200,               // Buffer length - SPARTN payload is 1024 bytes max
                                        processUart1SPARTN, // eom Call Back - in mosaic.ino
                                        "spartnParse");     // Parser Name
            if (!spartnParse)
                reportFatalError("Failed to initialize the SPARTN parser");

            // Uncomment the next line to enable SPARTN parser debug
            // But be careful - you get a lot of "SEMP: Spartn SPARTN 0 0, 0x00f4 (244) bytes, bad CRC"
            // if (settings.debugGnss) sempEnableDebugOutput(spartnParse);
        }
    }

    // Run task until a request is raised
    task.gnssReadTaskStopRequest = false;
    while (task.gnssReadTaskStopRequest == false)
    {
        // Display an alive message
        if (PERIODIC_DISPLAY(PD_TASK_GNSS_READ) && !inMainMenu)
        {
            PERIODIC_CLEAR(PD_TASK_GNSS_READ);
            systemPrintln("gnssReadTask running");
        }

        if ((settings.enableTaskReports == true) && (!inMainMenu))
            systemPrintf("SerialReadTask High watermark: %d\r\n", uxTaskGetStackHighWaterMark(nullptr));

        // Display the RX byte count
        static uint32_t totalRxByteCount = 0;
        if (PERIODIC_DISPLAY(PD_GNSS_DATA_RX_BYTE_COUNT) && !inMainMenu)
        {
            PERIODIC_CLEAR(PD_GNSS_DATA_RX_BYTE_COUNT);
            systemPrintf("gnssReadTask total byte count: %d\r\n", totalRxByteCount);
        }

        // Two methods are accessing the hardware serial port (um980Config) at the
        // same time: gnssReadTask() (to harvest incoming serial data) and um980 (the unicore library to configure the
        // device) To allow the Unicore library to send/receive serial commands, we need to block the gnssReadTask
        // If the Unicore library does not need lone access, then read from serial port

        // For the Facet mosaic-X5, things are different. The mosaic-X5 outputs a raw stream of L-Band bytes,
        // interspersed with periodic SBF blocks. The SBF blocks can contain encapsulated NMEA and RTCMv3.
        // We need to pass each incoming byte to a SBF parser first, so it can pick out any SBF blocks.
        // The SBF parser needs to 'give up' (return) any bytes which are not SBF. We do that using the
        // invalidDataCallback. Any data that is not SBF is then parsed by a separate SPARTN parser.
        // The SPARTN parser extracts SPARTN packets from the raw LBand data so they can be passed to the PPL.
        // Encapsulated NMEA and RTCMv3 is extracted and passed to the the main SEMP parser.
        // At some point in the future, Septentrio will (hopefully) add the ability to encapsulate the
        // raw L-Band data in SBF format. This will make life SO much easier as the SEMP will then be able
        // to parse everything and the separate SBF and SPARTN parsers won't be required.
        //
        // We need to be clever about this though. The raw L-Band data can manifest as SBF data. When that
        // happens, it can cause sbfParse to 'stick' - parsing a long ghost SBF block. This prevents RTCM
        // from being parsed from valid SBF blocks and causes the NTRIP server connection to break. We need
        // to add extra checks, above and beyond the invalidDataCallback, to make sure that doesn't happen.
        // Here we check that the SBF ID and length are expected / valid too.
        //
        // For Flex mosaic, we need the SBF parser but not the SPARTN parser

        if (gnss->isBlocking() == false)
        {
            // Determine if serial data is available
            while (serialGNSS->available())
            {
                // Read the data from UART1
                uint8_t incomingData[500];
                int bytesIncoming = serialGNSS->read(incomingData, sizeof(incomingData));
                totalRxByteCount += bytesIncoming;

                for (int x = 0; x < bytesIncoming; x++)
                {
                    // Update the parser state based on the incoming byte
                    // On mosaic-X5, pass the byte to sbfParse. On all other platforms, pass it straight to rtkParse
                    sempParseNextByte(sbfParserNeeded ? sbfParse : rtkParse, incomingData[x]);

                    // See notes above. On the mosaic-X5, check that the incoming SBF blocks have expected IDs and
                    // lengths to help prevent raw L-Band data being misidentified as SBF
                    if (sbfParserNeeded)
                    {
                        SEMP_SCRATCH_PAD *scratchPad = (SEMP_SCRATCH_PAD *)sbfParse->scratchPad;

                        // Check if this is Length MSB
                        // Also check parser is running - as invalidDataCallback may have just been called
                        if ((sbfParse->state != sempFirstByte) && (sbfParse->type == 0) && (sbfParse->length == 8))
                        {
                            bool expected = false;
                            for (int b = 0; b < MAX_MOSAIC_EXPECTED_SBF; b++) // For each expected SBF block
                            {
                                if (mosaicExpectedIDs[b].ID == scratchPad->sbf.sbfID) // Check for ID match
                                {
                                    expected = true;
                                    if (mosaicExpectedIDs[b].fixedLength)
                                    {
                                        // Check for length match if fixed
                                        if (mosaicExpectedIDs[b].length != scratchPad->sbf.length)
                                            expected = false;
                                    }
                                }
                            }
                            if (!expected) // SBF is not expected so restart the parsers
                            {
                                sbfParse->state = sempFirstByte;
                                if (spartnParserNeeded)                                
                                    spartnParse->state = sempFirstByte;
                                if (settings.debugGnss)
                                    systemPrintf("Unexpected SBF block %d - rejected on ID or length\r\n",
                                                 scratchPad->sbf.sbfID);
                                // We could pass the rejected bytes to the SPARTN parser but this is ~risky
                                // as the L-Band data could overlap the start of actual SBF. I think it's
                                // probably safer to discard the data and let both parsers re-sync?
                                // for (uint32_t dataOffset = 0; dataOffset < 8; dataOffset++)
                                // {
                                //     // Update the SPARTN parser state based on the non-SBF byte
                                //     sempParseNextByte(spartnParse, sbfParse->buffer[dataOffset]);
                                // }
                            }
                        }

                        // Extra checks for EncapsulatedOutput - length is variable but we can compare the length to the
                        // payload length
                        if ((sbfParse->type == 0) && (sbfParse->length == 18) && (scratchPad->sbf.sbfID == 4097))
                        {
                            bool expected = true;
                            if ((sbfParse->buffer[14] != 2) &&
                                (sbfParse->buffer[14] != 4)) // Check Mode for RTCMv3 and NMEA
                                expected = false;

                            // SBF block length should be 20 more than N (payload length) - with padding
                            if (expected)
                            {
                                uint16_t N = sbfParse->buffer[16];
                                N |= ((uint16_t)sbfParse->buffer[17]) << 8;
                                uint16_t expectedLength = N + 20; // Expected length
                                uint16_t remainder = ((N + 20) % 4);
                                if (remainder > 0)
                                    expectedLength += 4 - remainder; // Include the padding
                                if (scratchPad->sbf.length != expectedLength)
                                    expected = false;
                            }

                            if (!expected) // SBF is not expected so restart the parsers
                            {
                                sbfParse->state = sempFirstByte;
                                if (spartnParserNeeded)                                
                                    spartnParse->state = sempFirstByte;
                                if (settings.debugGnss)
                                    systemPrintf("Unexpected EncapsulatedOutput block - rejected\r\n");
                                // We could pass the rejected bytes to the SPARTN parser but this is ~risky
                                // as the L-Band data could overlap the start of actual SBF. I think it's
                                // probably safer to discard the data and let both parsers re-sync?
                                // for (uint32_t dataOffset = 0; dataOffset < 18; dataOffset++)
                                // {
                                //     // Update the SPARTN parser state based on the non-SBF byte
                                //     sempParseNextByte(spartnParse, sbfParse->buffer[dataOffset]);
                                // }
                            }
                        }
                    }
                }
            }
        }

        feedWdt();
        taskYIELD();
    }

    // Done parsing incoming data, free the parse buffer
    sempStopParser(&rtkParse);

    // Stop notification
    if (settings.printTaskStartStop)
        systemPrintln("Task gnssReadTask stopped");
    task.gnssReadTaskRunning = false;
    vTaskDelete(NULL);
}

// Force the NMEA Talker ID - if needed
void forceTalkerId(const char *Id, char *msg, size_t maxLen)
{
    if (msg[2] == *Id)
        return; // Nothing to do

    char oldTalker = msg[2];
    msg[2] = *Id; // Force the Talker ID
    
    // Update the checksum: XOR chars between '$' and '*'
    size_t len = 1;
    uint8_t csum = 0;
    while ((len < maxLen) && (msg[len] != '*'))
        csum = csum ^ msg[len++]; 

    if (len >= (maxLen - 3))
    {
        // Something went horribly wrong. Restore the Talker ID
        msg[2] = oldTalker;
        return;
    }

    len++; // Point at the checksum and update it
    sprintf(&msg[len], "%02X", csum);
}

// Force the RMC COG entry - if needed
void forceRmcCog(char *msg, size_t maxLen)
{
    const char *noCog = "0.0";

    if (strstr(msg, "RMC") == nullptr)
        return; // Nothing to do

    if (maxLen < (strlen(msg) + strlen(noCog)))
        return; // No room for the COG!

    // Find the start of COG ("Track made good") - after the 8th comma
    int numCommas = 0;
    size_t len = 0;
    while ((numCommas < 8) && (len < maxLen))
    {
        if (msg[len++] == ',')
            numCommas++;
    }

    if (len >= maxLen) // Something went horribly wrong
        return;

    // If the next char is not a ',' - there's nothing to do
    if (msg[len] != ',')
        return;

    // If the next char is a ',' - add "0.0" manually
    // Start by creating space for the "0.0"
    for (size_t i = strlen(msg); i > len; i--) // Work backwards from the NULL
        msg[i + strlen(noCog)] = msg[i];

    // Now insert the "0.0"
    memcpy(&msg[len], noCog, strlen(noCog));

    // Update the checksum: XOR chars between '$' and '*'
    len = 1;
    uint8_t csum = 0;
    while ((len < maxLen) && (msg[len] != '*'))
        csum = csum ^ msg[len++]; 
    len++; // Point at the checksum and update it
    sprintf(&msg[len], "%02X", csum);
}

// Remove the RMC Navigational Status field - if needed
void removeRmcNavStat(char *msg, size_t maxLen)
{
    if (strstr(msg, "RMC") == nullptr)
        return; // Nothing to do

    // Find the start of Nav Stat - at the 13th comma
    int numCommas = 0;
    size_t len = 0;
    while ((numCommas < 13) && (msg[len] != '*') && (len < maxLen))
    {
        if (msg[len++] == ',')
            numCommas++;
    }

    if (len >= (maxLen - 3)) // Something went horribly wrong
        return;

    // If the next char is '*' - there's nothing to do
    if ((msg[len] == '*') || (numCommas < 13))
        return;

    // Find the asterix. (NavStatus should be a single char)
    size_t asterix = len;
    len--;

    while ((msg[asterix] != '*') && (asterix < maxLen))
        asterix++;

    if (msg[asterix] != '*') // Something went horribly wrong
        return;

    // Delete the NavStat
    for (size_t i = 0; i < (strlen(msg) + 1 - asterix); i++) // Copy the * CSUM NULL
        msg[len + i] = msg[asterix + i];

    // Update the checksum: XOR chars between '$' and '*'
    len = 1;
    uint8_t csum = 0;
    while ((len < maxLen) && (msg[len] != '*'))
        csum = csum ^ msg[len++]; 
    len++; // Point at the checksum and update it
    sprintf(&msg[len], "%02X", csum);
}

// Call back from within parser, for end of message
// Process a complete message incoming from parser
// If we get a complete NMEA/UBX/RTCM message, pass on to SD/BT/TCP/UDP interfaces
void processUart1Message(SEMP_PARSE_STATE *parse, uint16_t type)
{
    int32_t bytesToCopy;
    const char *consumer;
    uint16_t message;
    RING_BUFFER_OFFSET remainingBytes;
    int32_t space;
    int32_t use;

    // Display the message
    if ((settings.enablePrintLogFileMessages || PERIODIC_DISPLAY(PD_GNSS_DATA_RX)) && (!inMainMenu))
    {
        PERIODIC_CLEAR(PD_GNSS_DATA_RX);
        if (settings.enablePrintLogFileMessages)
        {
            printTimeStamp();
            systemPrint("    ");
        }
        else
            systemPrint("GNSS RX: ");

        switch (type)
        {
        case RTK_NMEA_PARSER_INDEX:
            systemPrintf("%s %s %s, 0x%04x (%d) bytes\r\n", parse->parserName, parserNames[type],
                         sempNmeaGetSentenceName(parse), parse->length, parse->length);
            break;

        case RTK_UNICORE_HASH_PARSER_INDEX:
            systemPrintf("%s %s %s, 0x%04x (%d) bytes\r\n", parse->parserName, parserNames[type],
                         sempUnicoreHashGetSentenceName(parse), parse->length, parse->length);
            break;

        case RTK_RTCM_PARSER_INDEX:
            systemPrintf("%s %s %d, 0x%04x (%d) bytes\r\n", parse->parserName, parserNames[type],
                         sempRtcmGetMessageNumber(parse), parse->length, parse->length);
            break;

        case RTK_UBLOX_PARSER_INDEX:
            message = sempUbloxGetMessageNumber(parse);
            systemPrintf("%s %s %d.%d, 0x%04x (%d) bytes\r\n", parse->parserName, parserNames[type], message >> 8,
                         message & 0xff, parse->length, parse->length);
            break;

        case RTK_UNICORE_BINARY_PARSER_INDEX:
            systemPrintf("%s %s, 0x%04x (%d) bytes\r\n", parse->parserName, parserNames[type], parse->length,
                         parse->length);
            break;
        }
    }

    // Save GGA / RMC / GST for the Apple device
    // We should optimse this with a Lee table... TODO
    if ((online.authenticationCoPro) && (type == RTK_NMEA_PARSER_INDEX))
    {
        if (strstr(sempNmeaGetSentenceName(parse), "GGA") != nullptr)
        {
            if (parse->length < latestNmeaMaxLen)
            {
                memcpy(latestGPGGA, parse->buffer, parse->length);
                latestGPGGA[parse->length] = 0; // NULL terminate
                if ((strlen(latestGPGGA) > 10) && (latestGPGGA[strlen(latestGPGGA) - 2] == '\r'))
                    latestGPGGA[strlen(latestGPGGA) - 2] = 0; // Truncate the \r\n
                forceTalkerId("P",latestGPGGA,latestNmeaMaxLen);
            }
            else
                systemPrintf("Increase latestNmeaMaxLen to > %d\r\n", parse->length);
        }
        else if (strstr(sempNmeaGetSentenceName(parse), "RMC") != nullptr)
        {
            if (parse->length < latestNmeaMaxLen)
            {
                memcpy(latestGPRMC, parse->buffer, parse->length);
                latestGPRMC[parse->length] = 0; // NULL terminate
                if ((strlen(latestGPRMC) > 10) && (latestGPRMC[strlen(latestGPRMC) - 2] == '\r'))
                    latestGPRMC[strlen(latestGPRMC) - 2] = 0; // Truncate the \r\n
                forceTalkerId("P",latestGPRMC,latestNmeaMaxLen);
                forceRmcCog(latestGPRMC,latestNmeaMaxLen);
                removeRmcNavStat(latestGPRMC,latestNmeaMaxLen);
            }
            else
                systemPrintf("Increase latestNmeaMaxLen to > %d\r\n", parse->length);
        }
        else if (strstr(sempNmeaGetSentenceName(parse), "GST") != nullptr)
        {
            if (parse->length < latestNmeaMaxLen)
            {
                memcpy(latestGPGST, parse->buffer, parse->length);
                latestGPGST[parse->length] = 0; // NULL terminate
                if ((strlen(latestGPGST) > 10) && (latestGPGST[strlen(latestGPGST) - 2] == '\r'))
                    latestGPGST[strlen(latestGPGST) - 2] = 0; // Truncate the \r\n
                forceTalkerId("P",latestGPGST,latestNmeaMaxLen);
            }
            else
                systemPrintf("Increase latestNmeaMaxLen to > %d\r\n", parse->length);
        }
        else if (strstr(sempNmeaGetSentenceName(parse), "VTG") != nullptr)
        {
            if (parse->length < latestNmeaMaxLen)
            {
                memcpy(latestGPVTG, parse->buffer, parse->length);
                latestGPVTG[parse->length] = 0; // NULL terminate
                if ((strlen(latestGPVTG) > 10) && (latestGPVTG[strlen(latestGPVTG) - 2] == '\r'))
                    latestGPVTG[strlen(latestGPVTG) - 2] = 0; // Truncate the \r\n
                forceTalkerId("P",latestGPVTG,latestNmeaMaxLen);
            }
            else
                systemPrintf("Increase latestNmeaMaxLen to > %d\r\n", parse->length);
        }
        else if ((strstr(sempNmeaGetSentenceName(parse), "GSA") != nullptr)
                 || (strstr(sempNmeaGetSentenceName(parse), "GSV") != nullptr))
        {
            // If the Apple Accessory is sending the data to the EA Session,
            // discard this GSA / GSV. Bad things would happen if we were to
            // manipulate latestEASessionData while appleAccessory is using it.
#ifdef COMPILE_AUTHENTICATION
            if (appleAccessory->latestEASessionDataIsBlocking() == false)
            {
                size_t spaceAvailable = latestEASessionDataMaxLen - strlen(latestEASessionData);
                if (spaceAvailable >= 3)
                    spaceAvailable -= 3; // Leave room for the CR, LF and NULL
                while (spaceAvailable < parse->length) // If the buffer is full, delete the oldest message(s)
                {
                    const char *lfPtr = strstr(latestEASessionData, "\n"); // Find the first LF
                    if (lfPtr == nullptr)
                        break; // Something has gone badly wrong...
                    lfPtr++; // Point at the byte after the LF
                    size_t oldLen = lfPtr - latestEASessionData; // This much data is old
                    size_t newLen = strlen(latestEASessionData) - oldLen; // This much is new (not old)
                    for (size_t i = 0; i <= newLen; i++) // Move the new data over the old. Include the NULL
                        latestEASessionData[i] = latestEASessionData[oldLen + i];
                    spaceAvailable += oldLen;
                }
                size_t dataLen = strlen(latestEASessionData);
                memcpy(&latestEASessionData[dataLen], parse->buffer, parse->length); // Add the new NMEA data
                dataLen += parse->length;
                latestEASessionData[dataLen] = 0; // NULL terminate
                if (latestEASessionData[dataLen - 1] != '\n')
                {
                    latestEASessionData[dataLen] = '\r'; // Add CR
                    latestEASessionData[dataLen + 1] = '\n'; // Add LF
                    latestEASessionData[dataLen + 2] = 0; // NULL terminate
                }
            }
            else if (settings.debugNetworkLayer)
                systemPrintf("Discarding %d GSA/GSV bytes - latestEASessionDataIsBlocking\r\n", parse->length);
#endif //COMPILE_AUTHENTICATION
        }
    }

    // Determine if this message should be processed by the Unicore library
    // Pass NMEA to um980 before applying compensation
    if (present.gnss_um980)
    {
        if ((type == RTK_UNICORE_BINARY_PARSER_INDEX) || (type == RTK_UNICORE_HASH_PARSER_INDEX) ||
            (type == RTK_NMEA_PARSER_INDEX))
        {
            // Give this data to the library to update its internal variables
            um980UnicoreHandler(parse->buffer, parse->length);
        }
    }

    // Determine if this message should be processed by the LG290P library
    // Pass NMEA to LG290P before applying compensation
    if (present.gnss_lg290p)
    {
        // Give this data to the library to update its internal variables
        lg290pHandler(parse->buffer, parse->length);

        if (type == RTK_NMEA_PARSER_INDEX)
        {
            // Suppress PQTM/NMEA messages as needed
            if (lg290pMessageEnabled((char *)parse->buffer, parse->length) == false)
            {
                parse->buffer[0] = 0;
                parse->length = 0;
            }
        }
    }

    // Handle LLA compensation due to tilt or outputTipAltitude setting
    if (type == RTK_NMEA_PARSER_INDEX)
    {
        nmeaApplyCompensation((char *)parse->buffer, parse->length);
    }

    // Handle GST - extract the lat and lon standard deviations - on mosaic-X5 only
    if ((present.gnss_mosaicX5) && (type == RTK_NMEA_PARSER_INDEX))
    {
        nmeaExtractStdDeviations((char *)parse->buffer, parse->length);
    }

    // Determine where to send RTCM data
    if (inBaseMode() && type == RTK_RTCM_PARSER_INDEX)
    {
        // Pass data along to NTRIP Server, ESP-NOW radio, or LoRa
        processRTCM(parse->buffer, parse->length);
    }

    // Determine if we are using the PPL - UM980, LG290P, or mosaic-X5
    bool usingPPL = false;
    // UM980 : Determine if we want to use corrections, and are connected to the broker
    if ((present.gnss_um980) && (pointPerfectIsEnabled()) && (mqttClientIsConnected() == true))
        usingPPL = true;
    // LG290P : Determine if we want to use corrections, and are connected to the broker
    if ((present.gnss_lg290p) && (pointPerfectIsEnabled()) && (mqttClientIsConnected() == true))
        usingPPL = true;
    // mosaic-X5 : Determine if we want to use corrections
    if ((present.gnss_mosaicX5) && (pointPerfectLbandNeeded()))
        usingPPL = true;

    if (usingPPL)
    {
        bool passToPpl = false;

        // Only messages GPGGA/ZDA, and RTCM1019/1020/1042/1046 need to be passed to PPL
        if (type == RTK_NMEA_PARSER_INDEX)
        {
            if (strstr(sempNmeaGetSentenceName(parse), "GGA") != nullptr)
                passToPpl = true;
            else if (strstr(sempNmeaGetSentenceName(parse), "ZDA") != nullptr)
                passToPpl = true;
        }
        else if (type == RTK_RTCM_PARSER_INDEX)
        {
            if (sempRtcmGetMessageNumber(parse) == 1019)
                passToPpl = true;
            else if (sempRtcmGetMessageNumber(parse) == 1020)
                passToPpl = true;
            else if (sempRtcmGetMessageNumber(parse) == 1042)
                passToPpl = true;
            else if (sempRtcmGetMessageNumber(parse) == 1046)
                passToPpl = true;
        }

        if (passToPpl == true)
        {
            pplNewRtcmNmea = true; // Set flag for main loop updatePPL()
            sendGnssToPpl(parse->buffer, parse->length);

            if ((settings.debugCorrections == true) && !inMainMenu)
            {
                systemPrint("Sent to PPL: ");

                // Only messages GPGGA/ZDA, and RTCM1019/1020/1042/1046 need to be passed to PPL
                if (type == RTK_NMEA_PARSER_INDEX)
                {
                    systemPrint("GN");
                    if (strstr(sempNmeaGetSentenceName(parse), "GGA") != nullptr)
                        systemPrint("GGA");
                    else if (strstr(sempNmeaGetSentenceName(parse), "ZDA") != nullptr)
                        systemPrint("ZDA");
                }
                else if (type == RTK_RTCM_PARSER_INDEX)
                {
                    systemPrint("RTCM");
                    if (sempRtcmGetMessageNumber(parse) == 1019)
                        systemPrint("1019");
                    else if (sempRtcmGetMessageNumber(parse) == 1020)
                        systemPrint("1020");
                    else if (sempRtcmGetMessageNumber(parse) == 1042)
                        systemPrint("1042");
                    else if (sempRtcmGetMessageNumber(parse) == 1046)
                        systemPrint("1046");
                }

                systemPrintf(": %d bytes\r\n", parse->length);
            }
        }
    }

    // If BaseCasterOverride is enabled, remove everything but RTCM from the circular buffer
    // to avoid saturating the downstream radio link that is consuming over a TCP (NTRIP Caster) connection
    // Remove NMEA, etc after passing to the GNSS receiver library so that we still have SIV and other stats available
    if (tcpServerInCasterMode)
    {
        if (type != RTK_RTCM_PARSER_INDEX)
        {
            // Erase buffer
            parse->buffer[0] = 0;
            parse->length = 0;
        }
    }

    // Push GGA to Caster if enabled
    if (type == RTK_NMEA_PARSER_INDEX && strstr(sempNmeaGetSentenceName(parse), "GGA") != nullptr)
    {
        pushGPGGA((char *)parse->buffer);
    }

    // If the user has not specifically enabled RTCM used by the PPL, then suppress it
    if (inRoverMode() && gnss->getActiveRtcmMessageCount() == 0 && type == RTK_RTCM_PARSER_INDEX)
    {
        // Erase buffer
        parse->buffer[0] = 0;
        parse->length = 0;
    }

    // Suppress binary messages from UM980. Not needed by end GIS apps.
    if (type == RTK_UNICORE_BINARY_PARSER_INDEX)
    {
        // Erase buffer
        parse->buffer[0] = 0;
        parse->length = 0;
    }

    // If parse->length is zero, we should exit now.
    // Previously, the code would continue past here and fill rbOffsetArray with 'empty' entries.
    // E.g. RTCM1019/1042/1046 suppressed above
    if (parse->length == 0)
        return;

    // Use a semaphore to prevent handleGnssDataTask from gatecrashing
    if (ringBufferSemaphore == NULL)
        ringBufferSemaphore = xSemaphoreCreateMutex();  // Create the mutex
    
    // Take the semaphore. Long wait. handleGnssDataTask could block
    // Enable printing of the ring buffer offsets (s d 10) and the SD buffer sizes (s h 7)
    // to see this in action. No more gatecrashing!
    if (xSemaphoreTake(ringBufferSemaphore, ringBuffer_longWait_ms) == pdPASS)
    {
        ringBufferSemaphoreHolder = "processUart1Message";

        // Determine if this message will fit into the ring buffer
        int32_t discardedBytes = 0;
        bytesToCopy = parse->length;
        space = availableHandlerSpace; // Take a copy of availableHandlerSpace here
        use = settings.gnssHandlerBufferSize - space;
        consumer = (char *)slowConsumer;
        if (bytesToCopy > space) // Paul removed the && (!inMainMenu)) check 7-25-25
        {
            int32_t bufferedData;
            int32_t bytesToDiscard;
            int32_t listEnd;
            int32_t messageLength;
            int32_t previousTail;
            int32_t rbOffsetTail;

            // Determine the tail of the ring buffer
            previousTail = dataHead + space + 1;
            if (previousTail >= settings.gnssHandlerBufferSize)
                previousTail -= settings.gnssHandlerBufferSize;

            /*  The rbOffsetArray holds the offsets into the ring buffer of the
            *  start of each of the parsed messages.  A head (rbOffsetHead) and
            *  tail (rbOffsetTail) offsets are used for this array to insert and
            *  remove entries.  Typically this task only manipulates the head as
            *  new messages are placed into the ring buffer.  The handleGnssDataTask
            *  normally manipulates the tail as data is removed from the buffer.
            *  However this task will manipulate the tail under two conditions:
            *
            *  1.  The ring buffer gets full and data must be discarded
            *
            *  2.  The rbOffsetArray is too small to hold all of the message
            *      offsets for the data in the ring buffer.  The array is full
            *      when (Head + 1) == Tail
            *
            *  Notes:
            *      The rbOffsetArray is allocated along with the ring buffer in
            *      Begin.ino
            *
            *      The first entry rbOffsetArray[0] is initialized to zero (0)
            *      in Begin.ino
            *
            *      The array always has one entry in it containing the head offset
            *      which contains a valid offset into the ringBuffer, handled below
            *
            *      The empty condition is Tail == Head
            *
            *      The amount of data described by the rbOffsetArray is
            *      rbOffsetArray[Head] - rbOffsetArray[Tail]
            *
            *              rbOffsetArray                  ringBuffer
            *           .-----------------.           .-----------------.
            *           |                 |           |                 |
            *           +-----------------+           |                 |
            *  Tail --> |   Msg 1 Offset  |---------->+-----------------+ <-- Tail n
            *           +-----------------+           |      Msg 1      |
            *           |   Msg 2 Offset  |--------.  |                 |
            *           +-----------------+        |  |                 |
            *           |   Msg 3 Offset  |------. '->+-----------------+
            *           +-----------------+      |    |      Msg 2      |
            *  Head --> |   Head Offset   |--.   |    |                 |
            *           +-----------------+  |   |    |                 |
            *           |                 |  |   |    |                 |
            *           +-----------------+  |   |    |                 |
            *           |                 |  |   '--->+-----------------+
            *           +-----------------+  |        |      Msg 3      |
            *           |                 |  |        |                 |
            *           +-----------------+  '------->+-----------------+ <-- dataHead
            *           |                 |           |                 |
            */

            // Determine the index for the end of the circular ring buffer
            // offset list
            listEnd = rbOffsetHead;
            WRAP_OFFSET(listEnd, 1, rbOffsetEntries);

            // Update the tail, walk newest message to oldest message
            rbOffsetTail = rbOffsetHead;
            bufferedData = 0;
            messageLength = 0;
            while ((rbOffsetTail != listEnd) && (bufferedData < use))
            {
                // Determine the amount of data in the ring buffer up until
                // either the tail or the end of the rbOffsetArray
                //
                //                      |           |
                //                      |           | Valid, still in ring buffer
                //                      |  Newest   |
                //                      +-----------+ <-- rbOffsetHead
                //                      |           |
                //                      |           | free space
                //                      |           |
                //     rbOffsetTail --> +-----------+ <-- bufferedData
                //                      |   ring    |
                //                      |  buffer   | <-- used
                //                      |   data    |
                //                      +-----------+ Valid, still in ring buffer
                //                      |           |
                //
                messageLength = rbOffsetArray[rbOffsetTail];
                WRAP_OFFSET(rbOffsetTail, rbOffsetEntries - 1, rbOffsetEntries);
                messageLength -= rbOffsetArray[rbOffsetTail];
                if (messageLength < 0)
                    messageLength += settings.gnssHandlerBufferSize;
                bufferedData += messageLength;
            }

            // Account for any data in the ring buffer not described by the array
            //
            //                      |           |
            //                      +-----------+
            //                      |  Oldest   |
            //                      |           |
            //                      |   ring    |
            //                      |  buffer   | <-- used
            //                      |   data    |
            //                      +-----------+ Valid, still in ring buffer
            //                      |           |
            //     rbOffsetTail --> +-----------+ <-- bufferedData
            //                      |           |
            //                      |  Newest   |
            //                      +-----------+ <-- rbOffsetHead
            //                      |           |
            //
            if (bufferedData < use)
                discardedBytes = use - bufferedData;

            // Writing to the SD card, the network or Bluetooth, a partial
            // message may be written leaving the tail pointer mid-message
            //
            //                      |           |
            //     rbOffsetTail --> +-----------+
            //                      |  Oldest   |
            //                      |           |
            //                      |   ring    |
            //                      |  buffer   | <-- used
            //                      |   data    | Valid, still in ring buffer
            //                      +-----------+ <--
            //                      |           |
            //                      +-----------+
            //                      |           |
            //                      |  Newest   |
            //                      +-----------+ <-- rbOffsetHead
            //                      |           |
            //
            else if (bufferedData > use)
            {
                // Remove the remaining portion of the oldest entry in the array
                discardedBytes = messageLength + use - bufferedData;
                WRAP_OFFSET(rbOffsetTail, 1, rbOffsetEntries);
            }

            // rbOffsetTail now points to the beginning of a message in the
            // ring buffer
            // Determine the amount of data to discard
            bytesToDiscard = discardedBytes;
            if (bytesToDiscard < bytesToCopy)
                bytesToDiscard = bytesToCopy;
            if (bytesToDiscard < AMOUNT_OF_RING_BUFFER_DATA_TO_DISCARD)
                bytesToDiscard = AMOUNT_OF_RING_BUFFER_DATA_TO_DISCARD;

            // Walk the ring buffer messages from oldest to newest
            while ((discardedBytes < bytesToDiscard) && (rbOffsetTail != rbOffsetHead))
            {
                // Determine the length of the oldest message
                WRAP_OFFSET(rbOffsetTail, 1, rbOffsetEntries);
                discardedBytes = rbOffsetArray[rbOffsetTail] - previousTail;
                if (discardedBytes < 0)
                    discardedBytes += settings.gnssHandlerBufferSize;
            }

            // Discard the oldest data from the ring buffer
            // Printing the slow consumer is not that useful as any consumer will be
            // considered 'slow' if its data wraps over the end of the buffer and
            // needs a second write to clear...
            if (!inMainMenu)
            {
                if (consumer)
                    systemPrintf("Ring buffer full: discarding %d bytes, %s could be slow\r\n", discardedBytes, consumer);
                else
                    systemPrintf("Ring buffer full: discarding %d bytes\r\n", discardedBytes);
                Serial.flush(); // TODO - delete me!
            }

            // Update the tails. This needs semaphore protection
            updateRingBufferTails(previousTail, rbOffsetArray[rbOffsetTail]);
        }

        if (bytesToCopy > (space + discardedBytes - 1)) // Sanity check
        {
            systemPrintf("Ring buffer update error %s: bytesToCopy (%d) is > space (%d) + discardedBytes (%d) - 1\r\n",
                        getTimeStamp(), bytesToCopy, space, discardedBytes);
            Serial.flush(); // Flush Serial - the code is about to go bang...!
        }
        
        // Add another message to the ring buffer
        // Account for this message
        // Diagnostic prints are provided by settings.enablePrintSDBuffers and the handleGnssDataTask
        // The semaphore prevents badness here. Previously availableHandlerSpace may have been updated
        // by handleGnssDataTask
        availableHandlerSpace = availableHandlerSpace + discardedBytes - bytesToCopy;

        // Copy dataHead so we can update with a single write - redundant with the semaphore
        RING_BUFFER_OFFSET newDataHead = dataHead;

        // Fill the buffer to the end and then start at the beginning
        if ((newDataHead + bytesToCopy) > settings.gnssHandlerBufferSize)
            bytesToCopy = settings.gnssHandlerBufferSize - newDataHead;

        // Display the dataHead offset
        if (settings.enablePrintRingBufferOffsets && (!inMainMenu))
            systemPrintf("DH: %4d --> ", newDataHead);

        // Copy the data into the ring buffer
        memcpy(&ringBuffer[newDataHead], parse->buffer, bytesToCopy);
        newDataHead = newDataHead + bytesToCopy;
        if (newDataHead >= settings.gnssHandlerBufferSize)
            newDataHead = newDataHead - settings.gnssHandlerBufferSize;

        // Determine the remaining bytes
        remainingBytes = parse->length - bytesToCopy;
        if (remainingBytes)
        {
            // Copy the remaining bytes into the beginning of the ring buffer
            memcpy(ringBuffer, &parse->buffer[bytesToCopy], remainingBytes);
            newDataHead = newDataHead + remainingBytes;
            if (newDataHead >= settings.gnssHandlerBufferSize)
                newDataHead = newDataHead - settings.gnssHandlerBufferSize;
        }

        // Add the head offset to the offset array
        WRAP_OFFSET(rbOffsetHead, 1, rbOffsetEntries);
        rbOffsetArray[rbOffsetHead] = newDataHead;

        // Display the dataHead offset
        if (settings.enablePrintRingBufferOffsets && (!inMainMenu))
            systemPrintf("%4d @ %s\r\n", newDataHead, getTimeStamp());

        // Update dataHead in a single write - redundant with the semaphore
        // handleGnssDataTask will use it as soon as it updates
        dataHead = newDataHead;

        // handleGnssDataTask will be chomping at the bit. Let it fly!
        xSemaphoreGive(ringBufferSemaphore);
    }
    else
    {
        systemPrintf("processUart1Message could not get ringBuffer semaphore - held by %s\r\n", ringBufferSemaphoreHolder);
    }
}

// Remove previous messages from the ring buffer
void updateRingBufferTails(RING_BUFFER_OFFSET previousTail, RING_BUFFER_OFFSET newTail)
{
    // Trim any long or medium tails
    discardRingBufferBytes(&btRingBufferTail, previousTail, newTail);
    tcpClientDiscardBytes(previousTail, newTail);
    tcpServerDiscardBytes(previousTail, newTail);
    udpServerDiscardBytes(previousTail, newTail);
    discardRingBufferBytes(&sdRingBufferTail, previousTail, newTail);
    discardRingBufferBytes(&usbRingBufferTail, previousTail, newTail);
}

// Remove previous messages from the ring buffer
void discardRingBufferBytes(RING_BUFFER_OFFSET *tail, RING_BUFFER_OFFSET previousTail, RING_BUFFER_OFFSET newTail)
{
    // The longest tail is being trimmed.  Medium length tails may contain
    // some data within the region begin trimmed.  The shortest tails will
    // be trimmed.
    //
    // Devices that get their tails trimmed, may output a partial message
    // prior to the buffer trimming.  After the trimming, the tail of the
    // ring buffer points to the beginning of a new message.
    //
    //                 previousTail                newTail
    //                      |                         |
    //  Before trimming     v         Discarded       v   After trimming
    //  ----+-----------------  ...  -----+--  ..  ---+-----------+------
    //      | Partial message             |           |           |
    //  ----+-----------------  ...  -----+--  ..  ---+-----------+------
    //                      ^          ^                     ^
    //                      |          |                     |
    //        long tail ----'          '--- medium tail      '-- short tail
    //
    // Determine if the trimmed data wraps the end of the buffer
    if (previousTail < newTail)
    {
        // No buffer wrap occurred
        // Only discard the data from long and medium tails
        if ((*tail >= previousTail) && (*tail < newTail))
            *tail = newTail;
    }
    else
    {
        // Buffer wrap occurred
        if ((*tail >= previousTail) || (*tail < newTail))
            *tail = newTail;
    }
}

// If new data is in the ringBuffer, dole it out to appropriate interface
// Send data out Bluetooth, record to SD, or send to network clients
// Each device (Bluetooth, SD and network client) gets its own tail.  If the
// device is running too slowly then data for that device is dropped.
// The usedSpace variable tracks the total space in use in the buffer.
void handleGnssDataTask(void *e)
{
    int32_t bytesToSend;
    uint32_t deltaMillis;
    int32_t freeSpace;
    static uint32_t maxMillis[RBC_MAX];
    unsigned long startMillis;
    int32_t usedSpace;

    // Start notification
    task.handleGnssDataTaskRunning = true;
    if (settings.printTaskStartStop)
        systemPrintln("Task handleGnssDataTask started");

    // Initialize the tails
    btRingBufferTail = 0;
    tcpClientZeroTail();
    tcpServerZeroTail();
    udpServerZeroTail();
    sdRingBufferTail = 0;
    usbRingBufferTail = 0;

    // Run task until a request is raised
    task.handleGnssDataTaskStopRequest = false;
    while (task.handleGnssDataTaskStopRequest == false)
    {
        // Display an alive message
        if (PERIODIC_DISPLAY(PD_TASK_HANDLE_GNSS_DATA) && !inMainMenu)
        {
            PERIODIC_CLEAR(PD_TASK_HANDLE_GNSS_DATA);
            systemPrintln("handleGnssDataTask running");
        }

        usedSpace = 0;

        // Use a semaphore to prevent handleGnssDataTask from gatecrashing
        if (ringBufferSemaphore == NULL)
            ringBufferSemaphore = xSemaphoreCreateMutex();  // Create the mutex
        
        // Take the semaphore. Short wait. processUart1Message shouldn't block for long
        if (xSemaphoreTake(ringBufferSemaphore, ringBuffer_shortWait_ms) == pdPASS)
        {
            ringBufferSemaphoreHolder = "handleGnssDataTask";

            //----------------------------------------------------------------------
            // Send data over Bluetooth
            //----------------------------------------------------------------------

            startMillis = millis();

            // Determine BT connection state. Discard the data if in BLUETOOTH_RADIO_SPP_ACCESSORY_MODE
            bool connected = false;
            if (settings.bluetoothRadioType != BLUETOOTH_RADIO_SPP_ACCESSORY_MODE)
                connected = (bluetoothGetState() == BT_CONNECTED);

            if (!connected)
                // Discard the data
                btRingBufferTail = dataHead;
            else
            {
                // Determine the amount of Bluetooth data in the buffer
                bytesToSend = dataHead - btRingBufferTail;
                if (bytesToSend < 0)
                    bytesToSend += settings.gnssHandlerBufferSize;
                if (bytesToSend > 0)
                {
                    // Reduce bytes to send if we have more to send then the end of
                    // the buffer, we'll wrap next loop
                    if ((btRingBufferTail + bytesToSend) > settings.gnssHandlerBufferSize)
                        bytesToSend = settings.gnssHandlerBufferSize - btRingBufferTail;

                    // If we are in the config menu, suppress data flowing from GNSS to cell phone
                    if (btPrintEcho == false)
                    {
                        // Push new data over Bluetooth
                        bytesToSend = bluetoothWrite(&ringBuffer[btRingBufferTail], bytesToSend);
                    }

                    // Account for the data that was sent
                    if (bytesToSend > 0)
                    {
                        // If we are in base mode, assume part of the outgoing data is RTCM
                        if (inBaseMode() == true)
                            bluetoothOutgoingRTCM = true;

                        // Account for the sent or dropped data
                        btRingBufferTail += bytesToSend;
                        if (btRingBufferTail >= settings.gnssHandlerBufferSize)
                            btRingBufferTail -= settings.gnssHandlerBufferSize;

                        // Remember the maximum transfer time
                        deltaMillis = millis() - startMillis;
                        if (maxMillis[RBC_BLUETOOTH] < deltaMillis)
                            maxMillis[RBC_BLUETOOTH] = deltaMillis;

                        // Display the data movement
                        if (PERIODIC_DISPLAY(PD_BLUETOOTH_DATA_TX) && !inMainMenu)
                        {
                            PERIODIC_CLEAR(PD_BLUETOOTH_DATA_TX);
                            systemPrintf("Bluetooth: %d bytes written\r\n", bytesToSend);
                        }
                    }
                    else
                        log_w("BT failed to send");

                    // Determine the amount of data that remains in the buffer
                    bytesToSend = dataHead - btRingBufferTail;
                    if (bytesToSend < 0)
                        bytesToSend += settings.gnssHandlerBufferSize;
                    if (usedSpace < bytesToSend)
                    {
                        usedSpace = bytesToSend;
                        slowConsumer = "Bluetooth";
                    }
                }
            }

            //----------------------------------------------------------------------
            // Send data over USB serial
            //----------------------------------------------------------------------

            startMillis = millis();

            // Determine USB serial connection state
            if (!forwardGnssDataToUsbSerial)
                // Discard the data
                usbRingBufferTail = dataHead;
            else
            {
                // Determine the amount of USB serial data in the buffer
                bytesToSend = dataHead - usbRingBufferTail;
                if (bytesToSend < 0)
                    bytesToSend += settings.gnssHandlerBufferSize;
                if (bytesToSend > 0)
                {
                    // Reduce bytes to send if we have more to send then the end of
                    // the buffer, we'll wrap next loop
                    if ((usbRingBufferTail + bytesToSend) > settings.gnssHandlerBufferSize)
                        bytesToSend = settings.gnssHandlerBufferSize - usbRingBufferTail;

                    // Send data over USB serial to the PC
                    bytesToSend = systemWriteGnssDataToUsbSerial(&ringBuffer[usbRingBufferTail], bytesToSend);

                    // Account for the data that was sent
                    if (bytesToSend > 0)
                    {
                        // Account for the sent or dropped data
                        usbRingBufferTail += bytesToSend;
                        if (usbRingBufferTail >= settings.gnssHandlerBufferSize)
                            usbRingBufferTail -= settings.gnssHandlerBufferSize;

                        // Remember the maximum transfer time
                        deltaMillis = millis() - startMillis;
                        if (maxMillis[RBC_USB_SERIAL] < deltaMillis)
                            maxMillis[RBC_USB_SERIAL] = deltaMillis;
                    }

                    // Determine the amount of data that remains in the buffer
                    bytesToSend = dataHead - usbRingBufferTail;
                    if (bytesToSend < 0)
                        bytesToSend += settings.gnssHandlerBufferSize;
                    if (usedSpace < bytesToSend)
                    {
                        usedSpace = bytesToSend;
                        slowConsumer = "USB Serial";
                    }
                }
            }

            //----------------------------------------------------------------------
            // Send data to the network clients
            //----------------------------------------------------------------------

            startMillis = millis();

            // Update space available for use in UART task
            bytesToSend = tcpClientSendData(dataHead);
            if (usedSpace < bytesToSend)
            {
                usedSpace = bytesToSend;
                slowConsumer = "TCP client";
            }

            // Remember the maximum transfer time
            deltaMillis = millis() - startMillis;
            if (maxMillis[RBC_TCP_CLIENT] < deltaMillis)
                maxMillis[RBC_TCP_CLIENT] = deltaMillis;

            startMillis = millis();

            // Update space available for use in UART task
            bytesToSend = tcpServerSendData(dataHead);
            if (usedSpace < bytesToSend)
            {
                usedSpace = bytesToSend;
                slowConsumer = "TCP server";
            }

            // Remember the maximum transfer time
            deltaMillis = millis() - startMillis;
            if (maxMillis[RBC_TCP_SERVER] < deltaMillis)
                maxMillis[RBC_TCP_SERVER] = deltaMillis;

            startMillis = millis();

            // Update space available for use in UART task
            bytesToSend = udpServerSendData(dataHead);
            if (usedSpace < bytesToSend)
            {
                usedSpace = bytesToSend;
                slowConsumer = "UDP server";
            }

            // Remember the maximum transfer time
            deltaMillis = millis() - startMillis;
            if (maxMillis[RBC_UDP_SERVER] < deltaMillis)
                maxMillis[RBC_UDP_SERVER] = deltaMillis;

            //----------------------------------------------------------------------
            // Log data to the SD card
            //----------------------------------------------------------------------

            // Determine if the SD card is enabled for logging
            connected = online.logging && (!logTimeExceeded());

            // Block logging during Web Config to avoid SD collisions
            // See issue: https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/issues/693
            if(webServerIsRunning() == true)
                connected = false;

            // If user wants to log, record to SD
            if (!connected)
                // Discard the data
                sdRingBufferTail = dataHead;
            else
            {
                // Determine the amount of microSD card logging data in the buffer
                bytesToSend = dataHead - sdRingBufferTail;
                if (bytesToSend < 0)
                    bytesToSend += settings.gnssHandlerBufferSize;
                if (bytesToSend > 0)
                {
                    // Attempt to gain access to the SD card, avoids collisions with file
                    // writing from other functions like recordSystemSettingsToFile()
                    if (xSemaphoreTake(sdCardSemaphore, loggingSemaphoreWait_ms) == pdPASS)
                    {
                        markSemaphore(FUNCTION_WRITESD);

                        do // Do the SD write in a do loop so we can break out if needed
                        {
                            if (settings.enablePrintSDBuffers && (!inMainMenu))
                            {
                                int bufferAvailable = serialGNSS->available();

                                int availableUARTSpace = settings.uartReceiveBufferSize - bufferAvailable;

                                systemPrintf("SD Incoming Serial @ %s: %04d\tToRead: %04d\tMovedToBuffer: %04d\tavailableUARTSpace: "
                                            "%04d\tavailableHandlerSpace: %04d\tToRecord: %04d\tRecorded: %04d\tBO: %d\r\n",
                                            getTimeStamp(), bufferAvailable, 0, 0, availableUARTSpace, availableHandlerSpace,
                                            bytesToSend, 0, bufferOverruns);
                            }

                            // For the SD card, we need to write everything we've got
                            // to prevent the ARP Write and Events from gatecrashing...
                            
                            int32_t sendTheseBytes = bytesToSend;

                            // Reduce bytes to record if we have more then the end of the buffer
                            if ((sdRingBufferTail + sendTheseBytes) > settings.gnssHandlerBufferSize)
                                sendTheseBytes = settings.gnssHandlerBufferSize - sdRingBufferTail;

                            startMillis = millis();

                            // Write the data to the file
                            int32_t bytesSent = logFile->write(&ringBuffer[sdRingBufferTail], sendTheseBytes);

                            // Account for the sent data or dropped
                            sdRingBufferTail += bytesSent;
                            if (sdRingBufferTail >= settings.gnssHandlerBufferSize)
                                sdRingBufferTail -= settings.gnssHandlerBufferSize;

                            if (bytesSent != sendTheseBytes)
                            {
                                systemPrintf("SD write mismatch (1) @ %s: wrote %d bytes of %d\r\n",
                                             getTimeStamp(), bytesSent, sendTheseBytes);
                                break; // Exit the do loop
                            }

                            // If we have more data to write - and the first write was successful
                            if (bytesToSend > sendTheseBytes)
                            {
                                sendTheseBytes = bytesToSend - sendTheseBytes;

                                bytesSent = logFile->write(&ringBuffer[sdRingBufferTail], sendTheseBytes);

                                // Account for the sent data or dropped
                                sdRingBufferTail += bytesSent;
                                if (sdRingBufferTail >= settings.gnssHandlerBufferSize) // Should be redundant
                                    sdRingBufferTail -= settings.gnssHandlerBufferSize;

                                if (bytesSent != sendTheseBytes)
                                {
                                    systemPrintf("SD write mismatch (2) @ %s: wrote %d bytes of %d\r\n",
                                                 getTimeStamp(), bytesSent, sendTheseBytes);
                                    break; // Exit the do loop
                                }
                            }

                            if (PERIODIC_DISPLAY(PD_SD_LOG_WRITE) && (bytesSent > 0) && (!inMainMenu))
                            {
                                PERIODIC_CLEAR(PD_SD_LOG_WRITE);
                                systemPrintf("SD %d bytes written to log file\r\n", bytesToSend);
                            }

                            sdFreeSpace -= bytesToSend; // Update remaining space on SD

                            // Record any pending trigger events
                            if (newEventToRecord == true)
                            {
                                newEventToRecord = false;

                                if ((settings.enablePrintLogFileStatus) && (!inMainMenu))
                                    systemPrintln("Log file: recording event");

                                // Record trigger count with Time Of Week of rising edge (ms), Millisecond fraction of Time Of Week of
                                // rising edge (ns), and accuracy estimate (ns)
                                char eventData[82]; // Max NMEA sentence length is 82
                                snprintf(eventData, sizeof(eventData), "%d,%d,%d,%d", triggerCount, triggerTowMsR, triggerTowSubMsR,
                                        triggerAccEst);

                                char nmeaMessage[82]; // Max NMEA sentence length is 82
                                createNMEASentence(CUSTOM_NMEA_TYPE_EVENT, nmeaMessage, sizeof(nmeaMessage),
                                                eventData); // textID, buffer, sizeOfBuffer, text

                                logFile->write(nmeaMessage, strlen(nmeaMessage));
                                const char *crlf = "\r\n";
                                logFile->write(crlf, 2);

                                sdFreeSpace -= strlen(nmeaMessage) + 2; // Update remaining space on SD
                            }

                            // Record the Antenna Reference Position - if available
                            if (newARPAvailable == true && settings.enableARPLogging &&
                                ((millis() - lastARPLog) > (settings.ARPLoggingInterval_s * 1000)))
                            {
                                lastARPLog = millis();
                                newARPAvailable = false; // Clear flag. It doesn't matter if the ARP cannot be logged

                                double x = ARPECEFX;
                                x /= 10000.0; // Convert to m
                                double y = ARPECEFY;
                                y /= 10000.0; // Convert to m
                                double z = ARPECEFZ;
                                z /= 10000.0; // Convert to m
                                double h = ARPECEFH;
                                h /= 10000.0;     // Convert to m
                                char ARPData[82]; // Max NMEA sentence length is 82
                                snprintf(ARPData, sizeof(ARPData), "%.4f,%.4f,%.4f,%.4f", x, y, z, h);

                                if ((settings.enablePrintLogFileStatus) && (!inMainMenu))
                                    systemPrintf("Log file: recording Antenna Reference Position %s\r\n", ARPData);

                                char nmeaMessage[82]; // Max NMEA sentence length is 82
                                createNMEASentence(CUSTOM_NMEA_TYPE_ARP_ECEF_XYZH, nmeaMessage, sizeof(nmeaMessage),
                                                ARPData); // textID, buffer, sizeOfBuffer, text

                                logFile->write(nmeaMessage, strlen(nmeaMessage));
                                const char *crlf = "\r\n";
                                logFile->write(crlf, 2);

                                sdFreeSpace -= strlen(nmeaMessage) + 2; // Update remaining space on SD
                            }

                            logFileSize = logFile->fileSize(); // Update file size

                            // Force file sync every 60s
                            if ((millis() - lastUBXLogSyncTime) > 60000)
                            {
                                baseStatusLedBlink(); // Blink LED to indicate logging activity

                                logFile->sync();
                                sdUpdateFileAccessTimestamp(logFile); // Update the file access time & date

                                baseStatusLedBlink(); // Blink LED to indicate logging activity

                                lastUBXLogSyncTime = millis();
                            }

                            // Remember the maximum transfer time
                            deltaMillis = millis() - startMillis;
                            if (maxMillis[RBC_SD_CARD] < deltaMillis)
                                maxMillis[RBC_SD_CARD] = deltaMillis;

                            if (settings.enablePrintBufferOverrun)
                            {
                                if (deltaMillis > 150)
                                    systemPrintf("Long Write! Time: %ld ms / Location: %ld / Recorded %d bytes / "
                                                "spaceRemaining %d bytes\r\n",
                                                deltaMillis, logFileSize, bytesToSend, combinedSpaceRemaining);
                            }
                        } while(0);

                        xSemaphoreGive(sdCardSemaphore);
                    } // End sdCardSemaphore
                    else
                    {
                        char semaphoreHolder[50];
                        getSemaphoreFunction(semaphoreHolder);
                        log_w("sdCardSemaphore failed to yield for SD write, held by %s, Tasks.ino line %d",
                            semaphoreHolder, __LINE__);

                        feedWdt();
                        taskYIELD();
                    }

                    // Update space available for use in UART task
                    bytesToSend = dataHead - sdRingBufferTail;
                    if (bytesToSend < 0)
                        bytesToSend += settings.gnssHandlerBufferSize;
                    if (usedSpace < bytesToSend)
                    {
                        usedSpace = bytesToSend;
                        slowConsumer = "SD card";
                    }
                } // bytesToSend
            } // End connected

            //----------------------------------------------------------------------
            // Update the available space in the ring buffer
            //----------------------------------------------------------------------

            freeSpace = settings.gnssHandlerBufferSize - usedSpace;

            // Don't fill the last byte to prevent buffer overflow
            if (freeSpace)
                freeSpace -= 1;
            availableHandlerSpace = freeSpace;

            //----------------------------------------------------------------------
            // Display the millisecond values for the different ring buffer consumers
            //----------------------------------------------------------------------

            if (PERIODIC_DISPLAY(PD_RING_BUFFER_MILLIS) && !inMainMenu)
            {
                int milliseconds;
                int seconds;

                PERIODIC_CLEAR(PD_RING_BUFFER_MILLIS);
                for (int index = 0; index < RBC_MAX; index++)
                {
                    milliseconds = maxMillis[index];
                    if (milliseconds > 1)
                    {
                        seconds = milliseconds / MILLISECONDS_IN_A_SECOND;
                        milliseconds %= MILLISECONDS_IN_A_SECOND;
                        systemPrintf("%s: %d:%03d Sec\r\n", ringBufferConsumer[index], seconds, milliseconds);
                    }
                }
            }

            //----------------------------------------------------------------------
            // processUart1Message will be chomping at the bit. Let it fly!
            //----------------------------------------------------------------------

            xSemaphoreGive(ringBufferSemaphore);
        }
        else
        {
            systemPrintf("handleGnssDataTask could not get ringBuffer semaphore - held by %s\r\n", ringBufferSemaphoreHolder);
        }

        //----------------------------------------------------------------------
        // Let other tasks run, prevent watch dog timer (WDT) resets
        //----------------------------------------------------------------------

        feedWdt();
        taskYIELD();
    }

    // Stop notification
    if (settings.printTaskStartStop)
        systemPrintln("Task handleGnssDataTask stopped");
    task.handleGnssDataTaskRunning = false;
    vTaskDelete(NULL);
}

// Control Bluetooth LED on variants
// This is only called if ticker task is started so no pin tests are done
void tickerBluetoothLedUpdate()
{
    // If we are in WiFi config mode, fade LED
    if (inWebConfigMode() == true)
    {
        // Fade in/out the BT LED during WiFi AP mode
        btFadeLevel += pwmFadeAmount;
        if (btFadeLevel <= 0 || btFadeLevel >= 255)
            pwmFadeAmount *= -1;

        if (btFadeLevel > 255)
            btFadeLevel = 255;
        if (btFadeLevel < 0)
            btFadeLevel = 0;

        ledcWrite(pin_bluetoothStatusLED, btFadeLevel);
    }
    // Blink on/off while we wait for BT connection
    else if (bluetoothGetState() == BT_NOTCONNECTED)
    {
        if (btFadeLevel == 0)
            btFadeLevel = 255;
        else
            btFadeLevel = 0;
        ledcWrite(pin_bluetoothStatusLED, btFadeLevel);
    }
    // Solid LED if BT Connected
    else if (bluetoothGetState() == BT_CONNECTED)
        ledcWrite(pin_bluetoothStatusLED, 255);
    else
        ledcWrite(pin_bluetoothStatusLED, 0);
}

// Control GNSS LED on variants
void tickerGnssLedUpdate()
{
    static uint8_t ledCallCounter = 0; // Used to calculate a 50% or 10% on rate for blinking

    ledCallCounter++;
    ledCallCounter %= gnssTaskUpdatesHz; // Wrap to X calls per 1 second

    if (productVariant == RTK_TORCH || productVariant == RTK_TORCH_X2)
    {
        // Update the GNSS LED according to our state

        // Solid once RTK Fix is achieved, or PPP converges
        if (gnss->isRTKFix() == true || gnss->isPppConverged())
        {
            ledcWrite(pin_gnssStatusLED, 255);
        }
        else
        {
            ledcWrite(pin_gnssStatusLED, 0);
        }
    }
}

// Control Battery LED on variants
void tickerBatteryLedUpdate()
{
    static uint8_t batteryCallCounter = 0; // Used to calculate a 50% or 10% on rate for blinking

    batteryCallCounter++;
    batteryCallCounter %= batteryTaskUpdatesHz; // Wrap to X calls per 1 second

    if (productVariant == RTK_TORCH || productVariant == RTK_TORCH_X2)
    {
        // Update the Battery LED according to the battery level

        // Solid LED when fuel level is above 50%
        if (batteryLevelPercent > 50)
        {
            ledcWrite(pin_batteryStatusLED, 255);
        }
        // Blink a short blink to indicate battery is depleting
        else
        {
            if (batteryCallCounter == (batteryTaskUpdatesHz / 10)) // On for 1/10th of a second
                ledcWrite(pin_batteryStatusLED, 255);
            else
                ledcWrite(pin_batteryStatusLED, 0);
        }
    }
}

enum BeepState
{
    BEEP_OFF = 0,
    BEEP_ON,
    BEEP_QUIET,
};
BeepState beepState = BEEP_OFF;

// Control the length of time the beeper makes noise
// We move through a simple state machine in order to handle multiple types of beeps (see beepMultiple())
void tickerBeepUpdate()
{
    if (present.beeper == true)
    {
        switch (beepState)
        {
        default:
            if (beepState != BEEP_OFF)
                beepState = BEEP_OFF;
            break;

        case BEEP_OFF:
            if (beepLengthMs > 0)
            {
                beepNextEventMs = millis() + beepLengthMs;
                beepOn();
                beepState = BEEP_ON;
            }
            break;

        case BEEP_ON:
            // https://stackoverflow.com/a/3097744 - see issue #742
            if (!((long)(beepNextEventMs - millis()) > 0))
            {
                if (beepCount == 1)
                {
                    beepLengthMs = 0; // Stop state machine
                    beepState = BEEP_OFF;
                    beepOff();
                }
                else
                {
                    beepNextEventMs = millis() + beepQuietLengthMs;
                    beepState = BEEP_QUIET;
                    beepOff();
                }
            }
            break;

        case BEEP_QUIET:
            if (!((long)(beepNextEventMs - millis()) > 0))
            {
                beepCount--;

                if (beepCount == 0)
                {
                    // We should not be here, but just in case
                    beepLengthMs = 0; // Stop state machine
                    beepState = BEEP_OFF;
                    beepOff();
                }
                else
                {
                    beepNextEventMs = millis() + beepLengthMs;
                    beepState = BEEP_ON;
                    beepOn();
                }
            }
            break;
        }
    }
}

// Monitor momentary buttons
void buttonCheckTask(void *e)
{
    // Record the time of the most recent two button releases
    // This allows us to detect single and double presses
    unsigned long doubleTapInterval = 250; // User must press and release twice within this to create a double tap

    if (present.imu_im19 && (present.display_type == DISPLAY_MAX_NONE))
        doubleTapInterval = 1000; // We are only interested in double taps, so use a longer interval

    unsigned long previousButtonRelease = 0;
    unsigned long thisButtonRelease = 0;
    bool singleTap = false;
    bool doubleTap = false;

    bool showMenu = false;

    // Start notification
    task.buttonCheckTaskRunning = true;
    if (settings.printTaskStartStop)
        systemPrintln("Task buttonCheckTask started");

    // Run task until a request is raised
    task.buttonCheckTaskStopRequest = false;
    while (task.buttonCheckTaskStopRequest == false)
    {
        // Display an alive message
        if (PERIODIC_DISPLAY(PD_TASK_BUTTON_CHECK) && !inMainMenu)
        {
            PERIODIC_CLEAR(PD_TASK_BUTTON_CHECK);
            systemPrintln("ButtonCheckTask running");
        }

        buttonRead();

        // Begin button checking

        if (buttonReleased() == true) // If a button release is detected, record it
        {
            previousButtonRelease = thisButtonRelease;
            thisButtonRelease = millis();

            // If we are not currently showing the menu, immediately display it
            if (showMenu == false && systemState != STATE_DISPLAY_SETUP)
                showMenu = true;
        }

        if ((previousButtonRelease > 0) && (thisButtonRelease > 0) &&
            ((thisButtonRelease - previousButtonRelease) <= doubleTapInterval)) // Do we have a double tap?
        {
            // Do not register button tap until the system is displaying the menu
            // If this platform doesn't have a display, then register the button tap
            if (systemState == STATE_DISPLAY_SETUP || present.display_type == DISPLAY_MAX_NONE)
            {
                doubleTap = true;
                singleTap = false;
                previousButtonRelease = 0;
                thisButtonRelease = 0;
            }
        }
        else if ((thisButtonRelease > 0) &&
                 ((millis() - thisButtonRelease) > doubleTapInterval)) // Do we have a single tap?
        {
            // Do not register button tap until the system is displaying the menu
            // If this platform doesn't have a display, then register the button tap
            if (systemState == STATE_DISPLAY_SETUP || present.display_type == DISPLAY_MAX_NONE)
            {
                previousButtonRelease = 0;
                thisButtonRelease = 0;
                doubleTap = false;

                if (firstButtonThrownOut == false)
                    firstButtonThrownOut = true; // Throw away the first button press
                else
                    singleTap = true;
            }
        }

        // else // if ((previousButtonRelease == 0) && (thisButtonRelease > 0)) // Tap in progress?
        else if ((millis() - previousButtonRelease) > 2000) // No user interaction
        {
            doubleTap = false;
            singleTap = false;
        }

        // If user presses the center button or right, act as double tap (select)
        if (buttonLastPressed() == gpioExpander_center || buttonLastPressed() == gpioExpander_right)
        {
            doubleTap = true;
            singleTap = false;
            previousButtonRelease = 0;
            thisButtonRelease = 0;

            gpioExpander_lastReleased = 255; // Reset for the next read
        }

        // End button checking

        // If in direct connect mode. Note: this is just a flag not a STATE. 
        if (inDirectConnectMode)
        {
            // TODO: check if this works on both Torch and Flex. Note: Flex does not yet support buttons
            if (singleTap || doubleTap)
            {
                // Beep to indicate exit
                beepOn();
                delay(300);
                beepOff();
                delay(100);
                beepOn();
                delay(300);
                beepOff();

                // Remove all the special files
                removeUpdateLoraFirmware();
                um980FirmwareRemoveUpdate();
                gnssFirmwareRemoveUpdate();

                systemPrintln("Exiting direct connection (passthrough) mode");
                systemFlush(); // Complete prints

                ESP.restart();
            }
        }
        // Torch is a special case. Handle tilt stop and web config mode
        else if (productVariant == RTK_TORCH)
        //else if (present.imu_im19 && (present.display_type == DISPLAY_MAX_NONE)) // TODO delete me
        {
            // Platform has no display and tilt corrections, ie RTK Torch

            // In in tilt mode, exit on button press
            if ((singleTap || doubleTap) && (tiltIsCorrecting() == true))
            {
                tiltRequestStop(); // Don't force the hardware off here as it may be in use in another task
            }

            else if (doubleTap)
            {
                // If we are in Rover/Base mode, enter WiFi Config Mode
                if (inRoverMode() || inBaseMode())
                {
                    // Beep if we are not locally compiled or a release candidate
                    if (ENABLE_DEVELOPER == false)
                    {
                        beepOn();
                        delay(300);
                        beepOff();
                        delay(100);
                        beepOn();
                        delay(300);
                        beepOff();
                    }

                    forceSystemStateUpdate = true; // Immediately go to this new state
                    changeState(STATE_WEB_CONFIG_NOT_STARTED);
                }

                // If we are in WiFi Config Mode, exit to Rover
                else if (inWebConfigMode())
                {
                    // Beep if we are not locally compiled or a release candidate
                    if (ENABLE_DEVELOPER == false)
                    {
                        beepOn();
                        delay(300);
                        beepOff();
                        delay(100);
                        beepOn();
                        delay(300);
                        beepOff();
                    }

                    forceSystemStateUpdate = true; // Immediately go to this new state
                    changeState(STATE_ROVER_NOT_STARTED);
                }
            }

            // The RTK Torch uses a shutdown IC configured to turn off ~3s
            // Beep shortly before the shutdown IC takes over
            else if (buttonPressedFor(2100) == true)
            {
                systemPrintln("Shutting down");

                tickerStop(); // Stop controlling LEDs via ticker task

                pinMode(pin_gnssStatusLED, OUTPUT);
                pinMode(pin_bluetoothStatusLED, OUTPUT);

                gnssStatusLedOn();
                bluetoothLedOn();

                // Beep if we are not locally compiled or a release candidate
                if (ENABLE_DEVELOPER == false)
                {
                    // Announce powering down
                    beepMultiple(3, 100, 50); // Number of beeps, length of beep ms, length of quiet ms

                    delay(500); // We will be shutting off during this delay but this prevents another beepMultiple()
                                // from firing
                }

                while (1)
                    ;
            }
        } // End productVariant == Torch
        else // RTK EVK, RTK Facet v2, RTK Facet mosaic, RTK Postcard
        {
            if (systemState == STATE_SHUTDOWN)
            {
                // Ignore button presses while shutting down
            }
            else if (buttonPressedFor(shutDownButtonTime))
            {
                forceSystemStateUpdate = true;
                requestChangeState(STATE_SHUTDOWN);

                if (inMainMenu)
                    powerDown(true); // State machine is not updated while in menu system so go straight to power down
                                     // as needed
            }
            else if ((systemState == STATE_BASE_NOT_STARTED) && (firstRoverStart == true) &&
                     (buttonPressedFor(500) == true))
            {
                lastSetupMenuChange = millis(); // Prevent a timeout during state change
                forceSystemStateUpdate = true;
                requestChangeState(STATE_TEST);
            }

            // If the button is disabled, do nothing
            // If we detect a singleTap, move through menus
            // If the button was pressed to initially show the menu, then allow immediate entry and show the menu
            else if ((settings.disableSetupButton == false) && ((singleTap && firstRoverStart == false) || showMenu))
            {
                switch (systemState)
                {
                // If we are in any running state, change to STATE_DISPLAY_SETUP
                case STATE_ROVER_NOT_STARTED:
                case STATE_ROVER_NO_FIX:
                case STATE_ROVER_FIX:
                case STATE_ROVER_RTK_FLOAT:
                case STATE_ROVER_RTK_FIX:
                case STATE_BASE_NOT_STARTED:
                case STATE_BASE_TEMP_SETTLE:
                case STATE_BASE_TEMP_SURVEY_STARTED:
                case STATE_BASE_TEMP_TRANSMITTING:
                case STATE_BASE_FIXED_NOT_STARTED:
                case STATE_BASE_FIXED_TRANSMITTING:
                case STATE_WEB_CONFIG_NOT_STARTED:
                case STATE_WEB_CONFIG:
                case STATE_ESPNOW_PAIRING_NOT_STARTED:
                case STATE_ESPNOW_PAIRING:
                case STATE_NTPSERVER_NOT_STARTED:
                case STATE_NTPSERVER_NO_SYNC:
                case STATE_NTPSERVER_SYNC:
                    lastSystemState = systemState; // Remember this state to return if needed
                    requestChangeState(STATE_DISPLAY_SETUP);
                    lastSetupMenuChange = millis();
                    setupSelectedButton = 0; // Highlight the first button
                    showMenu = false;
                    break;

                case STATE_DISPLAY_SETUP:
                    // If we are displaying the setup menu, a single tap will cycle through possible system states
                    // Exit into new system state on double tap - see below
                    // Exit display setup into previous state after ~10s - see updateSystemState()
                    lastSetupMenuChange = millis();

                    forceDisplayUpdate = true; // User is interacting so repaint display quickly

                    if (online.gpioExpanderButtons == true)
                    {
                        // React to five different buttons
                        if (buttonLastPressed() == gpioExpander_up || buttonLastPressed() == gpioExpander_left)
                        {
                            if (setupSelectedButton == 0) // Top reached?
                                setupSelectedButton = setupButtons.size() - 1;
                            else
                                setupSelectedButton--;
                        }
                        else if (buttonLastPressed() == gpioExpander_down)
                        {
                            setupSelectedButton++;
                            if (setupSelectedButton == setupButtons.size()) // Limit reached?
                                setupSelectedButton = 0;
                        }
                    }
                    else
                    {
                        // React to single mode/setup button
                        setupSelectedButton++;
                        if (setupSelectedButton == setupButtons.size()) // Limit reached?
                            setupSelectedButton = 0;
                    }

                    break;

                case STATE_TEST:
                    // Do nothing. User is releasing the setup button.
                    break;

                case STATE_TESTING:
                    // If we are in testing, return to Base Not Started
                    lastSetupMenuChange = millis(); // Prevent a timeout during state change
                    baseCasterDisableOverride();    // Leave Caster mode
                    requestChangeState(STATE_BASE_NOT_STARTED);
                    break;

                case STATE_PROFILE:
                    // If the user presses the setup button during a profile change, do nothing
                    // Allow system to return to lastSystemState
                    break;

                default:
                    systemPrintf("buttonCheckTask single tap - untrapped system state: %d\r\n", systemState);
                    // requestChangeState(STATE_BASE_NOT_STARTED);
                    break;
                } // End singleTap switch (systemState)
            } // End singleTap
            else if (doubleTap && (firstRoverStart == false) && (settings.disableSetupButton == false))
            {
                switch (systemState)
                {
                case STATE_DISPLAY_SETUP:
                {
                    // If we are displaying the setup menu, a single tap will cycle through possible system states - see
                    // above Exit into new system state on double tap Exit display setup into previous state after ~10s
                    // - see updateSystemState()
                    lastSetupMenuChange = millis(); // Prevent a timeout during state change
                    uint8_t thisIsButton = 0;
                    for (auto it = setupButtons.begin(); it != setupButtons.end(); it = std::next(it))
                    {
                        if (thisIsButton == setupSelectedButton)
                        {
                            if (it->newState == STATE_PROFILE)
                            {
                                displayProfile = it->newProfile; // paintProfile needs the unit
                                requestChangeState(STATE_PROFILE);
                            }
                            else if (it->newState == STATE_NOT_SET) // Exit
                            {
                                firstButtonThrownOut = false;
                                requestChangeState(lastSystemState);
                            }
                            else if (it->newState ==
                                     STATE_BASE_NOT_STARTED) // User selected Base, clear BaseCast override
                            {
                                baseCasterDisableOverride();
                                requestChangeState(it->newState);
                            }
                            else
                                requestChangeState(it->newState);

                            break;
                        }
                        thisIsButton++;
                    }
                }
                break;

                default:
                    systemPrintf("buttonCheckTask double tap - untrapped system state: %d\r\n", systemState);
                    // requestChangeState(STATE_BASE_NOT_STARTED);
                    break;
                } // End doubleTap switch (systemState)
            } // End doubleTap
        } // End productVariant != Torch

        feedWdt();
        taskYIELD();
    } // End while (task.buttonCheckTaskStopRequest == false)

    // Stop notification
    if (settings.printTaskStartStop)
        systemPrintln("Task buttonCheckTask stopped");
    task.buttonCheckTaskRunning = false;
    vTaskDelete(NULL);
}

void idleTask(void *e)
{
    int cpu = xPortGetCoreID();
    volatile bool *idleTaskRunning;
    uint32_t idleCount = 0;
    uint32_t lastDisplayIdleTime = 0;
    uint32_t lastStackPrintTime = 0;

    // Start notification
    idleTaskRunning = cpu ? &task.idleTask1Running : &task.idleTask0Running;
    *idleTaskRunning = true;
    if (settings.printTaskStartStop)
        systemPrintf("Task idleTask%d started\r\n", cpu);

    // Verify that the task is still running
    while (*idleTaskRunning)
    {
        // Increment a count during the idle time
        idleCount++;

        // Determine if it is time to print the CPU idle times
        if ((millis() - lastDisplayIdleTime) >= (IDLE_TIME_DISPLAY_SECONDS * 1000) && !inMainMenu)
        {
            lastDisplayIdleTime = millis();

            // Get the idle time
            if (idleCount > max_idle_count)
                max_idle_count = idleCount;

            // Display the idle times
            if (settings.enablePrintIdleTime)
            {
                systemPrintf("CPU %d idle time: %d%% (%d/%d)\r\n", cpu, idleCount * 100 / max_idle_count, idleCount,
                             max_idle_count);

                // Print the task count
                if (cpu)
                    systemPrintf("%d Tasks\r\n", uxTaskGetNumberOfTasks());
            }

            // Restart the idle count for the next display time
            idleCount = 0;
        }

        // Display the high water mark if requested
        if ((settings.enableTaskReports == true) &&
            ((millis() - lastStackPrintTime) >= (IDLE_TIME_DISPLAY_SECONDS * 1000)))
        {
            lastStackPrintTime = millis();
            systemPrintf("idleTask %d High watermark: %d\r\n", xPortGetCoreID(), uxTaskGetStackHighWaterMark(nullptr));
        }

        // The idle task should NOT delay or yield
    }

    // Stop notification
    if (settings.printTaskStartStop)
        systemPrintf("Task idleTask%d stopped\r\n", cpu);
    *idleTaskRunning = false;
    vTaskDelete(NULL);
}

// Serial Read/Write tasks for the GNSS receiver must be started after BT is up and running otherwise
// SerialBT->available will cause reboot
bool tasksStartGnssUart()
{
    TaskHandle_t taskHandle;

    if (present.gnss_to_uart == false)
        return (true);

    // Verify that the ring buffer was successfully allocated
    if (!ringBuffer)
    {
        systemPrintln("ERROR: Ring buffer allocation failure!");
        systemPrintln("Decrease GNSS handler (ring) buffer size");
        displayNoRingBuffer(5000);
        return false;
    }

    availableHandlerSpace = settings.gnssHandlerBufferSize;

    // Reads data from GNSS and stores data into circular buffer
    if (!task.gnssReadTaskRunning)
        xTaskCreatePinnedToCore(gnssReadTask,                  // Function to call
                                "gnssRead",                    // Just for humans
                                gnssReadTaskStackSize,         // Stack Size
                                nullptr,                       // Task input parameter
                                settings.gnssReadTaskPriority, // Priority
                                &taskHandle,                   // Task handle
                                settings.gnssReadTaskCore);    // Core where task should run, 0=core, 1=Arduino

    // Reads data from circular buffer and sends data to SD, SPP, or network clients
    if (!task.handleGnssDataTaskRunning)
        xTaskCreatePinnedToCore(handleGnssDataTask,                  // Function to call
                                "handleGNSSData",                    // Just for humans
                                handleGnssDataTaskStackSize,         // Stack Size
                                nullptr,                             // Task input parameter
                                settings.handleGnssDataTaskPriority, // Priority
                                &taskHandle,                         // Task handle
                                settings.handleGnssDataTaskCore);    // Core where task should run, 0=core, 1=Arduino

    // Reads data from BT and sends to GNSS
    if (!task.btReadTaskRunning)
        xTaskCreatePinnedToCore(btReadTask,                  // Function to call
                                "btRead",                    // Just for humans
                                btReadTaskStackSize,         // Stack Size
                                nullptr,                     // Task input parameter
                                settings.btReadTaskPriority, // Priority
                                &taskHandle,                 // Task handle
                                settings.btReadTaskCore);    // Core where task should run, 0=core, 1=Arduino
    return true;
}

// Stop tasks - useful when running firmware update or WiFi AP is running
void tasksStopGnssUart()
{
    // Stop tasks if running
    task.gnssReadTaskStopRequest = true;
    task.handleGnssDataTaskStopRequest = true;
    task.btReadTaskStopRequest = true;

    // Give the other CPU time to finish
    // Eliminates CPU bus hang condition
    do
        delay(10);
    while (task.gnssReadTaskRunning || task.handleGnssDataTaskRunning || task.btReadTaskRunning);
}

// Checking the number of available clusters on the SD card can take multiple seconds
// Rather than blocking the system, we run a background task
// Once the size check is complete, the task is removed
void sdSizeCheckTask(void *e)
{
    // Start notification
    task.sdSizeCheckTaskRunning = true;
    if (settings.printTaskStartStop)
        systemPrintln("Task sdSizeCheckTask started");

    // Run task until a request is raised
    task.sdSizeCheckTaskStopRequest = false;
    while (task.sdSizeCheckTaskStopRequest == false)
    {
        // Display an alive message
        if (PERIODIC_DISPLAY(PD_TASK_SD_SIZE_CHECK) && !inMainMenu)
        {
            PERIODIC_CLEAR(PD_TASK_SD_SIZE_CHECK);
            systemPrintln("sdSizeCheckTask running");
        }

        if (online.microSD && sdCardSize == 0)
        {
            // Attempt to gain access to the SD card
            if (xSemaphoreTake(sdCardSemaphore, fatSemaphore_longWait_ms) == pdPASS)
            {
                markSemaphore(FUNCTION_SDSIZECHECK);

                csd_t csd;
                sd->card()->readCSD(&csd); // Card Specific Data
                sdCardSize = (uint64_t)512 * sd->card()->sectorCount();

                sd->volumeBegin();

                // Find available cluster/space
                sdFreeSpace = sd->vol()->freeClusterCount(); // This takes a few seconds to complete
                sdFreeSpace *= sd->vol()->sectorsPerCluster();
                sdFreeSpace *= 512L; // Bytes per sector

                xSemaphoreGive(sdCardSemaphore);

                // uint64_t sdUsedSpace = sdCardSize - sdFreeSpace; //Don't think of it as used, think of it as unusable

                String cardSize;
                stringHumanReadableSize(cardSize, sdCardSize);
                String freeSpace;
                stringHumanReadableSize(freeSpace, sdFreeSpace);
                systemPrintf("SD card size: %s / Free space: %s\r\n", cardSize, freeSpace);

                outOfSDSpace = false;

                sdSizeCheckTaskComplete = true;
            }
            else
            {
                char semaphoreHolder[50];
                getSemaphoreFunction(semaphoreHolder);
                log_d("sdCardSemaphore failed to yield, held by %s, Tasks.ino line %d\r\n", semaphoreHolder, __LINE__);
            }
        }

        feedWdt();
        taskYIELD(); // Let other tasks run
    }

    // Stop notification
    if (settings.printTaskStartStop)
        systemPrintln("Task sdSizeCheckTask stopped");
    task.sdSizeCheckTaskRunning = false;
    vTaskDelete(NULL);
}

// Validate the task table lengths
void tasksValidateTables()
{
    if (ringBufferConsumerEntries != RBC_MAX)
        reportFatalError("Fix ringBufferConsumer table to match RingBufferConsumers");
}

// Monitor the 2nd BleSerial port (bluetoothSerialBleCommands) for incoming serial
// Read incoming serial until \r\n is received, then pass to command processor
void bluetoothCommandTask(void *pvParameters)
{
    int rxSpot = 0;
    char rxData[256]; // Input limit of 256 chars

    // Start notification
    task.bluetoothCommandTaskRunning = true;
    if (settings.printTaskStartStop)
        systemPrintln("Task bluetoothCommandTask started");

    // Run task until a request is raised
    task.bluetoothCommandTaskStopRequest = false;
    while (task.bluetoothCommandTaskStopRequest == false)
    {
        // Display an alive message
        if (PERIODIC_DISPLAY(PD_TASK_BLUETOOTH_READ) && !inMainMenu)
        {
            PERIODIC_CLEAR(PD_TASK_BLUETOOTH_READ);
            systemPrintln("bluetoothCommandTask running");
        }

        // Check stream for incoming characters
        if (bluetoothCommandAvailable() > 0)
        {
            byte incoming = bluetoothCommandRead();

            rxData[rxSpot++] = incoming;
            rxSpot %= sizeof(rxData); // Wrap

            // Verify presence of trailing \r\n
            if (rxSpot > 2 && rxData[rxSpot - 1] == '\n' && rxData[rxSpot - 2] == '\r')
            {
                rxData[rxSpot - 2] = '\0'; // Remove \r\n
                processCommand(rxData);
                rxSpot = 0; // Reset
            }
        }

        if (PERIODIC_DISPLAY(PD_BLUETOOTH_DATA_RX) && !inMainMenu)
        {
            PERIODIC_CLEAR(PD_BLUETOOTH_DATA_RX);
            systemPrintf("Bluetooth received %d bytes\r\n", rxSpot);
        }

        if ((settings.enableTaskReports == true) && (!inMainMenu))
            systemPrintf("bluetoothCommandTask High watermark: %d\r\n", uxTaskGetStackHighWaterMark(nullptr));

        feedWdt();
        taskYIELD();
    } // End while(true)

    // Stop notification
    if (settings.printTaskStartStop)
        systemPrintln("Task bluetoothCommandTask stopped");
    task.bluetoothCommandTaskRunning = false;
    vTaskDelete(NULL);
}

void beginRtcmParse()
{
    // Begin the RTCM parser - which will extract the base location from RTCM1005 / 1006
    rtcmParse = sempBeginParser(rtcmParserTable, rtcmParserCount, rtcmParserNames, rtcmParserNameCount,
                               0,                   // Scratchpad bytes
                               1050,                // Buffer length
                               processRTCMMessage, // eom Call Back
                               "rtcmParse");         // Parser Name
    if (!rtcmParse)
        reportFatalError("Failed to initialize the RTCM parser");

    if (settings.debugNtripClientRtcm)
    {
        sempEnableDebugOutput(rtcmParse);
        sempPrintParserConfiguration(rtcmParse);
    }
}

// Check and record the base location in RTCM1005/1006
void processRTCMMessage(SEMP_PARSE_STATE *parse, uint16_t type)
{
    SEMP_SCRATCH_PAD *scratchPad = (SEMP_SCRATCH_PAD *)parse->scratchPad;

    if (sempRtcmGetMessageNumber(parse) == 1005)
    {
        ARPECEFX = sempRtcmGetSignedBits(parse, 34, 38);
        ARPECEFY = sempRtcmGetSignedBits(parse, 74, 38);
        ARPECEFZ = sempRtcmGetSignedBits(parse, 114, 38);
        ARPECEFH = 0;
        newARPAvailable = true;
    }

    if (sempRtcmGetMessageNumber(parse) == 1006)
    {
        ARPECEFX = sempRtcmGetSignedBits(parse, 34, 38);
        ARPECEFY = sempRtcmGetSignedBits(parse, 74, 38);
        ARPECEFZ = sempRtcmGetSignedBits(parse, 114, 38);
        ARPECEFH = sempRtcmGetUnsignedBits(parse, 152, 16);
        newARPAvailable = true;
    }
}