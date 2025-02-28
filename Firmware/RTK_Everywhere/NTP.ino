
/*------------------------------------------------------------------------------
NTP.ino

  This module implements the network time protocol (NTP).

  NTP Testing using Ethernet:

    Raspberry Pi Setup:
        * Install Raspberry Pi OS
        * Edit /etc/systemd/timesyncd.conf
        * Remove '#" from in front of NTP= line
        * Set NTP= line to:
            NTP="your NTP server address" "addresses from FallbackNTPK= line"
        * without the double quotes
        * Force a time update using:
            sudo systemctl restart systemd-timesyncd.service

    NTP Testing on Raspberry Pi:
        * Log into the Raspberry Pi system
        * Start the terminal program
        * Display the time server using:
            timedatectl timesync-status
        * Verify that the Server specifies your NTP server IP address
        * Force a time update using:
            sudo systemctl restart systemd-timesyncd.service

  Test Setup:

          RTK Reference Station          Raspberry Pi
                    ^                     NTP Server
                    | Ethernet cable           ^
                    v                          |
             Ethernet Switch <-----------------'
                    ^
                    | Ethernet cable
                    v
            Internet Firewall
                    ^
                    | Ethernet cable
                    v
                  Modem
                    ^
                    |
                    v
                Internet
                    ^
                    |
                    v
               NTP Server

------------------------------------------------------------------------------*/

#ifdef COMPILE_ETHERNET

//----------------------------------------
// Constants
//----------------------------------------

enum NTP_STATE
{
    NTP_STATE_OFF,
    NTP_STATE_NETWORK_CONNECTED,
    NTP_STATE_SERVER_RUNNING,
    // Insert new states here
    NTP_STATE_MAX
};

const char *const ntpServerStateName[] = {"NTP_STATE_OFF", "NTP_STATE_NETWORK_CONNECTED", "NTP_STATE_SERVER_RUNNING"};
const int ntpServerStateNameEntries = sizeof(ntpServerStateName) / sizeof(ntpServerStateName[0]);

const RtkMode_t ntpServerMode = RTK_MODE_NTP;

//----------------------------------------
// Locals
//----------------------------------------

static NetworkUDP *ntpServer; // This will be instantiated when we know the NTP port
static uint8_t ntpServerState;
static uint32_t lastLoggedNTPRequest;

//----------------------------------------
// Menu to get the NTP settings
//----------------------------------------

