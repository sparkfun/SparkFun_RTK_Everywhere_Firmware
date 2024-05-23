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
                          SPI       or      I2C
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

#define WRAP_OFFSET(offset, increment, arraySize)                                                                      \
    {                                                                                                                  \
        offset += increment;                                                                                           \
        if (offset >= arraySize)                                                                                       \
            offset -= arraySize;                                                                                       \
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
    "Bluetooth", "TCP Client", "TCP Server", "SD Card", "UDP Server", "USB Serial",
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
    sempNmeaPreamble, sempUnicoreHashPreamble, sempRtcmPreamble, sempUbloxPreamble, sempUnicoreBinaryPreamble,
};
const int parserCount = sizeof(parserTable) / sizeof(parserTable[0]);

// List the names of the parsers
const char *const parserNames[] = {
    "NMEA", "Unicore Hash_(#)", "RTCM", "u-Blox", "Unicore Binary",
};
const int parserNameCount = sizeof(parserNames) / sizeof(parserNames[0]);

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
        2000; // Bluetooth serial traffic must stop this amount before an escape char is recognized
    uint8_t btEscapeCharsReceived = 0; // Used to enter remote command mode

    uint8_t btAppCommandCharsReceived = 0; // Used to enter app command mode

    // Start notification
    task.btReadTaskRunning = true;
    if (settings.printTaskStartStop)
        systemPrintln("Task btReadTask started");

    // Run task until a request is raised
    task.btReadTaskStopRequest = false;
    while (task.btReadTaskStopRequest == false)
    {
        // Display an alive message
        if (PERIODIC_DISPLAY(PD_TASK_BLUETOOTH_READ))
        {
            PERIODIC_CLEAR(PD_TASK_BLUETOOTH_READ);
            systemPrintln("btReadTask running");
        }

        // Receive RTCM corrections or UBX config messages over bluetooth and pass them along to GNSS
        rxBytes = 0;
        if (bluetoothGetState() == BT_CONNECTED)
        {
            while (btPrintEcho == false && bluetoothRxDataAvailable())
            {
                // Check stream for command characters
                byte incoming = bluetoothRead();
                rxBytes += 1;

                if (incoming == btEscapeCharacter)
                {
                    // Ignore escape characters received within 2 seconds of serial traffic
                    // Allow escape characters received within the first 2 seconds of power on
                    if (millis() - btLastByteReceived > btMinEscapeTime || millis() < btMinEscapeTime)
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
                else if (incoming == btAppCommandCharacter)
                {
                    btAppCommandCharsReceived++;
                    if (btAppCommandCharsReceived == btMaxAppCommandCharacters)
                    {
                        systemPrintln("Device has entered config mode over Bluetooth");
                        printEndpoint = PRINT_ENDPOINT_ALL;
                        btPrintEcho = true;
                        runCommandMode = true;

                        btAppCommandCharsReceived = 0;
                        btLastByteReceived = millis();
                    }
                }

                else // This character is not a command character, pass along to GNSS
                {
                    // Pass any escape characters that turned out to not be a complete escape sequence
                    while (btEscapeCharsReceived-- > 0)
                    {
                        addToGnssBuffer(btEscapeCharacter);
                    }
                    while (btAppCommandCharsReceived-- > 0)
                    {
                        addToGnssBuffer(btAppCommandCharacter);
                    }

                    // Pass byte to GNSS receiver or to system
                    // TODO - control if this RTCM source should be listened to or not

                    // UART RX can be corrupted by UART TX
                    // See issue: https://github.com/sparkfun/SparkFun_RTK_Firmware/issues/469
                    // serialGNSS->write(incoming);
                    addToGnssBuffer(incoming);

                    btLastByteReceived = millis();
                    btEscapeCharsReceived = 0; // Update timeout check for escape char and partial frame

                    btAppCommandCharsReceived = 0;

                    bluetoothIncomingRTCM = true;

                    // Record the arrival of RTCM from the Bluetooth connection (a phone or tablet is providing the RTCM
                    // via NTRIP). This resets the RTCM timeout used on the L-Band.
                    rtcmLastPacketReceived = millis();

                } // End just a character in the stream

            } // End btPrintEcho == false && bluetoothRxDataAvailable()

            if (PERIODIC_DISPLAY(PD_BLUETOOTH_DATA_RX))
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
    updateCorrectionsLastSeen(CORR_BLUETOOTH);
    if (isHighestRegisteredCorrectionsSource(CORR_BLUETOOTH))
    {
        if (gnssPushRawData(bluetoothOutgoingToGnss, bluetoothOutgoingToGnssHead))
        {
            if (PERIODIC_DISPLAY(PD_ZED_DATA_TX))
            {
                PERIODIC_CLEAR(PD_ZED_DATA_TX);
                systemPrintf("Sent %d BT bytes to GNSS\r\n", bluetoothOutgoingToGnssHead);
            }
            // log_d("Pushed %d bytes RTCM to GNSS", bluetoothOutgoingToGnssHead);
        }
    }
    else
    {
        if (PERIODIC_DISPLAY(PD_ZED_DATA_TX))
        {
            PERIODIC_CLEAR(PD_ZED_DATA_TX);
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
    static SEMP_PARSE_STATE *parse;

    // Start notification
    task.gnssReadTaskRunning = true;
    if (settings.printTaskStartStop)
        systemPrintln("Task gnssReadTask started");

    // Initialize the parser
    parse = sempBeginParser(parserTable, parserCount, parserNames, parserNameCount,
                            0,                   // Scratchpad bytes
                            3000,                // Buffer length
                            processUart1Message, // eom Call Back
                            "Log");              // Parser Name
    if (!parse)
        reportFatalError("Failed to initialize the parser");

    if (settings.debugGnss)
        sempEnableDebugOutput(parse);

    // Run task until a request is raised
    task.gnssReadTaskStopRequest = false;
    while (task.gnssReadTaskStopRequest == false)
    {
        // Display an alive message
        if (PERIODIC_DISPLAY(PD_TASK_GNSS_READ))
        {
            PERIODIC_CLEAR(PD_TASK_GNSS_READ);
            systemPrintln("gnssReadTask running");
        }

        if ((settings.enableTaskReports == true) && (!inMainMenu))
            systemPrintf("SerialReadTask High watermark: %d\r\n", uxTaskGetStackHighWaterMark(nullptr));

        // Two methods are accessing the hardware serial port (um980Config) at the
        // same time: gnssReadTask() (to harvest incoming serial data) and um980 (the unicore library to configure the
        // device) To allow the Unicore library to send/receive serial commands, we need to block the gnssReadTask
        // If the Unicore library does not need lone access, then read from serial port
        if (gnssIsBlocking() == false)
        {
            // Determine if serial data is available
            while (serialGNSS->available())
            {
                // Read the data from UART1
                uint8_t incomingData[500];
                int bytesIncoming = serialGNSS->read(incomingData, sizeof(incomingData));

                for (int x = 0; x < bytesIncoming; x++)
                {
                    // Update the parser state based on the incoming byte
                    sempParseNextByte(parse, incomingData[x]);
                }
            }
        }

        feedWdt();
        taskYIELD();
    }

    // Done parsing incoming data, free the parse buffer
    sempStopParser(&parse);

    // Stop notification
    if (settings.printTaskStartStop)
        systemPrintln("Task gnssReadTask stopped");
    task.gnssReadTaskRunning = false;
    vTaskDelete(NULL);
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
    if ((settings.enablePrintLogFileMessages || PERIODIC_DISPLAY(PD_ZED_DATA_RX)) && (!parse->crc) && (!inMainMenu))
    {
        PERIODIC_CLEAR(PD_ZED_DATA_RX);
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

    if (tiltIsCorrecting() == true)
    {
        if (type == RTK_NMEA_PARSER_INDEX)
        {
            parse->buffer[parse->length++] = '\0'; // Null terminate string
            tiltApplyCompensation((char *)parse->buffer, parse->length);
        }
    }

    // Determine where to send RTCM data
    if (inBaseMode() && type == RTK_RTCM_PARSER_INDEX)
    {
        // Pass data along to NTRIP Server, or ESP-NOW radio
        processRTCM(parse->buffer, parse->length);
    }

    // Determine if we are using the PPL
    if (present.gnss_um980)
    {
        // Determine if we want to use corrections, and are connected to the broker
        if (settings.enablePointPerfectCorrections && mqttClientIsConnected() == true)
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
            }
        }
    }

    // Determine if this message will fit into the ring buffer
    bytesToCopy = parse->length;
    space = availableHandlerSpace;
    use = settings.gnssHandlerBufferSize - space;
    consumer = (char *)slowConsumer;
    if ((bytesToCopy > space) && (!inMainMenu))
    {
        int32_t bufferedData;
        int32_t bytesToDiscard;
        int32_t discardedBytes;
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
        discardedBytes = 0;
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
        if (consumer)
            systemPrintf("Ring buffer full: discarding %d bytes, %s is slow\r\n", discardedBytes, consumer);
        else
            systemPrintf("Ring buffer full: discarding %d bytes\r\n", discardedBytes);
        updateRingBufferTails(previousTail, rbOffsetArray[rbOffsetTail]);
        availableHandlerSpace += discardedBytes;
    }

    // Add another message to the ring buffer
    // Account for this message
    availableHandlerSpace -= bytesToCopy;

    // Fill the buffer to the end and then start at the beginning
    if ((dataHead + bytesToCopy) > settings.gnssHandlerBufferSize)
        bytesToCopy = settings.gnssHandlerBufferSize - dataHead;

    // Display the dataHead offset
    if (settings.enablePrintRingBufferOffsets && (!inMainMenu))
        systemPrintf("DH: %4d --> ", dataHead);

    // Copy the data into the ring buffer
    memcpy(&ringBuffer[dataHead], parse->buffer, bytesToCopy);
    dataHead += bytesToCopy;
    if (dataHead >= settings.gnssHandlerBufferSize)
        dataHead -= settings.gnssHandlerBufferSize;

    // Determine the remaining bytes
    remainingBytes = parse->length - bytesToCopy;
    if (remainingBytes)
    {
        // Copy the remaining bytes into the beginning of the ring buffer
        memcpy(ringBuffer, &parse->buffer[bytesToCopy], remainingBytes);
        dataHead += remainingBytes;
        if (dataHead >= settings.gnssHandlerBufferSize)
            dataHead -= settings.gnssHandlerBufferSize;
    }

    // Add the head offset to the offset array
    WRAP_OFFSET(rbOffsetHead, 1, rbOffsetEntries);
    rbOffsetArray[rbOffsetHead] = dataHead;

    // Display the dataHead offset
    if (settings.enablePrintRingBufferOffsets && (!inMainMenu))
        systemPrintf("%4d\r\n", dataHead);
}

// Remove previous messages from the ring buffer
void updateRingBufferTails(RING_BUFFER_OFFSET previousTail, RING_BUFFER_OFFSET newTail)
{
    // Trim any long or medium tails
    discardRingBufferBytes(&btRingBufferTail, previousTail, newTail);
    discardRingBufferBytes(&sdRingBufferTail, previousTail, newTail);
    discardTcpClientBytes(previousTail, newTail);
    discardTcpServerBytes(previousTail, newTail);
    discardUdpServerBytes(previousTail, newTail);
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
    uint32_t startMillis;
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

    // Run task until a request is raised
    task.handleGnssDataTaskStopRequest = false;
    while (task.handleGnssDataTaskStopRequest == false)
    {
        // Display an alive message
        if (PERIODIC_DISPLAY(PD_TASK_HANDLE_GNSS_DATA))
        {
            PERIODIC_CLEAR(PD_TASK_HANDLE_GNSS_DATA);
            systemPrintln("handleGnssDataTask running");
        }

        usedSpace = 0;

        //----------------------------------------------------------------------
        // Send data over Bluetooth
        //----------------------------------------------------------------------

        startMillis = millis();

        // Determine BT connection state
        bool connected = (bluetoothGetState() == BT_CONNECTED) && (systemState != STATE_BASE_TEMP_SETTLE) &&
                         (systemState != STATE_BASE_TEMP_SURVEY_STARTED);
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

                // If we are in the config menu, suppress data flowing from ZED to cell phone
                if (btPrintEcho == false)
                    // Push new data to BT SPP
                    bytesToSend = bluetoothWrite(&ringBuffer[btRingBufferTail], bytesToSend);

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
                    if (PERIODIC_DISPLAY(PD_BLUETOOTH_DATA_TX))
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
        connected = online.logging && ((systemTime_minutes - startLogTime_minutes) < settings.maxLogTime_minutes);

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

                    // Reduce bytes to record if we have more then the end of the buffer
                    if ((sdRingBufferTail + bytesToSend) > settings.gnssHandlerBufferSize)
                        bytesToSend = settings.gnssHandlerBufferSize - sdRingBufferTail;

                    if (settings.enablePrintSDBuffers && (!inMainMenu))
                    {
                        int bufferAvailable = serialGNSS->available();

                        int availableUARTSpace = settings.uartReceiveBufferSize - bufferAvailable;

                        systemPrintf("SD Incoming Serial: %04d\tToRead: %04d\tMovedToBuffer: %04d\tavailableUARTSpace: "
                                     "%04d\tavailableHandlerSpace: %04d\tToRecord: %04d\tRecorded: %04d\tBO: %d\r\n",
                                     bufferAvailable, 0, 0, availableUARTSpace, availableHandlerSpace, bytesToSend, 0,
                                     bufferOverruns);
                    }

                    // Write the data to the file
                    long startTime = millis();
                    startMillis = millis();

                    bytesToSend = ubxFile->write(&ringBuffer[sdRingBufferTail], bytesToSend);
                    if (PERIODIC_DISPLAY(PD_SD_LOG_WRITE) && (bytesToSend > 0))
                    {
                        PERIODIC_CLEAR(PD_SD_LOG_WRITE);
                        systemPrintf("SD %d bytes written to log file\r\n", bytesToSend);
                    }

                    fileSize = ubxFile->fileSize(); // Update file size

                    sdFreeSpace -= bytesToSend; // Update remaining space on SD

                    // Force file sync every 60s
                    if (millis() - lastUBXLogSyncTime > 60000)
                    {
                        baseStatusLedBlink(); // Blink LED to indicate logging activity

                        ubxFile->sync();
                        sdUpdateFileAccessTimestamp(ubxFile); // Update the file access time & date

                        baseStatusLedBlink(); // Blink LED to indicate logging activity

                        lastUBXLogSyncTime = millis();
                    }

                    // Remember the maximum transfer time
                    deltaMillis = millis() - startMillis;
                    if (maxMillis[RBC_SD_CARD] < deltaMillis)
                        maxMillis[RBC_SD_CARD] = deltaMillis;
                    long endTime = millis();

                    if (settings.enablePrintBufferOverrun)
                    {
                        if (endTime - startTime > 150)
                            systemPrintf("Long Write! Time: %ld ms / Location: %ld / Recorded %d bytes / "
                                         "spaceRemaining %d bytes\r\n",
                                         endTime - startTime, fileSize, bytesToSend, combinedSpaceRemaining);
                    }

                    xSemaphoreGive(sdCardSemaphore);

                    // Account for the sent data or dropped
                    if (bytesToSend > 0)
                    {
                        sdRingBufferTail += bytesToSend;
                        if (sdRingBufferTail >= settings.gnssHandlerBufferSize)
                            sdRingBufferTail -= settings.gnssHandlerBufferSize;
                    }
                } // End sdCardSemaphore
                else
                {
                    char semaphoreHolder[50];
                    getSemaphoreFunction(semaphoreHolder);
                    log_w("sdCardSemaphore failed to yield for SD write, held by %s, Tasks.ino line %d",
                          semaphoreHolder, __LINE__);

                    delay(1); // Needed to prevent WDT resets during long Record Settings locks
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

        if (PERIODIC_DISPLAY(PD_RING_BUFFER_MILLIS))
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
        // Let other tasks run, prevent watch dog timer (WDT) resets
        //----------------------------------------------------------------------

        delay(1);
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
    if (inWiFiConfigMode() == true)
    {
        // Fade in/out the BT LED during WiFi AP mode
        btFadeLevel += pwmFadeAmount;
        if (btFadeLevel <= 0 || btFadeLevel >= 255)
            pwmFadeAmount *= -1;

        if (btFadeLevel > 255)
            btFadeLevel = 255;
        if (btFadeLevel < 0)
            btFadeLevel = 0;

        ledcWrite(ledBtChannel, btFadeLevel);
    }
    // Blink on/off while we wait for BT connection
    else if (bluetoothGetState() == BT_NOTCONNECTED)
    {
        if (btFadeLevel == 0)
            btFadeLevel = 255;
        else
            btFadeLevel = 0;
        ledcWrite(ledBtChannel, btFadeLevel);
    }
    // Solid LED if BT Connected
    else if (bluetoothGetState() == BT_CONNECTED)
        ledcWrite(ledBtChannel, 255);
    else
        ledcWrite(ledBtChannel, 0);
}

// Control GNSS LED on variants
void tickerGnssLedUpdate()
{
    static uint8_t ledCallCounter = 0; // Used to calculate a 50% or 10% on rate for blinking

    ledCallCounter++;
    ledCallCounter %= gnssTaskUpdatesHz; // Wrap to X calls per 1 second

    if (productVariant == RTK_TORCH)
    {
        // Update the GNSS LED according to our state

        // Solid once RTK Fix is achieved, or PPP converges
        if (gnssIsRTKFix() == true || gnssIsPppConverged())
        {
            ledcWrite(ledGnssChannel, 255);
        }
        else
        {
            ledcWrite(ledGnssChannel, 0);
        }
    }
}

// Control Battery LED on variants
void tickerBatteryLedUpdate()
{
    static uint8_t batteryCallCounter = 0; // Used to calculate a 50% or 10% on rate for blinking

    batteryCallCounter++;
    batteryCallCounter %= batteryTaskUpdatesHz; // Wrap to X calls per 1 second

    if (productVariant == RTK_TORCH)
    {
        // Update the Battery LED according to the battery level

        // Solid LED when fuel level is above 50%
        if (batteryLevelPercent > 50)
        {
            ledcWrite(ledBatteryChannel, 255);
        }
        // Blink a short blink to indicate battery is depleting
        else
        {
            if (batteryCallCounter == (batteryTaskUpdatesHz / 10)) // On for 1/10th of a second
                ledcWrite(ledBatteryChannel, 255);
            else
                ledcWrite(ledBatteryChannel, 0);
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
            if (millis() >= beepNextEventMs)
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
            if (millis() >= beepNextEventMs)
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
    unsigned long doubleTapInterval = 500; // User must press and release twice within this to create a double tap
    if (present.imu_im19 && (present.display_type == DISPLAY_MAX_NONE))
        doubleTapInterval = 1000; // We are only interested in double taps, so use a longer interval
    unsigned long previousButtonRelease = 0;
    unsigned long thisButtonRelease = 0;
    bool singleTap = false;
    bool doubleTap = false;

    // Start notification
    task.buttonCheckTaskRunning = true;
    if (settings.printTaskStartStop)
        systemPrintln("Task buttonCheckTask started");

    // Run task until a request is raised
    task.buttonCheckTaskStopRequest = false;
    while (task.buttonCheckTaskStopRequest == false)
    {
        // Display an alive message
        if (PERIODIC_DISPLAY(PD_TASK_BUTTON_CHECK))
        {
            PERIODIC_CLEAR(PD_TASK_BUTTON_CHECK);
            systemPrintln("ButtonCheckTask running");
        }

        userBtn->read();

        if (userBtn->wasReleased()) // If a button release is detected, record it
        {
            previousButtonRelease = thisButtonRelease;
            thisButtonRelease = millis();
        }

        if ((previousButtonRelease > 0) && (thisButtonRelease > 0) &&
            ((thisButtonRelease - previousButtonRelease) <= doubleTapInterval)) // Do we have a double tap?
        {
            doubleTap = true;
            singleTap = false;
            previousButtonRelease = 0;
            thisButtonRelease = 0;
        }
        else if ((thisButtonRelease > 0) &&
                 ((millis() - thisButtonRelease) > doubleTapInterval)) // Do we have a single tap?
        {
            doubleTap = false;
            singleTap = true;
            previousButtonRelease = 0;
            thisButtonRelease = 0;
        }
        else // if ((previousButtonRelease == 0) && (thisButtonRelease > 0)) // Tap in progress?
        {
            doubleTap = false;
            singleTap = false;
        }

        if (present.imu_im19 && (present.display_type == DISPLAY_MAX_NONE))
        {
            // Platform has no display and tilt corrections, ie RTK Torch

            // The user button only exits tilt mode
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
                    changeState(STATE_WIFI_CONFIG_NOT_STARTED);
                }

                // If we are in WiFi Config Mode, exit to Rover
                else if (inWiFiConfigMode())
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
            else if (userBtn->pressedFor(2100))
            {
                tickerStop(); // Stop controlling LEDs via ticker task

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
            }
        } // End productVariant == Torch
        else // RTK EVK, RTK Facet v2, RTK Facet mosaic
        {
            if (systemState == STATE_SHUTDOWN)
            {
                // Ignore button presses while shutting down
            }
            else if (userBtn->pressedFor(shutDownButtonTime))
            {
                forceSystemStateUpdate = true;
                requestChangeState(STATE_SHUTDOWN);

                if (inMainMenu)
                    powerDown(true); // State machine is not updated while in menu system so go straight to power down
                                     // as needed
            }
            else if ((systemState == STATE_BASE_NOT_STARTED) && (firstRoverStart == true) && (userBtn->pressedFor(500)))
            {
                lastSetupMenuChange = millis(); // Prevent a timeout during state change
                forceSystemStateUpdate = true;
                requestChangeState(STATE_TEST);
            }
            else if (singleTap && (firstRoverStart == false) && (settings.disableSetupButton == false))
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
                case STATE_WIFI_CONFIG_NOT_STARTED:
                case STATE_WIFI_CONFIG:
                case STATE_ESPNOW_PAIRING_NOT_STARTED:
                case STATE_ESPNOW_PAIRING:
                case STATE_NTPSERVER_NOT_STARTED:
                case STATE_NTPSERVER_NO_SYNC:
                case STATE_NTPSERVER_SYNC:
                case STATE_CONFIG_VIA_ETH_NOT_STARTED:
                    lastSystemState = systemState; // Remember this state to return if needed
                    requestChangeState(STATE_DISPLAY_SETUP);
                    lastSetupMenuChange = millis();
                    setupSelectedButton = 0; // Highlight the first button
                    break;

                case STATE_DISPLAY_SETUP:
                    // If we are displaying the setup menu, a single tap will cycle through possible system states
                    // Exit into new system state on double tap - see below
                    // Exit display setup into previous state after ~10s - see updateSystemState()
                    lastSetupMenuChange = millis();
                    setupSelectedButton++;
                    if (setupSelectedButton == setupButtons.size()) // Limit reached?
                        setupSelectedButton = 0;
                    break;

                case STATE_TEST:
                    // Do nothing. User is releasing the setup button.
                    break;

                case STATE_TESTING:
                    // If we are in testing, return to Base Not Started
                    lastSetupMenuChange = millis(); // Prevent a timeout during state change
                    requestChangeState(STATE_BASE_NOT_STARTED);
                    break;

                case STATE_PROFILE:
                    // If the user presses the setup button during a profile change, do nothing
                    // Allow system to return to lastSystemState
                    break;

                case STATE_CONFIG_VIA_ETH_STARTED:
                case STATE_CONFIG_VIA_ETH:
                    // If the user presses the button during configure-via-ethernet,
                    // do a complete restart into Base mode
                    lastSetupMenuChange = millis(); // Prevent a timeout during state change
                    requestChangeState(STATE_CONFIG_VIA_ETH_RESTART_BASE);
                    break;

                    /* These lines are commented to allow default to print the diagnostic.
                    case STATE_KEYS_STARTED:
                    case STATE_KEYS_NEEDED:
                    case STATE_KEYS_WIFI_STARTED:
                    case STATE_KEYS_WIFI_CONNECTED:
                    case STATE_KEYS_WIFI_TIMEOUT:
                    case STATE_KEYS_EXPIRED:
                    case STATE_KEYS_DAYS_REMAINING:
                    case STATE_KEYS_LBAND_CONFIGURE:
                    case STATE_KEYS_LBAND_ENCRYPTED:
                    case STATE_KEYS_PROVISION_WIFI_STARTED:
                    case STATE_KEYS_PROVISION_WIFI_CONNECTED:
                        // Abort key download?
                        // TODO: check this! I think we want to be able to terminate STATE_KEYS via the button?
                        break;
                    */

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
                case STATE_DISPLAY_SETUP: {
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
                                requestChangeState(lastSystemState);
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

    // Reads data from ZED and stores data into circular buffer
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
        if (PERIODIC_DISPLAY(PD_TASK_SD_SIZE_CHECK))
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

        delay(1);
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
