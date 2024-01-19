#ifdef PARSE_UNICORE_MESSAGES // Remove this line

/*------------------------------------------------------------------------------
Parse_Unicore.ino

  Unicore message parsing support routines
------------------------------------------------------------------------------*/

#include <SparkFun_Unicore_GNSS_Arduino_Library.h> // Get constants and crc32Table

//
//    Unicore Binary Response
//
//    |<----- 24 byte header ------>|<--- length --->|<- 4 bytes ->|
//    |                             |                |             |
//    +------------+----------------+----------------+-------------+
//    |  Preamble  | See table 7-48 |      Data      |    CRC      |
//    |  3 bytes   |   21 bytes     |    n bytes     |   32 bits   |
//    | 0xAA 44 B5 |                |                |             |
//    +------------+----------------+----------------+-------------+
//    |                                              |
//    |<------------------------ CRC --------------->|
//

// Check for the preamble
uint8_t unicorePreamble(PARSE_STATE *parse, uint8_t data)
{
    if (data == 0xAA)
    {
        parse->state = unicoreBinarySync2;
        return SENTENCE_TYPE_UNICORE;
    }
    return SENTENCE_TYPE_NONE;
}

// Read the second sync byte
uint8_t unicoreBinarySync2(PARSE_STATE *parse, uint8_t data)
{
    // Verify sync byte 2
    if (data != 0x44)
    {
        // Invalid sync byte, place this byte at the beginning of the buffer
        parse->length = 0;
        parse->buffer[parse->length++] = data;
        return (gpsMessageParserFirstByte(parse, data)); // Start searching for a preamble byte
    }

    parse->state = unicoreBinarySync3; // Move on
    return SENTENCE_TYPE_UNICORE;
}

// Read the third sync byte
uint8_t unicoreBinarySync3(PARSE_STATE *parse, uint8_t data)
{
    // Verify sync byte 3
    if (data != 0xB5)
    {
        // Invalid sync byte, place this byte at the beginning of the buffer
        parse->length = 0;
        parse->buffer[parse->length++] = data;
        return (gpsMessageParserFirstByte(parse, data)); // Start searching for a preamble byte
    }

    parse->state = unicoreBinaryReadLength; // Move on
    return SENTENCE_TYPE_UNICORE;
}

// Read the message length
uint8_t unicoreBinaryReadLength(PARSE_STATE *parse, uint8_t data)
{
    if (parse->length < offsetHeaderMessageLength + 2)
        return SENTENCE_TYPE_UNICORE;

    // Get the length
    uint16_t expectedLength =
        (parse->buffer[offsetHeaderMessageLength + 1] << 8) | parse->buffer[offsetHeaderMessageLength];

    // The overall message length is header (24) + data (expectedLength) + CRC (4)
    parse->bytesRemaining = um980HeaderLength + expectedLength + 4;

    if (parse->bytesRemaining > PARSE_BUFFER_LENGTH)
    {
        systemPrintln("Length overflow");

        // Invalid length, place this byte at the beginning of the buffer
        parse->length = 0;
        parse->buffer[parse->length++] = data;

        // Start searching for a preamble byte
        return gpsMessageParserFirstByte(parse, data);
    }

    // Account for the bytes already received
    parse->bytesRemaining -= parse->length;
    parse->state = unicoreReadData; // Move on
    return SENTENCE_TYPE_UNICORE;
}

// Calculate and return the CRC of the given buffer
uint32_t calculateCRC32(uint8_t *charBuffer, uint16_t bufferSize)
{
    uint32_t crc = 0;
    for (uint16_t x = 0; x < bufferSize; x++)
        crc = crc32Table[(crc ^ charBuffer[x]) & 0xFF] ^ (crc >> 8);
    return crc;
}

// Read the message content until we reach the end, then check CRC
uint8_t unicoreReadData(PARSE_STATE *parse, uint8_t data)
{
    // Account for this data byte
    parse->bytesRemaining -= 1;

    // Wait until all the data is received, including the 4-byte CRC
    if (parse->bytesRemaining > 0)
        return SENTENCE_TYPE_UNICORE;

    // We have all the data including the CRC

    // Check the CRC
    uint32_t sentenceCRC = ((uint32_t)parse->buffer[parse->length - 4] << (8 * 0)) |
                           ((uint32_t)parse->buffer[parse->length - 3] << (8 * 1)) |
                           ((uint32_t)parse->buffer[parse->length - 2] << (8 * 2)) |
                           ((uint32_t)parse->buffer[parse->length - 1] << (8 * 3));
    uint32_t calculatedCRC =
        calculateCRC32(parse->buffer, parse->length - 4); // CRC is calculated on entire message, sans CRC

    // Process this message if CRC is valid
    if (sentenceCRC == calculatedCRC)
    {
        // Message complete, CRC is valid
        parse->eomCallback(parse, SENTENCE_TYPE_UNICORE);
    }
    else
    {
        if (settings.enableGNSSdebug)
        {
            systemPrintln();
            systemPrintf("Unicore CRC failed. Sentence CRC: 0x%02X Calculated CRC: 0x%02X\r\n", sentenceCRC,
                         calculatedCRC);
        }
    }

    // Search for another preamble byte
    parse->length = 0;
    parse->state = gpsMessageParserFirstByte; // Move on
    return SENTENCE_TYPE_NONE;
}

#endif // PARSE_UNICORE_MESSAGES, remove this line