void menuNTP()
{
    if (!present.ethernet_ws5500 == true)
    {
        clearBuffer(); // Empty buffer of any newline chars
        return;
    }

    while (1)
    {
        systemPrintln();
        systemPrintln("Menu: NTP");
        systemPrintln();

        systemPrint("1) Poll Exponent: 2^");
        systemPrintln(settings.ntpPollExponent);

        systemPrint("2) Precision: 2^");
        systemPrintln(settings.ntpPrecision);

        systemPrint("3) Root Delay (us): ");
        systemPrintln(settings.ntpRootDelay);

        systemPrint("4) Root Dispersion (us): ");
        systemPrintln(settings.ntpRootDispersion);

        systemPrint("5) Reference ID: ");
        systemPrintln(settings.ntpReferenceId);

        systemPrint("6) NTP Port: ");
        systemPrintln(settings.ethernetNtpPort);

        systemPrintln("x) Exit");

        byte incoming = getUserInputCharacterNumber();

        if (incoming == 1)
        {
            systemPrint("Enter new poll exponent (2^, Min 3, Max 17): ");
            long newVal = getUserInputNumber();
            if ((newVal >= 3) && (newVal <= 17))
                settings.ntpPollExponent = newVal;
            else
                systemPrintln("Error: poll exponent out of range");
        }
        else if (incoming == 2)
        {
            systemPrint("Enter new precision (2^, Min -30, Max 0): ");
            long newVal = getUserInputNumber();
            if ((newVal >= -30) && (newVal <= 0))
                settings.ntpPrecision = newVal;
            else
                systemPrintln("Error: precision out of range");
        }
        else if (incoming == 3)
        {
            systemPrint("Enter new root delay (us): ");
            long newVal = getUserInputNumber();
            if ((newVal >= 0) && (newVal <= 1000000))
                settings.ntpRootDelay = newVal;
            else
                systemPrintln("Error: root delay out of range");
        }
        else if (incoming == 4)
        {
            systemPrint("Enter new root dispersion (us): ");
            long newVal = getUserInputNumber();
            if ((newVal >= 0) && (newVal <= 1000000))
                settings.ntpRootDispersion = newVal;
            else
                systemPrintln("Error: root dispersion out of range");
        }
        else if (incoming == 5)
        {
            systemPrint("Enter new Reference ID (4 Chars Max): ");
            char newId[5];
            if (getUserInputString(newId, 5) == INPUT_RESPONSE_VALID)
            {
                int i = 0;
                for (; i < strlen(newId); i++)
                    settings.ntpReferenceId[i] = newId[i];
                for (; i < 5; i++)
                    settings.ntpReferenceId[i] = 0;
            }
            else
                systemPrintln("Error: invalid Reference ID");
        }
        else if (incoming == 6)
        {
            systemPrint("Enter new NTP port: ");
            long newVal = getUserInputNumber();
            if ((newVal >= 0) && (newVal <= 65535))
                settings.ethernetNtpPort = newVal;
            else
                systemPrintln("Error: port number out of range");
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

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// NTP Packet storage and utilities

struct NTPpacket
{
    static const uint8_t NTPpacketSize = 48;

    uint8_t packet[NTPpacketSize]; // Copy of the NTP packet
    void setPacket(uint8_t *ptr)
    {
        memcpy(packet, ptr, NTPpacketSize);
    }
    void getPacket(uint8_t *ptr)
    {
        memcpy(ptr, packet, NTPpacketSize);
    }

    const uint32_t NTPtoUnixOffset = 2208988800; // NTP starts at Jan 1st 1900. Unix starts at Jan 1st 1970.

    uint8_t LiVnMode; // Leap Indicator, Version Number, Mode

    // Leap Indicator is 2 bits :
    // 00 : No warning
    // 01 : Last minute of the day has 61s
    // 10 : Last minute of the day has 59 s
    // 11 : Alarm condition (clock not synchronized)
    const uint8_t defaultLeapInd = 0;
    uint8_t LI()
    {
        return LiVnMode >> 6;
    }
    void LI(uint8_t val)
    {
        LiVnMode = (LiVnMode & 0x3F) | ((val & 0x03) << 6);
    }

    // Version Number is 3 bits. NTP version is currently four (4)
    const uint8_t defaultVersion = 4;
    uint8_t VN()
    {
        return (LiVnMode >> 3) & 0x07;
    }
    void VN(uint8_t val)
    {
        LiVnMode = (LiVnMode & 0xC7) | ((val & 0x07) << 3);
    }

    // Mode is 3 bits:
    // 0 : Reserved
    // 1 : Symmetric active
    // 2 : Symmetric passive
    // 3 : Client
    // 4 : Server
    // 5 : Broadcast
    // 6 : NTP control message
    // 7 : Reserved for private use
    const uint8_t defaultMode = 4;
    uint8_t mode()
    {
        return (LiVnMode & 0x07);
    }
    void mode(uint8_t val)
    {
        LiVnMode = (LiVnMode & 0xF8) | (val & 0x07);
    }

    // Stratum is 8 bits:
    // 0 : Unspecified
    // 1 : Reference clock (e.g., radio clock)
    // 2-15 : Secondary server (via NTP)
    // 16-255 : Unreachable
    //
    // We'll use 1 = Reference Clock
    const uint8_t defaultStratum = 1;
    uint8_t stratum;

    // Poll exponent
    // This is an eight-bit unsigned integer indicating the maximum interval between successive messages,
    // in seconds to the nearest power of two.
    // In the reference implementation, the values can range from 3 (8 s) through 17 (36 h).
    //
    // RFC 5905 suggests 6-10. We'll use 6. 2^6 = 64 seconds
    const uint8_t defaultPollExponent = 6;
    uint8_t pollExponent;

    // Precision
    // This is an eight-bit signed integer indicating the precision of the system clock,
    // in seconds to the nearest power of two. For instance, a value of -18 corresponds to a precision of about 4us.
    //
    // tAcc is usually around 1us. So we'll use -20 (0xEC). 2^-20 = 0.95us
    const int8_t defaultPrecision = -20; // 0xEC
    int8_t precision;

    // Root delay
    // This is a 32-bit, unsigned, fixed-point number indicating the total round-trip delay to the reference clock,
    // in seconds with fraction point between bits 15 and 16. In contrast to the calculated peer round-trip delay,
    // which can take both positive and negative values, this value is always positive.
    //
    // We are the reference clock, so we'll use zero (0x00000000).
    const uint32_t defaultRootDelay = 0x00000000;
    uint32_t rootDelay;

    // Root dispersion
    // This is a 32-bit, unsigned, fixed-point number indicating the maximum error relative to the reference clock,
    // in seconds with fraction point between bits 15 and 16.
    //
    // Tricky... Could depend on interrupt service time? Maybe go with ~1ms?
    const uint32_t defaultRootDispersion = 0x00000042; // 1007us
    uint32_t rootDispersion;

    // Reference identifier
    // This is a 32-bit code identifying the particular reference clock. The interpretation depends on the value in
    // the stratum field. For stratum 0 (unsynchronized), this is a four-character ASCII (American Standard Code for
    // Information Interchange) string called the kiss code, which is used for debugging and monitoring purposes.
    // GPS : Global Positioning System
    const uint8_t referenceIdLen = 4;
    const char defaultReferenceId[4] = {'G', 'P', 'S', 0};
    char referenceId[4];

    // Reference timestamp
    // This is the local time at which the system clock was last set or corrected, in 64-bit NTP timestamp format.
    uint32_t referenceTimestampSeconds;
    uint32_t referenceTimestampFraction;

    // Originate timestamp
    // This is the local time at which the request departed the client for the server, in 64-bit NTP timestamp format.
    uint32_t originateTimestampSeconds;
    uint32_t originateTimestampFraction;

    // Receive timestamp
    // This is the local time at which the request arrived at the server, in 64-bit NTP timestamp format.
    uint32_t receiveTimestampSeconds;
    uint32_t receiveTimestampFraction;

    // Transmit timestamp
    // This is the local time at which the reply departed the server for the client, in 64-bit NTP timestamp format.
    uint32_t transmitTimestampSeconds;
    uint32_t transmitTimestampFraction;

    typedef union {
        int8_t signed8;
        uint8_t unsigned8;
    } unsignedSigned8;

    uint32_t extractUnsigned32(uint8_t *ptr)
    {
        uint32_t val = 0;
        val |= *ptr++ << 24; // NTP data is Big-Endian
        val |= *ptr++ << 16;
        val |= *ptr++ << 8;
        val |= *ptr++;
        return val;
    }

    void insertUnsigned32(uint8_t *ptr, uint32_t val)
    {
        *ptr++ = val >> 24; // NTP data is Big-Endian
        *ptr++ = (val >> 16) & 0xFF;
        *ptr++ = (val >> 8) & 0xFF;
        *ptr++ = val & 0xFF;
    }

    // Extract the data from an NTP packet into the correct fields
    void extract()
    {
        uint8_t *ptr = packet;

        LiVnMode = *ptr++;
        stratum = *ptr++;
        pollExponent = *ptr++;

        unsignedSigned8 converter8;
        converter8.unsigned8 = *ptr++; // Convert to int8_t without ambiguity
        precision = converter8.signed8;

        rootDelay = extractUnsigned32(ptr);
        ptr += 4;
        rootDispersion = extractUnsigned32(ptr);
        ptr += 4;

        for (uint8_t i = 0; i < referenceIdLen; i++)
            referenceId[i] = *ptr++;

        referenceTimestampSeconds = extractUnsigned32(ptr);
        ptr += 4;
        referenceTimestampFraction =
            extractUnsigned32(ptr); // Note: the fraction is in increments of (1 / 2^32) secs, not microseconds
        ptr += 4;
        originateTimestampSeconds = extractUnsigned32(ptr);
        ptr += 4;
        originateTimestampFraction = extractUnsigned32(ptr);
        ptr += 4;
        receiveTimestampSeconds = extractUnsigned32(ptr);
        ptr += 4;
        receiveTimestampFraction = extractUnsigned32(ptr);
        ptr += 4;
        transmitTimestampSeconds = extractUnsigned32(ptr);
        ptr += 4;
        transmitTimestampFraction = extractUnsigned32(ptr);
        ptr += 4;
    }

    // Insert the data from the fields into an NTP packet
    void insert()
    {
        uint8_t *ptr = packet;

        *ptr++ = LiVnMode;
        *ptr++ = stratum;
        *ptr++ = pollExponent;

        unsignedSigned8 converter8;
        converter8.signed8 = precision;
        *ptr++ = converter8.unsigned8; // Convert to uint8_t without ambiguity

        insertUnsigned32(ptr, rootDelay);
        ptr += 4;
        insertUnsigned32(ptr, rootDispersion);
        ptr += 4;

        for (uint8_t i = 0; i < 4; i++)
            *ptr++ = referenceId[i];

        insertUnsigned32(ptr, referenceTimestampSeconds);
        ptr += 4;
        insertUnsigned32(
            ptr,
            referenceTimestampFraction); // Note: the fraction is in increments of (1 / 2^32) secs, not microseconds
        ptr += 4;
        insertUnsigned32(ptr, originateTimestampSeconds);
        ptr += 4;
        insertUnsigned32(ptr, originateTimestampFraction);
        ptr += 4;
        insertUnsigned32(ptr, receiveTimestampSeconds);
        ptr += 4;
        insertUnsigned32(ptr, receiveTimestampFraction);
        ptr += 4;
        insertUnsigned32(ptr, transmitTimestampSeconds);
        ptr += 4;
        insertUnsigned32(ptr, transmitTimestampFraction);
        ptr += 4;
    }

    uint32_t convertMicrosToSecsAndFraction(uint32_t val) // 16-bit fraction used by root delay and dispersion
    {
        double secs = val;
        secs /= 1000000.0;  // Convert micros to seconds
        secs = floor(secs); // Convert to integer, round down

        double microsecs = val;
        microsecs -= secs * 1000000.0; // Subtract the seconds
        microsecs /= 1000000.0;        // Convert micros to seconds
        microsecs *= pow(2.0, 16.0);   // Convert to 16-bit fraction

        uint32_t result = ((uint32_t)secs) << 16;
        result |= ((uint32_t)microsecs) & 0xFFFF;
        return (result);
    }

    uint32_t convertMicrosToFraction(uint32_t val) // 32-bit fraction used by the timestamps
    {
        val %= 1000000;      // Just in case
        double v = val;      // Convert micros to double
        v /= 1000000.0;      // Convert micros to seconds
        v *= pow(2.0, 32.0); // Convert to fraction
        return (uint32_t)v;
    }

    uint32_t convertFractionToMicros(uint32_t val) // 32-bit fraction used by the timestamps
    {
        double v = val;      // Convert fraction to double
        v /= pow(2.0, 32.0); // Convert fraction to seconds
        v *= 1000000.0;      // Convert to micros
        uint32_t ret = (uint32_t)v;
        ret %= 1000000; // Just in case
        return ret;
    }

    uint32_t convertNTPsecondsToUnix(uint32_t val)
    {
        return (val - NTPtoUnixOffset);
    }

    uint32_t convertUnixSecondsToNTP(uint32_t val)
    {
        return (val + NTPtoUnixOffset);
    }
};

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// NTP process one request
// recTv contains the timeval the NTP packet was received
// syncTv contains the timeval when the RTC was last sync'd
// ntpDiag will contain useful diagnostics
bool ntpProcessOneRequest(bool process, const timeval *recTv, const timeval *syncTv, char *ntpDiag = nullptr,
                          size_t ntpDiagSize = 0); // Header
bool ntpProcessOneRequest(bool process, const timeval *recTv, const timeval *syncTv, char *ntpDiag, size_t ntpDiagSize)
{
    bool processed = false;

    if (ntpDiag != nullptr)
        *ntpDiag = 0; // Clear any existing diagnostics

    ntpServer->parsePacket();

    int packetDataSize = ntpServer->available();

    if (packetDataSize > 0)
    {
        gettimeofday((timeval *)&ethernetNtpTv,
                     nullptr); // Record the time of the NTP request. This was in ethernetISR()

        IPAddress remoteIP = ntpServer->remoteIP();
        uint16_t remotePort = ntpServer->remotePort();

        if (ntpDiag != nullptr) // Add the packet size and remote IP/Port to the diagnostics
        {
            snprintf(ntpDiag, ntpDiagSize, "NTP request from:  Remote IP: %s  Remote Port: %d\r\n", remoteIP.toString(),
                     remotePort);
        }

        if (packetDataSize >= NTPpacket::NTPpacketSize)
        {
            // Read the NTP packet
            NTPpacket packet;

            ntpServer->read((char *)&packet.packet, NTPpacket::NTPpacketSize); // Copy the NTP data into our packet

            /*
            // Add the incoming packet to the diagnostics
            if (ntpDiag != nullptr)
            {
                char tmpbuf[128];
                snprintf(tmpbuf, sizeof(tmpbuf), "Packet <-- : ");
                strlcat(ntpDiag, tmpbuf, ntpDiagSize);
                for (int i = 0; i < NTPpacket::NTPpacketSize; i++)
                {
                    snprintf(tmpbuf, sizeof(tmpbuf), "%02X ", packet.packet[i]);
                    strlcat(ntpDiag, tmpbuf, ntpDiagSize);
                }
                snprintf(tmpbuf, sizeof(tmpbuf), "\r\n");
                strlcat(ntpDiag, tmpbuf, ntpDiagSize);
            }
            */

            // If process is false, return now
            if (!process)
            {
                char tmpbuf[128];
                snprintf(tmpbuf, sizeof(tmpbuf),
                         "NTP request ignored. Time has not been synchronized - or not in NTP mode.\r\n");
                strlcat(ntpDiag, tmpbuf, ntpDiagSize);
                return false;
            }

            packet.extract(); // Extract the raw data into fields

            packet.LI(packet.defaultLeapInd);       // Clear the leap second adjustment. TODO: set this correctly using
                                                    // getLeapSecondEvent from the GNSS
            packet.VN(packet.defaultVersion);       // Set the version number
            packet.mode(packet.defaultMode);        // Set the mode
            packet.stratum = packet.defaultStratum; // Set the stratum
            packet.pollExponent = settings.ntpPollExponent;                                  // Set the poll interval
            packet.precision = settings.ntpPrecision;                                        // Set the precision
            packet.rootDelay = packet.convertMicrosToSecsAndFraction(settings.ntpRootDelay); // Set the Root Delay
            packet.rootDispersion =
                packet.convertMicrosToSecsAndFraction(settings.ntpRootDispersion); // Set the Root Dispersion
            for (uint8_t i = 0; i < packet.referenceIdLen; i++)
                packet.referenceId[i] = settings.ntpReferenceId[i]; // Set the reference Id

            // REF: http://support.ntp.org/bin/view/Support/DraftRfc2030
            // '.. the client sets the Transmit Timestamp field in the request
            // to the time of day according to the client clock in NTP timestamp format.'
            // '.. The server copies this field to the originate timestamp in the reply and
            // sets the Receive Timestamp and Transmit Timestamp fields to the time of day
            // according to the server clock in NTP timestamp format.'

            // Important note: the NTP Era started January 1st 1900.
            // tv will contain the time based on the Unix epoch (January 1st 1970)
            // We need to adjust...

            // First, add the client transmit timestamp to our diagnostics
            if (ntpDiag != nullptr)
            {
                char tmpbuf[128];
                snprintf(tmpbuf, sizeof(tmpbuf), "Originate Timestamp (Client Transmit): %u.%06u\r\n",
                         packet.transmitTimestampSeconds,
                         packet.convertFractionToMicros(packet.transmitTimestampFraction));
                strlcat(ntpDiag, tmpbuf, ntpDiagSize);
            }

            // Copy the client transmit timestamp into the originate timestamp
            packet.originateTimestampSeconds = packet.transmitTimestampSeconds;
            packet.originateTimestampFraction = packet.transmitTimestampFraction;

            // Set the receive timestamp to the time we received the packet (logged by the W5500 interrupt)
            uint32_t recUnixSeconds = recTv->tv_sec;
            recUnixSeconds -= settings.timeZoneSeconds; // Subtract the time zone offset to convert recTv to Unix time
            recUnixSeconds -= settings.timeZoneMinutes * 60;
            recUnixSeconds -= settings.timeZoneHours * 60 * 60;
            packet.receiveTimestampSeconds = packet.convertUnixSecondsToNTP(recUnixSeconds);  // Unix -> NTP
            packet.receiveTimestampFraction = packet.convertMicrosToFraction(recTv->tv_usec); // Micros to 1/2^32

            // Add the receive timestamp to the diagnostics
            if (ntpDiag != nullptr)
            {
                char tmpbuf[128];
                snprintf(tmpbuf, sizeof(tmpbuf), "Received Timestamp:                    %u.%06u\r\n",
                         packet.receiveTimestampSeconds,
                         packet.convertFractionToMicros(packet.receiveTimestampFraction));
                strlcat(ntpDiag, tmpbuf, ntpDiagSize);
            }

            // Add when our clock was last sync'd
            uint32_t syncUnixSeconds = syncTv->tv_sec;
            syncUnixSeconds -= settings.timeZoneSeconds; // Subtract the time zone offset to convert recTv to Unix time
            syncUnixSeconds -= settings.timeZoneMinutes * 60;
            syncUnixSeconds -= settings.timeZoneHours * 60 * 60;
            packet.referenceTimestampSeconds = packet.convertUnixSecondsToNTP(syncUnixSeconds);  // Unix -> NTP
            packet.referenceTimestampFraction = packet.convertMicrosToFraction(syncTv->tv_usec); // Micros to 1/2^32

            // Add that to the diagnostics
            if (ntpDiag != nullptr)
            {
                char tmpbuf[128];
                snprintf(tmpbuf, sizeof(tmpbuf), "Reference Timestamp (Last Sync):       %u.%06u\r\n",
                         packet.referenceTimestampSeconds,
                         packet.convertFractionToMicros(packet.referenceTimestampFraction));
                strlcat(ntpDiag, tmpbuf, ntpDiagSize);
            }

            // Add the transmit time - i.e. now!
            timeval txTime;
            gettimeofday(&txTime, nullptr);
            uint32_t nowUnixSeconds = txTime.tv_sec;
            nowUnixSeconds -= settings.timeZoneSeconds; // Subtract the time zone offset to convert recTv to Unix time
            nowUnixSeconds -= settings.timeZoneMinutes * 60;
            nowUnixSeconds -= settings.timeZoneHours * 60 * 60;
            packet.transmitTimestampSeconds = packet.convertUnixSecondsToNTP(nowUnixSeconds);  // Unix -> NTP
            packet.transmitTimestampFraction = packet.convertMicrosToFraction(txTime.tv_usec); // Micros to 1/2^32

            packet.insert(); // Copy the data fields back into the buffer

            // Now transmit the response to the client.
            ntpServer->beginPacket(remoteIP, remotePort);
            ntpServer->write(packet.packet, NTPpacket::NTPpacketSize);
            int result = ntpServer->endPacket();
            processed = true;

            // Add our server transmit time to the diagnostics
            if (ntpDiag != nullptr)
            {
                char tmpbuf[128];
                snprintf(tmpbuf, sizeof(tmpbuf), "Transmit Timestamp:                    %u.%06u\r\n",
                         packet.transmitTimestampSeconds,
                         packet.convertFractionToMicros(packet.transmitTimestampFraction));
                strlcat(ntpDiag, tmpbuf, ntpDiagSize);
            }

            /*
            // Add the socketSendUDP result to the diagnostics
            if (ntpDiag != nullptr)
            {
                char tmpbuf[128];
                snprintf(tmpbuf, sizeof(tmpbuf), "socketSendUDP result: %d\r\n", result);
                strlcat(ntpDiag, tmpbuf, ntpDiagSize);
            }
            */

            /*
            // Add the outgoing packet to the diagnostics
            if (ntpDiag != nullptr)
            {
                char tmpbuf[128];
                snprintf(tmpbuf, sizeof(tmpbuf), "Packet --> : ");
                strlcat(ntpDiag, tmpbuf, ntpDiagSize);
                for (int i = 0; i < NTPpacket::NTPpacketSize; i++)
                {
                    snprintf(tmpbuf, sizeof(tmpbuf), "%02X ", packet.packet[i]);
                    strlcat(ntpDiag, tmpbuf, ntpDiagSize);
                }
                snprintf(tmpbuf, sizeof(tmpbuf), "\r\n");
                strlcat(ntpDiag, tmpbuf, ntpDiagSize);
            }
            */
        }
        else
        {
            char tmpbuf[64];
            snprintf(tmpbuf, sizeof(tmpbuf), "Invalid size: %d\r\n", packetDataSize);
            strlcat(ntpDiag, tmpbuf, ntpDiagSize);
        }
    }

    ntpServer->clear();

    return processed;
}

// Configure specific aspects of the receiver for NTP mode
bool configureUbloxModuleNTP()
{
    if (present.timePulseInterrupt == false)
        return (false);

    if (online.gnss == false)
        return (false);

    // If our settings haven't changed, and this is first config since power on, trust GNSS's settings
    // Unless this is an EVK - where the GNSS has no battery-backed RAM
    if ((productVariant != RTK_EVK) && (settings.updateGNSSSettings == false) && (firstPowerOn == true))
    {
        firstPowerOn = false; // Next time user switches modes, new settings will be applied
        log_d("Skipping ZED NTP configuration");
        return (true);
    }
    firstPowerOn = false; // If we switch between rover/base in the future, force config of module.

    gnss->update(); // Regularly poll to get latest data
    return gnss->configureNtpMode();
}

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
// NTP Server routines
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

// Update the state of the NTP server state machine
void ntpServerSetState(uint8_t newState)
{
    if ((settings.debugNtp || PERIODIC_DISPLAY(PD_NTP_SERVER_STATE)) && (!inMainMenu))
    {
        if (ntpServerState == newState)
            systemPrint("NTP Server: *");
        else
            systemPrintf("NTP Server: %s --> ", ntpServerStateName[ntpServerState]);
    }
    ntpServerState = newState;
    if (settings.debugNtp || PERIODIC_DISPLAY(PD_NTP_SERVER_STATE))
    {
        PERIODIC_CLEAR(PD_NTP_SERVER_STATE);
        if (newState >= NTP_STATE_MAX)
        {
            systemPrintf("Unknown state: %d\r\n", newState);
            reportFatalError("Unknown NTP Server state");
        }
        else if (!inMainMenu)
            systemPrintln(ntpServerStateName[ntpServerState]);
    }
}

// Stop the NTP server
void ntpServerStop()
{
    // Mark the NTP server as off
    online.ethernetNTPServer = false;

    // Release the NTP server memory
    if (ntpServer)
    {
        ntpServer->stop();
        delete ntpServer;
        ntpServer = nullptr;
        if (!inMainMenu)
            reportHeapNow(settings.debugNtp);
    }

    // Stop the NTP server
    networkConsumerOffline(NETCONSUMER_NTP_SERVER);
    ntpServerSetState(NTP_STATE_OFF);
}

// Update the NTP server state
void ntpServerUpdate()
{
    bool connected;
    char ntpDiag[768]; // Char array to hold diagnostic messages

    if (present.ethernet_ws5500 == false)
        return;

    // Shutdown the NTP server when the mode changes or network fails
    connected = networkConsumerIsConnected(NETCONSUMER_NTP_SERVER);
    if (NEQ_RTK_MODE(ntpServerMode) || !connected)
    {
        if (ntpServerState > NTP_STATE_OFF)
            ntpServerStop();
        return;
    }

    // Process the NTP state
    DMW_st(ntpServerSetState, ntpServerState);
    switch (ntpServerState)
    {
    default:
        break;

    case NTP_STATE_OFF:
        // Determine if the NTP server is enabled
        if (EQ_RTK_MODE(ntpServerMode))
        {
            // The NTP server only works over Ethernet
            if (networkInterfaceHasInternet(NETWORK_ETHERNET))
                ntpServerSetState(NTP_STATE_NETWORK_CONNECTED);
        }
        break;

    case NTP_STATE_NETWORK_CONNECTED:
        // Attempt to start the NTP server
        ntpServer = new NetworkUDP;
        if (!ntpServer)
            // Insufficient memory to start the NTP server
            ntpServerStop();
        else
        {
            ntpServer->begin(settings.ethernetNtpPort); // Start the NTP server
            online.ethernetNTPServer = true;
            if (!inMainMenu)
                reportHeapNow(settings.debugNtp);
            ntpServerSetState(NTP_STATE_SERVER_RUNNING);
        }
        break;

    case NTP_STATE_SERVER_RUNNING:
        // Check for new NTP requests - if the time has been sync'd
        bool processed = ntpProcessOneRequest(systemState == STATE_NTPSERVER_SYNC, (const timeval *)&ethernetNtpTv,
                                              (const timeval *)&gnssSyncTv, ntpDiag, sizeof(ntpDiag));

        // Print the diagnostics - if enabled
        if ((settings.debugNtp || PERIODIC_DISPLAY(PD_NTP_SERVER_DATA)) && (strlen(ntpDiag) > 0) && (!inMainMenu))
        {
            PERIODIC_CLEAR(PD_NTP_SERVER_DATA);
            systemPrint(ntpDiag);
        }

        if (processed)
        {
            // Log the NTP request to file - if enabled
            if (settings.enableNTPFile)
            {
                // Gain access to the SPI controller for the microSD card
                if (xSemaphoreTake(sdCardSemaphore, fatSemaphore_longWait_ms) == pdPASS)
                {
                    markSemaphore(FUNCTION_NTPEVENT);

                    // Get the marks file name
                    char fileName[55];
                    bool fileOpen = false;
                    bool sdCardWasOnline;
                    int year;
                    int month;
                    int day;

                    // Get the date
                    year = rtc.getYear();
                    month = rtc.getMonth() + 1;
                    day = rtc.getDay();

                    // Build the file name
                    snprintf(fileName, sizeof(fileName), "/NTP_Requests_%04d_%02d_%02d.txt", year, month, day);

                    // Try to gain access the SD card
                    sdCardWasOnline = online.microSD;
                    if (online.microSD != true)
                        beginSD();

                    if (online.microSD == true)
                    {
                        // Check if the NTP file already exists
                        bool ntpFileExists = false;
                        ntpFileExists = sd->exists(fileName);

                        // Open the NTP file
                        SdFile ntpFile;

                        if (ntpFileExists)
                        {
                            if (ntpFile && ntpFile.open(fileName, O_APPEND | O_WRITE))
                            {
                                fileOpen = true;
                                sdUpdateFileCreateTimestamp(&ntpFile);
                            }
                        }
                        else
                        {
                            if (ntpFile && ntpFile.open(fileName, O_CREAT | O_WRITE))
                            {
                                fileOpen = true;
                                sdUpdateFileAccessTimestamp(&ntpFile);

                                // If you want to add a file header, do it here
                            }
                        }

                        if (fileOpen)
                        {
                            // Write the NTP request to the file
                            ntpFile.write((const uint8_t *)ntpDiag, strlen(ntpDiag));

                            // Update the file to create time & date
                            sdUpdateFileCreateTimestamp(&ntpFile);

                            // Close the mark file
                            ntpFile.close();
                        }

                        // Dismount the SD card
                        if (!sdCardWasOnline)
                            endSD(true, false);
                    }

                    // Done with the SPI controller
                    xSemaphoreGive(sdCardSemaphore);

                    lastLoggedNTPRequest = millis();
                    ntpLogIncreasing = true;
                } // End sdCardSemaphore
            }
        }

        if (millis() > (lastLoggedNTPRequest + 5000))
            ntpLogIncreasing = false;
        break;
    }

    // Periodically display the NTP server state
    if (PERIODIC_DISPLAY(PD_NTP_SERVER_STATE))
        ntpServerSetState(ntpServerState);
}

// Verify the NTP tables
void ntpValidateTables()
{
    if (ntpServerStateNameEntries != NTP_STATE_MAX)
        reportFatalError("Fix ntpServerStateNameEntries to match NTP_STATE");
}

#endif // COMPILE_ETHERNET
