/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
GNSS_Mosaic.h

  Declarations and definitions for the Mosaic GNSS receiver
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#ifndef __GNSS_MOSAIC_H__
#define __GNSS_MOSAIC_H__

#include "GNSS_Mosaic.h"
#include <SparkFun_Extensible_Message_Parser.h> //http://librarymanager/All#SparkFun_Extensible_Message_Parser

typedef struct
{
    const uint16_t ID;
    const bool fixedLength;
    const uint16_t length; // Padded to modulo-4
    const char name[19];
} mosaicExpectedID;

const mosaicExpectedID mosaicExpectedIDs[] = {
    {4007, true, 96, "PVTGeodetic"},  {4013, false, 0, "ChannelStatus"}, {4014, false, 0, "ReceiverStatus"},
    {4059, false, 0, "DiskStatus"},   {4090, false, 0, "InputLink"},     {4097, false, 0, "EncapsulatedOutput"},
    {5914, true, 24, "ReceiverTime"},
};

#define MAX_MOSAIC_EXPECTED_SBF (sizeof(mosaicExpectedIDs) / sizeof(mosaicExpectedID))

// "A Stream is defined as a list of messages that should be output with the same interval on one
//  connection descriptor (Cd). In other words, one Stream is associated with one Cd and one Interval."
// Allocate this many streams for NMEA messages: Stream1, Stream2
// We actually need four times this many as COM1, COM2, USB1 and DSK1 all need their own individual streams
// COM1 uses streams 1 & 2; COM2 uses 3 & 4; USB1 uses 5 & 6; DSK1 uses 7 & 8
#define MOSAIC_NUM_NMEA_STREAMS 2 // X5 supports 10 streams in total
#define MOSAIC_DEFAULT_NMEA_STREAM_INTERVALS {MOSAIC_MSG_RATE_MSEC500, MOSAIC_MSG_RATE_SEC1}

// Output SBF PVTGeodetic and ReceiverTime on this stream - on COM1 only
// The SBFOutput streams are separate to the NMEAOutput streams. It is OK to start at Stream1.
#define MOSAIC_SBF_PVT_STREAM 1

// Output SBF ExtEvent event timing messages - to DSK1 only, OnChange only
// EventA is available on the Data port - via the multiplexer
// EventB is connected to ESP32 Pin 18 (on v1.1 PCB only)
#define MOSAIC_SBF_EXTEVENT_STREAM (MOSAIC_SBF_PVT_STREAM + 1)

// Output SBF InputLink messages on this stream - on COM1 only
// This indicates if RTCM corrections are being received - on COM2
#define MOSAIC_SBF_INPUTLINK_STREAM (MOSAIC_SBF_EXTEVENT_STREAM + 1)

// Output SBF ChannelStatus, ReceiverStatus and DiskStatus messages on this stream - on COM1 only
// ChannelStatus provides the count of satellites being tracked
// DiskStatus provides the disk usage
#define MOSAIC_SBF_STATUS_STREAM (MOSAIC_SBF_INPUTLINK_STREAM + 1)

// TODO: allow the user to define their own SBF stream for logging to DSK1 - through the menu / web config
// But, in the interim, the user can define their own SBF stream (>= Stream3) via the X5 web page over USB-C
// The updated configuration can be saved by entering and exiting the main menu, or by:
// Admin \ Expert Control \ Expert Console
// eccf,Current,Boot
// To restore the default configuration, "Reset all settings to default" (s r y), or by:
// Admin \ Expert Control \ Expert Console
// eccf,RxDefault,Boot
// eccf,RxDefault,Current

enum mosaicFileDuration_e
{
    MOSAIC_FILE_DURATION_HOUR1 = 0,
    MOSAIC_FILE_DURATION_HOUR6,
    MOSAIC_FILE_DURATION_HOUR24,
    MOSAIC_FILE_DURATION_MINUTE15,

    MOSAIC_NUM_FILE_DURATIONS // Must be last
};

typedef struct
{
    const char name[9];
    const char namingType[7];
    const char humanName[6];
    const uint16_t minutes;
} mosaicFileDuration;

const mosaicFileDuration mosaicFileDurations[] = {
    {"hour1", "IGS1H", "1h", 60},
    {"hour6", "IGS6H", "6h", 360},
    {"hour24", "IGS24H", "24h", 1440},
    {"minute15", "IGS15M", "15min", 15},
};

#define MAX_MOSAIC_FILE_DURATIONS (sizeof(mosaicFileDurations) / sizeof(mosaicFileDuration))

enum mosaicObsInterval_e
{
    MOSAIC_OBS_INTERVAL_SEC1 = 0,
    MOSAIC_OBS_INTERVAL_SEC2,
    MOSAIC_OBS_INTERVAL_SEC5,
    MOSAIC_OBS_INTERVAL_SEC10,
    MOSAIC_OBS_INTERVAL_SEC15,
    MOSAIC_OBS_INTERVAL_SEC30,
    MOSAIC_OBS_INTERVAL_SEC60,

    MOSAIC_NUM_OBS_INTERVALS // Must be last
};

typedef struct
{
    const char name[6];
    const char humanName[4];
    const uint8_t seconds;
} mosaicObsInterval;

const mosaicObsInterval mosaicObsIntervals[] = {
    {"sec1", "1s", 1},    {"sec2", "2s", 2},    {"sec5", "5s", 5},    {"sec10", "10s", 10},
    {"sec15", "15s", 15}, {"sec30", "30s", 30}, {"sec60", "60s", 60},
};

#define MAX_MOSAIC_OBS_INTERVALS (sizeof(mosaicObsIntervals) / sizeof(mosaicObsInterval))

enum mosaicCOMBaud
{
    MOSAIC_COM_RATE_BAUD4800 = 0,
    MOSAIC_COM_RATE_BAUD9600,
    MOSAIC_COM_RATE_BAUD19200,
    MOSAIC_COM_RATE_BAUD38400,
    MOSAIC_COM_RATE_BAUD57600,
    MOSAIC_COM_RATE_BAUD115200,
    MOSAIC_COM_RATE_BAUD230400,
    MOSAIC_COM_RATE_BAUD460800,
    MOSAIC_COM_RATE_BAUD921600,

    MOSAIC_NUM_COM_RATES // Must be last
};

// Each baud rate will have its config command text, enable, and a visible rate
typedef struct
{
    const char name[12];
    const uint32_t rate;
} mosaicComRate;

const mosaicComRate mosaicComRates[] = {
    {"baud4800", 4800},     {"baud9600", 9600},     {"baud19200", 19200},
    {"baud38400", 38400},   {"baud57600", 57600},   {"baud115200", 115200},
    {"baud230400", 230400}, {"baud460800", 460800}, {"baud921600", 921600},
};

#define MAX_MOSAIC_COM_RATES (sizeof(mosaicComRates) / sizeof(mosaicComRate))

enum mosaicPpsIntervals
{
    // OFF is dealt with by settings.enableExternalPulse
    MOSAIC_PPS_INTERVAL_MSEC10 = 0,
    MOSAIC_PPS_INTERVAL_MSEC20,
    MOSAIC_PPS_INTERVAL_MSEC50,
    MOSAIC_PPS_INTERVAL_MSEC100,
    MOSAIC_PPS_INTERVAL_MSEC200,
    MOSAIC_PPS_INTERVAL_MSEC250,
    MOSAIC_PPS_INTERVAL_MSEC500,
    MOSAIC_PPS_INTERVAL_SEC1,
    MOSAIC_PPS_INTERVAL_SEC2,
    MOSAIC_PPS_INTERVAL_SEC4,
    MOSAIC_PPS_INTERVAL_SEC5,
    MOSAIC_PPS_INTERVAL_SEC10,
    MOSAIC_PPS_INTERVAL_SEC30,
    MOSAIC_PPS_INTERVAL_SEC60,

    MOSAIC_NUM_PPS_INTERVALS // Must be last
};

// Each PPS interval will have its config command text, enable, and a visible rate
typedef struct
{
    const char name[8];
    const char humanName[6];
    const uint32_t interval_us;
} mosaicPPSInterval;

const mosaicPPSInterval mosaicPPSIntervals[] = {
    {"msec10", "10ms", 10000},    {"msec20", "20ms", 20000},    {"msec50", "50ms", 50000},
    {"msec100", "100ms", 100000}, {"msec200", "200ms", 200000}, {"msec250", "250ms", 250000},
    {"msec500", "500ms", 500000}, {"sec1", "1s", 1000000},      {"sec2", "2s", 2000000},
    {"sec4", "4s", 4000000},      {"sec5", "5s", 5000000},      {"sec10", "10s", 10000000},
    {"sec30", "30s", 30000000},   {"sec60", "60s", 60000000},
};

#define MAX_MOSAIC_PPS_INTERVALS (sizeof(mosaicPPSIntervals) / sizeof(mosaicPPSInterval))

enum mosaicConstellations
{
    MOSAIC_SIGNAL_CONSTELLATION_GPS = 0,
    MOSAIC_SIGNAL_CONSTELLATION_GLONASS,
    MOSAIC_SIGNAL_CONSTELLATION_GALILEO,
    MOSAIC_SIGNAL_CONSTELLATION_SBAS,
    MOSAIC_SIGNAL_CONSTELLATION_BEIDOU,
    MOSAIC_SIGNAL_CONSTELLATION_QZSS,
    MOSAIC_SIGNAL_CONSTELLATION_NAVIC,

    MOSAIC_NUM_SIGNAL_CONSTELLATIONS // Must be last
};

// Each constellation will have its config command text, enable, and a visible name
typedef struct
{
    const char name[8];
    const char configName[8];
} mosaicSignalConstellation;

// Constellations monitored/used for fix
const mosaicSignalConstellation mosaicSignalConstellations[] = {
    {"GPS", "GPS"},       {"GLONASS", "GLONASS"}, {"GALILEO", "Galileo"}, {"SBAS", "SBAS"},
    {"BEIDOU", "BeiDou"}, {"QZSS", "QZSS"},       {"NAVIC", "NavIC"},
};

#define MAX_MOSAIC_CONSTELLATIONS (sizeof(mosaicSignalConstellations) / sizeof(mosaicSignalConstellation))

// Enum to define message output rates
// Don't allow rate to be "off"
// If the user wants to disable a message, the stream (mosaicStreamIntervalsNMEA etc.) should be set to 0 instead
enum mosaicMessageRates
{
    // MOSAIC_MSG_RATE_OFF = 0,
    // MOSAIC_MSG_RATE_ONCHANGE,
    MOSAIC_MSG_RATE_MSEC10 = 0,
    MOSAIC_MSG_RATE_MSEC20,
    MOSAIC_MSG_RATE_MSEC40,
    MOSAIC_MSG_RATE_MSEC50,
    MOSAIC_MSG_RATE_MSEC100,
    MOSAIC_MSG_RATE_MSEC200,
    MOSAIC_MSG_RATE_MSEC500,
    MOSAIC_MSG_RATE_SEC1,
    MOSAIC_MSG_RATE_SEC2,
    MOSAIC_MSG_RATE_SEC5,
    MOSAIC_MSG_RATE_SEC10,
    MOSAIC_MSG_RATE_SEC15,
    MOSAIC_MSG_RATE_SEC30,
    MOSAIC_MSG_RATE_SEC60,
    MOSAIC_MSG_RATE_MIN2,
    MOSAIC_MSG_RATE_MIN5,
    MOSAIC_MSG_RATE_MIN10,
    MOSAIC_MSG_RATE_MIN15,
    MOSAIC_MSG_RATE_MIN30,
    MOSAIC_MSG_RATE_MIN60,

    MOSAIC_NUM_MSG_RATES // Must be last
};

// Struct to hold the message rate name and enum
typedef struct
{
    const char name[9];
    const char humanName[6];
} mosaicMsgRate;

// Static array containing all the mosaic message rates
const mosaicMsgRate mosaicMsgRates[] = {
    // { "off"},
    // { "OnChange"},
    {"msec10", "10ms"},   {"msec20", "20ms"},   {"msec40", "40ms"}, {"msec50", "50ms"}, {"msec100", "100ms"},
    {"msec200", "200ms"}, {"msec500", "500ms"}, {"sec1", "1s"},     {"sec2", "2s"},     {"sec5", "5s"},
    {"sec10", "10s"},     {"sec15", "15s"},     {"sec30", "30s"},   {"sec60", "60s"},   {"min2", "2min"},
    {"min5", "5min"},     {"min10", "10min"},   {"min15", "15min"}, {"min30", "30min"}, {"min60", "60min"},
};

// Check MAX_MOSAIC_MSG_RATES == MOSAIC_NUM_MSG_RATES
#define MAX_MOSAIC_MSG_RATES (sizeof(mosaicMsgRates) / sizeof(mosaicMsgRates[0]))

// Struct to describe support messages on the mosaic
// Each message will have the serial command and its default value
typedef struct
{
    const char msgTextName[8];
    const uint8_t msgDefaultStream;
} mosaicNMEAMsg;

// Static array containing all the compatible messages
// Make the default streams (intervals) the same as the UM980
// Stream 0 is off; stream 1 defaults to MSEC500; stream 2 defaults to SEC1
const mosaicNMEAMsg mosaicMessagesNMEA[] = {
    // NMEA
    {"ALM", 0}, {"AVR", 0}, {"DTM", 0}, {"GBS", 0},     {"GFA", 0}, {"GGA", 1}, {"GGK", 0}, {"GGQ", 0},
    {"GLL", 0}, {"GMP", 0}, {"GNS", 0}, {"GRS", 0},     {"GSA", 1}, {"GST", 1}, {"GSV", 2}, {"HDT", 0},
    {"HRP", 0}, {"LLK", 0}, {"LLQ", 0}, {"RBD", 0},     {"RBP", 0}, {"RBV", 0}, {"RMC", 1}, {"ROT", 0},
    {"SNC", 0}, {"TFM", 0}, {"THS", 0}, {"TXTbase", 0}, {"VTG", 0}, {"ZDA", 0},
};

#define MAX_MOSAIC_NMEA_MSG (sizeof(mosaicMessagesNMEA) / sizeof(mosaicNMEAMsg))

/* Do we need RTCMv2?

typedef struct
{
    const char msgName[10];
    const uint16_t defaultZCountOrInterval;
    const bool isZCount;
    const bool enabled;
} mosaicRTCMv2Msg;

typedef struct
{
    uint16_t zCountOrInterval;
    bool enabled;
} mosaicRTCMv2MsgRate;

const mosaicRTCMv2Msg mosaicMessagesRTCMv2[] = {

    {"RTCM1", 2, true, false},  {"RTCM3", 2, true, false}, {"RTCM9", 2, true, false},
    {"RTCM16", 2, true, false}, {"RTCM17", 2, true, false},
    {"RTCM18|19", 1, false, false}, {"RTCM20|21", 1, false, false},
    {"RTCM22", 2, true, false},  {"RTCM23|24", 2, true, false}, {"RTCM31", 2, true, false},
    {"RTCM32", 2, true, false},
};

#define MAX_MOSAIC_RTCM_V2_MSG (sizeof(mosaicMessagesRTCMv2) / sizeof(mosaicRTCMv2Msg))

*/

enum mosaicRTCMv3IntervalGroups
{
    MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1001_2 = 0,
    MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1003_4,
    MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1005_6,
    MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1007_8,
    MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1009_10,
    MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1011_12,
    MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1013,
    MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1019,
    MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1020,
    MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1029,
    MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1033,
    MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1042,
    MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1044,
    MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1045,
    MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1046,
    MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM1,
    MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM2,
    MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM3,
    MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM4,
    MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM5,
    MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM6,
    MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM7,
    MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1230,

    MOSAIC_NUM_RTCM_V3_INTERVAL_GROUPS // Must be last
};

typedef struct
{
    const char name[12];
    const float defaultInterval;
} mosaicRTCMv3MsgIntervalGroup;

const mosaicRTCMv3MsgIntervalGroup mosaicRTCMv3MsgIntervalGroups[] = {
    {"RTCM1001|2", 1.0},  {"RTCM1003|4", 1.0}, {"RTCM1005|6", 1.0}, {"RTCM1007|8", 1.0}, {"RTCM1009|10", 1.0},
    {"RTCM1011|12", 1.0}, {"RTCM1013", 1.0},   {"RTCM1019", 1.0},   {"RTCM1020", 1.0},   {"RTCM1029", 1.0},
    {"RTCM1033", 10.0},   {"RTCM1042", 1.0},   {"RTCM1044", 1.0},   {"RTCM1045", 1.0},   {"RTCM1046", 1.0},
    {"MSM1", 1.0},        {"MSM2", 1.0},       {"MSM3", 1.0},       {"MSM4", 1.0},       {"MSM5", 1.0},
    {"MSM6", 1.0},        {"MSM7", 1.0},       {"RTCM1230", 1.0},
};

#define MAX_MOSAIC_RTCM_V3_INTERVAL_GROUPS                                                                             \
    (sizeof(mosaicRTCMv3MsgIntervalGroups) / sizeof(mosaicRTCMv3MsgIntervalGroup))

typedef struct
{
    const char name[9];
    const uint8_t intervalGroup;
    const bool defaultEnabled;
} mosaicRTCMv3Msg;

const mosaicRTCMv3Msg mosaicMessagesRTCMv3[] = {

    {"RTCM1001", MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1001_2, false},
    {"RTCM1002", MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1001_2, false},
    {"RTCM1003", MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1003_4, false},
    {"RTCM1004", MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1003_4, false},
    {"RTCM1005", MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1005_6, true},
    {"RTCM1006", MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1005_6, false},
    {"RTCM1007", MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1007_8, false},
    {"RTCM1008", MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1007_8, false},
    {"RTCM1009", MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1009_10, false},
    {"RTCM1010", MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1009_10, false},
    {"RTCM1011", MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1011_12, false},
    {"RTCM1012", MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1011_12, false},
    {"RTCM1013", MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1013, false},
    {"RTCM1019", MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1019, false},
    {"RTCM1020", MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1020, false},
    {"RTCM1029", MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1029, false},
    {"RTCM1033", MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1033, true},
    {"RTCM1042", MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1042, false},
    {"RTCM1044", MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1044, false},
    {"RTCM1045", MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1045, false},
    {"RTCM1046", MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1046, false},
    {"MSM1", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM1, false},
    {"MSM2", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM2, false},
    {"MSM3", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM3, false},
    {"MSM4", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM4, true},
    {"MSM5", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM5, false},
    {"MSM6", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM6, false},
    {"MSM7", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM7, false},
    /*
        {"RTCM1071", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM1, false},
        {"RTCM1072", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM2, false},
        {"RTCM1073", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM3, false},
        {"RTCM1074", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM4, false},
        {"RTCM1075", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM5, false},
        {"RTCM1076", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM6, false},
        {"RTCM1077", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM7, false},
        {"RTCM1081", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM1, false},
        {"RTCM1082", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM2, false},
        {"RTCM1083", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM3, false},
        {"RTCM1084", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM4, false},
        {"RTCM1085", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM5, false},
        {"RTCM1086", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM6, false},
        {"RTCM1087", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM7, false},
        {"RTCM1091", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM1, false},
        {"RTCM1092", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM2, false},
        {"RTCM1093", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM3, false},
        {"RTCM1094", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM4, false},
        {"RTCM1095", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM5, false},
        {"RTCM1096", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM6, false},
        {"RTCM1097", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM7, false},
        {"RTCM1101", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM1, false},
        {"RTCM1102", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM2, false},
        {"RTCM1103", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM3, false},
        {"RTCM1104", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM4, false},
        {"RTCM1105", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM5, false},
        {"RTCM1106", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM6, false},
        {"RTCM1107", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM7, false},
        {"RTCM1111", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM1, false},
        {"RTCM1112", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM2, false},
        {"RTCM1113", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM3, false},
        {"RTCM1114", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM4, false},
        {"RTCM1115", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM5, false},
        {"RTCM1116", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM6, false},
        {"RTCM1117", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM7, false},
        {"RTCM1121", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM1, false},
        {"RTCM1122", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM2, false},
        {"RTCM1123", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM3, false},
        {"RTCM1124", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM4, false},
        {"RTCM1125", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM5, false},
        {"RTCM1126", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM6, false},
        {"RTCM1127", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM7, false},
        {"RTCM1131", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM1, false},
        {"RTCM1132", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM2, false},
        {"RTCM1133", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM3, false},
        {"RTCM1134", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM4, false},
        {"RTCM1135", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM5, false},
        {"RTCM1136", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM6, false},
        {"RTCM1137", MOSAIC_RTCM_V3_INTERVAL_GROUP_MSM7, false},
    */
    {"RTCM1230", MOSAIC_RTCM_V3_INTERVAL_GROUP_RTCM1230, false},
};

#define MAX_MOSAIC_RTCM_V3_MSG (sizeof(mosaicMessagesRTCMv3) / sizeof(mosaicRTCMv3Msg))

enum mosaic_Dynamics
{
    MOSAIC_DYN_MODEL_STATIC = 0,
    MOSAIC_DYN_MODEL_QUASISTATIC,
    MOSAIC_DYN_MODEL_PEDESTRIAN,
    MOSAIC_DYN_MODEL_AUTOMOTIVE,
    MOSAIC_DYN_MODEL_RACECAR,
    MOSAIC_DYN_MODEL_HEAVYMACHINERY,
    MOSAIC_DYN_MODEL_UAV,
    MOSAIC_DYN_MODEL_UNLIMITED,

    MOSAIC_NUM_DYN_MODELS, // Must be last
};

typedef struct
{
    const char name[15];
    const char humanName[16];
} mosaicReceiverDynamic;

const mosaicReceiverDynamic mosaicReceiverDynamics[] = {
    {"Static", "Static"},
    {"Quasistatic", "Quasistatic"},
    {"Pedestrian", "Pedestrian"},
    {"Automotive", "Automotive"},
    {"RaceCar", "Race Car"},
    {"HeavyMachinery", "Heavy Machinery"},
    {"UAV", "UAV"},
    {"Unlimited", "Unlimited"},
};

#define MAX_MOSAIC_RX_DYNAMICS (sizeof(mosaicReceiverDynamics) / sizeof(mosaicReceiverDynamic))

void mosaicX5flushRX(unsigned long timeout = 0); // Header
bool mosaicX5waitCR(unsigned long timeout = 25); // Header

class GNSS_MOSAIC : GNSS
{
    // The mosaic-X5 does not have self-contained interface library.
    // But the ZED-F9P, UM980 and LG290P all do.
    // On the X5, we communicate manually over serial2GNSS using functions like
    // sendWithResponse and sendAndWaitForIdle.
    // In essence, the interface library is wholly contained in this class.
    // TODO: consider breaking the mosaic comms functions out into their own library
    // and add a private library class instance here.

  protected:
    // Flag which indicates GNSS is blocking (needs exclusive access to the UART)
    bool _isBlocking = false;

    // These globals are updated regularly via the SBF parser
    double _clkBias_ms;             // PVTGeodetic RxClkBias (will be sawtooth unless clock steering is enabled)
    bool _determiningFixedPosition; // PVTGeodetic Mode Bit 6
    bool _antennaIsOpen;            // ReceiverStatus RxState Bit 1 ACTIVEANTENNA indicates antenna current draw
    bool _antennaIsShorted;         // ReceiverStatus RxError Bit 5 ANTENNA indicates antenna overcurrent

    // Record NrBytesReceived so we can tell if Radio Ext (COM2) is receiving correction data.
    // On the mosaic, we know that InputLink will arrive at 1Hz. But on the ZED, UBX-MON-COMMS
    // is tied to the navigation rate. To keep it simple, record the last time NrBytesReceived
    // was seen to increase and use that for corrections timeout. This is updated by the SBF
    // InputLink message. isCorrRadioExtPortActive returns true if the bytes-received has
    // increased in the previous settings.correctionsSourcesLifetime_s
    uint32_t _radioExtBytesReceived_millis;

    // See notes at GNSS_MOSAIC::setCorrRadioExtPort
    uint32_t previousNrBytesReceived = 0;
    bool firstTimeNrBytesReceived = true;

    // Setup the general configuration of the GNSS
    // Not Rover or Base specific (ie, baud rates)
    // Outputs:
    //   Returns true if successfully configured and false upon failure
    bool configure();

    // Set the minimum satellite signal level for navigation.
    bool setMinCN0(uint8_t cnoValue);

  public:
    // Allow access from parser routines
    float _latStdDev;
    float _lonStdDev;
    bool _receiverSetupSeen;
    bool _diskStatusSeen;
    struct svTracking_t
    {
        uint8_t SVID;
        unsigned long lastSeen;
    };
    std::vector<svTracking_t> svInTracking;
    // Find sv in the vector of svTracking_t
    // https://stackoverflow.com/a/590005
    struct find_sv
    {
        uint8_t findThisSv;
        find_sv(uint8_t sv) : findThisSv(sv)
        {
        }
        bool operator()(const svTracking_t &m) const
        {
            return m.SVID == findThisSv;
        }
    };
    // Check if SV is stale based on its lastSeen
    struct find_stale_sv
    {
        const unsigned long expireAfter_millis = 2000;
        unsigned long millisNow;
        find_stale_sv(unsigned long now) : millisNow(now)
        {
        }
        bool operator()(const svTracking_t &m) const
        {
            return (millisNow > (m.lastSeen + expireAfter_millis));
        }
    };

    // Constructor
    GNSS_MOSAIC()
        : _determiningFixedPosition(true), _clkBias_ms(0), _latStdDev(999.9), _lonStdDev(999.9),
          _receiverSetupSeen(false), _radioExtBytesReceived_millis(0), _diskStatusSeen(false), _antennaIsOpen(false),
          _antennaIsShorted(false), GNSS()
    {
        svInTracking.clear();
    }

    // If we have decryption keys, configure module
    // Note: don't check online.lband_neo here. We could be using ip corrections
    void applyPointPerfectKeys();

    // Set RTCM for base mode to defaults (1005/MSM4 1Hz & 1033 0.1Hz)
    void baseRtcmDefault();

    // Reset to Low Bandwidth Link (MSM4 0.5Hz & 1005/1033 0.1Hz)
    void baseRtcmLowDataRate();

    // Check if a given baud rate is supported by this module
    bool baudIsAllowed(uint32_t baudRate);
    uint32_t baudGetMinimum();
    uint32_t baudGetMaximum();

    // Connect to GNSS and identify particulars
    void begin();

    // Setup TM2 time stamp input as need
    // Outputs:
    //   Returns true when an external event occurs and false if no event
    bool beginExternalEvent();

    bool checkNMEARates();

    bool checkPPPRates();

    // Configure the Base
    // Outputs:
    //   Returns true if successfully configured and false upon failure
    bool configureBase();

    // Configure mosaic-X5 COM1 for Encapsulated RTCMv3 + SBF + NMEA, plus L-Band
    bool configureGNSSCOM(bool enableLBand);

    // Configure mosaic-X5 L-Band
    bool configureLBand(bool enableLBand, uint32_t LBandFreq = 0);

    // Configure specific aspects of the receiver for NTP mode
    bool configureNtpMode();

    // Perform the GNSS configuration
    // Outputs:
    //   Returns true if successfully configured and false upon failure
    bool configureOnce();

    // Configure the Rover
    // Outputs:
    //   Returns true if successfully configured and false upon failure
    bool configureRover();

    // Responds with the messages supported on this platform
    // Inputs:
    //   returnText: String to receive message names
    // Returns message names in the returnText string
    void createMessageList(String &returnText);

    // Responds with the RTCM/Base messages supported on this platform
    // Inputs:
    //   returnText: String to receive message names
    // Returns message names in the returnText string
    void createMessageListBase(String &returnText);

    void debuggingDisable();

    void debuggingEnable();

    // Restore the GNSS to the factory settings
    void factoryReset();

    uint16_t fileBufferAvailable();

    uint16_t fileBufferExtractData(uint8_t *fileBuffer, int fileBytesToRead);

    // Start the base using fixed coordinates
    // Outputs:
    //   Returns true if successfully started and false upon failure
    bool fixedBaseStart();

    bool fixRateIsAllowed(uint32_t fixRateMs);

    // Return min/max rate in ms
    uint32_t fixRateGetMinimumMs();

    uint32_t fixRateGetMaximumMs();

    // Return the number of active/enabled messages
    uint8_t getActiveMessageCount();

    // Return the number of active/enabled RTCM messages
    uint8_t getActiveRtcmMessageCount()
    {
        return (0);
    }

    // Get the altitude
    // Outputs:
    //   Returns the altitude in meters or zero if the GNSS is offline
    double getAltitude();

    // Returns the carrier solution or zero if not online
    uint8_t getCarrierSolution();

    // Get the COM port baud rate
    // Outputs:
    //   Returns 0 if the get fails
    uint32_t getCOMBaudRate(uint8_t port);

    // Mosaic COM3 is connected to the Data connector - via the multiplexer
    // Outputs:
    //   Returns 0 if the get fails
    uint32_t getDataBaudRate();

    // Returns the day number or zero if not online
    uint8_t getDay();

    // Return the number of milliseconds since GNSS data was last updated
    uint16_t getFixAgeMilliseconds();

    // Returns the fix type or zero if not online
    uint8_t getFixType();

    // Returns the hours of 24 hour clock or zero if not online
    uint8_t getHour();

    // Get the horizontal position accuracy
    // Outputs:
    //   Returns the horizontal position accuracy or zero if offline
    float getHorizontalAccuracy();

    const char *getId();

    // Get the latitude value
    // Outputs:
    //   Returns the latitude value or zero if not online
    double getLatitude();

    // Query GNSS for current leap seconds
    uint8_t getLeapSeconds();

    // Return the type of logging that matches the enabled messages - drives the logging icon
    uint8_t getLoggingType();

    // Get the longitude value
    // Outputs:
    //   Returns the longitude value or zero if not online
    double getLongitude();

    // Returns two digits of milliseconds or zero if not online
    uint8_t getMillisecond();

    // Returns minutes or zero if not online
    uint8_t getMinute();

    // Returns the current mode: Base/Rover/etc
    uint8_t getMode();

    // Returns month number or zero if not online
    uint8_t getMonth();

    // Returns nanoseconds or zero if not online
    uint32_t getNanosecond();

    // Given the name of a message, return the array number
    int getNMEAMessageNumberByName(const char *msgName);

    // Mosaic COM2 is connected to the Radio connector
    // Outputs:
    //   Returns 0 if the get fails
    uint32_t getRadioBaudRate();

    // Returns the seconds between solutions
    double getRateS();

    const char *getRtcmDefaultString();

    const char *getRtcmLowDataRateString();

    // Given the name of a message, return the array number
    int getRtcmMessageNumberByName(const char *msgName);

    // Returns the number of satellites in view or zero if offline
    uint8_t getSatellitesInView();

    // Returns seconds or zero if not online
    uint8_t getSecond();

    // Get the survey-in mean accuracy
    // Outputs:
    //   Returns the mean accuracy or zero (0)
    float getSurveyInMeanAccuracy();

    // Return the number of seconds the survey-in process has been running
    int getSurveyInObservationTime();

    // Returns timing accuracy or zero if not online
    uint32_t getTimeAccuracy();

    // Returns full year, ie 2023, not 23.
    uint16_t getYear();

    // Helper functions for the current mode as read from the GNSS receiver
    bool gnssInBaseFixedMode();
    bool gnssInBaseSurveyInMode();
    bool gnssInRoverMode();

    // Antenna Short / Open detection
    bool isAntennaShorted();
    bool isAntennaOpen();

    bool isBlocking();

    // Date is confirmed once we have GNSS fix
    bool isConfirmedDate();

    // Date is confirmed once we have GNSS fix
    bool isConfirmedTime();

    // Returns true if data is arriving on the Radio Ext port
    bool isCorrRadioExtPortActive();

    // Return true if GNSS receiver has a higher quality DGPS fix than 3D
    bool isDgpsFixed();

    // Some functions (L-Band area frequency determination) merely need
    // to know if we have a valid fix, not what type of fix
    // This function checks to see if the given platform has reached
    // sufficient fix type to be considered valid
    bool isFixed();

    // Used in tpISR() for time pulse synchronization
    bool isFullyResolved();

    bool isNMEAMessageEnabled(const char *msgName);

    bool isPppConverged();

    bool isPppConverging();

    // Send commands out the UART to see if a mosaic module is present
    bool isPresent();
    bool isPresentOnSerial(HardwareSerial *serialPort, const char *command, const char *response, const char *console,
                           int retryLimit = 20);
    bool mosaicIsPresentOnFlex();

    // Some functions (L-Band area frequency determination) merely need
    // to know if we have an RTK Fix.  This function checks to see if the
    // given platform has reached sufficient fix type to be considered valid
    bool isRTKFix();

    // Some functions (L-Band area frequency determination) merely need
    // to know if we have an RTK Float.  This function checks to see if
    // the given platform has reached sufficient fix type to be considered
    // valid
    bool isRTKFloat();

    // Determine if the survey-in operation is complete
    // Outputs:
    //   Returns true if the survey-in operation is complete and false
    //   if the operation is still running
    bool isSurveyInComplete();

    // Date will be valid if the RTC is reporting (regardless of GNSS fix)
    bool isValidDate();

    // Time will be valid if the RTC is reporting (regardless of GNSS fix)
    bool isValidTime();

    // Controls the constellations that are used to generate a fix and logged
    void menuConstellations();

    void menuMessageBaseRtcm();

    // Control the messages that get broadcast over Bluetooth and logged (if enabled)
    void menuMessages();

    void menuMessagesNMEA();

    void menuMessagesRTCM(bool rover);

    // Print the module type and firmware version
    void printModuleInfo();

    // Send correction data to the GNSS
    // Inputs:
    //   dataToSend: Address of a buffer containing the data
    //   dataLength: The number of valid data bytes in the buffer
    // Outputs:
    //   Returns the number of correction data bytes written
    int pushRawData(uint8_t *dataToSend, int dataLength);

    // Hardware or software reset the GNSS receiver
    bool reset();

    uint16_t rtcmBufferAvailable();

    // If LBand is being used, ignore any RTCM that may come in from the GNSS
    void rtcmOnGnssDisable();

    // If L-Band is available, but encrypted, allow RTCM through other sources (radio, ESP-NOW) to GNSS receiver
    void rtcmOnGnssEnable();

    uint16_t rtcmRead(uint8_t *rtcmBuffer, int rtcmBytesToRead);

    // Save the current configuration
    // Outputs:
    //   Returns true when the configuration was saved and false upon failure
    bool saveConfiguration();

    // Send message. Wait for up to timeout millis for reply to arrive
    // If the reply is received, keep reading bytes until the serial port has
    // been idle for idle millis
    // If response is defined, copy up to responseSize bytes
    // Inputs:
    //   message: Zero terminated string of characters containing the message
    //            to send to the GNSS
    //   reply: String containing the first portion of the expected response
    //   timeout: Number of milliseconds to wat for the reply to arrive
    //   idle: Number of milliseconds to wait after last reply character is received
    //   response: Address of buffer to receive the response
    //   responseSize: Maximum number of bytes to copy
    // Outputs:
    //   Returns true if the response was received and false upon failure
    bool sendAndWaitForIdle(const char *message, const char *reply, unsigned long timeout = 1000,
                            unsigned long idle = 25, char *response = nullptr, size_t responseSize = 0,
                            bool debug = true);
    bool sendAndWaitForIdle(HardwareSerial *serialPort, const char *message, const char *reply,
                            unsigned long timeout = 1000, unsigned long idle = 25, char *response = nullptr,
                            size_t responseSize = 0, bool debug = true);

    // Send message. Wait for up to timeout millis for reply to arrive
    // If the reply is received, keep reading bytes until the serial port has
    // been idle for idle millis
    // If response is defined, copy up to responseSize bytes
    // Inputs:
    //   message: String containing the message to send to the GNSS
    //   reply: String containing the first portion of the expected response
    //   timeout: Number of milliseconds to wat for the reply to arrive
    //   idle: Number of milliseconds to wait after last reply character is received
    //   response: Address of buffer to receive the response
    //   responseSize: Maximum number of bytes to copy
    // Outputs:
    //   Returns true if the response was received and false upon failure
    bool sendAndWaitForIdle(String message, const char *reply, unsigned long timeout = 1000, unsigned long idle = 25,
                            char *response = nullptr, size_t responseSize = 0, bool debug = true);

    // Send message. Wait for up to timeout millis for reply to arrive
    // If the reply has started to be received when timeout is reached, wait for a further wait millis
    // If the reply is seen, wait for a further wait millis
    // During wait, keep reading incoming serial. If response is defined, copy up to responseSize bytes
    // Inputs:
    //   message: Zero terminated string of characters containing the message
    //            to send to the GNSS
    //   reply: String containing the first portion of the expected response
    //   timeout: Number of milliseconds to wat for the reply to arrive
    //   wait: Number of additional milliseconds if the reply is detected
    //   response: Address of buffer to receive the response
    //   responseSize: Maximum number of bytes to copy
    // Outputs:
    //   Returns true if the response was received and false upon failure
    bool sendWithResponse(const char *message, const char *reply, unsigned long timeout = 1000, unsigned long wait = 25,
                          char *response = nullptr, size_t responseSize = 0);
    bool sendWithResponse(HardwareSerial *serialPort, const char *message, const char *reply,
                          unsigned long timeout = 1000, unsigned long wait = 25, char *response = nullptr,
                          size_t responseSize = 0);

    // Send message. Wait for up to timeout millis for reply to arrive
    // If the reply has started to be received when timeout is reached, wait for a further wait millis
    // If the reply is seen, wait for a further wait millis
    // During wait, keep reading incoming serial. If response is defined, copy up to responseSize bytes
    // Inputs:
    //   message: String containing the message to send to the GNSS
    //   reply: String containing the first portion of the expected response
    //   timeout: Number of milliseconds to wat for the reply to arrive
    //   wait: Number of additional milliseconds if the reply is detected
    //   response: Address of buffer to receive the response
    //   responseSize: Maximum number of bytes to copy
    // Outputs:
    //   Returns true if the response was received and false upon failure
    bool sendWithResponse(String message, const char *reply, unsigned long timeout = 1000, unsigned long wait = 25,
                          char *response = nullptr, size_t responseSize = 0);

    // Set the baud rate of mosaic-X5 COM1
    // This is used during the Bluetooth test
    // Inputs:
    //   port: COM port number
    //   baudRate: New baud rate for the COM port
    // Outputs:
    //   Returns true if the baud rate was set and false upon failure
    bool setBaudRate(uint8_t uartNumber, uint32_t baudRate); // From the super class
    bool setBaudRateCOM(uint8_t port, uint32_t baudRate);    // Original X5 implementation

    bool setBaudRateComm(uint32_t baudRate);

    bool setBaudRateData(uint32_t baudRate);

    bool setBaudRateRadio(uint32_t baudRate);

    // Enable all the valid constellations and bands for this platform
    bool setConstellations();

    // Enable / disable corrections protocol(s) on the Radio External port
    // Always update if force is true. Otherwise, only update if enable has changed state
    bool setCorrRadioExtPort(bool enable, bool force);

    // Set the elevation in degrees
    // Inputs:
    //   elevationDegrees: The elevation value in degrees
    bool setElevation(uint8_t elevationDegrees);

    // Enable or disable HAS E6 capability
    bool setHighAccuracyService(bool enableGalileoHas);

    // Configure any logging settings - currently mosaic-X5 specific
    bool setLogging();

    // Turn on all the enabled NMEA messages on COM1
    bool setMessagesNMEA();

    // Turn on all the enabled RTCM Base messages on COM1, COM2 and USB1 (if enabled)
    bool setMessagesRTCMBase();

    // Turn on all the enabled RTCM Rover messages on COM1, COM2 and USB1 (if enabled)
    bool setMessagesRTCMRover();

    // Set the dynamic model to use for RTK
    // Inputs:
    //   modelNumber: Number of the model to use, provided by radio library
    bool setModel(uint8_t modelNumber);

    bool setMultipathMitigation(bool enableMultipathMitigation);

    // Given the name of a message, find it, and set the rate
    bool setNmeaMessageRateByName(const char *msgName, uint8_t msgRate);

    // Setup the timepulse output on the PPS pin for external triggering
    // Outputs
    //   Returns true if the pin was successfully setup and false upon
    //   failure
    bool setPPS();

    // Specify the interval between solutions
    // Inputs:
    //   secondsBetweenSolutions: Number of seconds between solutions
    // Outputs:
    //   Returns true if the rate was successfully set and false upon
    //   failure
    bool setRate(double secondsBetweenSolutions);

    // Enable/disable any output needed for tilt compensation
    bool setTilt();

    bool standby();

    // Save the data from the SBF Block 4007
    void storeBlock4007(SEMP_PARSE_STATE *parse);

    // Save the data from the SBF Block 4013
    void storeBlock4013(SEMP_PARSE_STATE *parse);

    // Save the data from the SBF Block 4014
    void storeBlock4014(SEMP_PARSE_STATE *parse);

    // Save the data from the SBF Block 4059
    void storeBlock4059(SEMP_PARSE_STATE *parse);

    // Save the data from the SBF Block 4090
    void storeBlock4090(SEMP_PARSE_STATE *parse);

    // Save the data from the SBF Block 5914
    void storeBlock5914(SEMP_PARSE_STATE *parse);

    // Antenna Short / Open detection
    bool supportsAntennaShortOpen();

    // Reset the survey-in operation
    // Outputs:
    //   Returns true if the survey-in operation was reset successfully
    //   and false upon failure
    bool surveyInReset();

    // Start the survey-in operation
    // Outputs:
    //   Return true if successful and false upon failure
    bool surveyInStart();

    // Poll routine to update the GNSS state
    void update();

    void updateSD();

    void waitSBFReceiverSetup(HardwareSerial *serialPort, unsigned long timeout);
};

// Forward routine declarations
bool mosaicCreateString(RTK_Settings_Types type,
                        int settingsIndex,
                        char * newSettings);
bool mosaicIsPresentOnFlex();
void mosaicNewClass();
bool mosaicNewSettingValue(RTK_Settings_Types type,
                           const char * suffix,
                           int qualifier,
                           double d);
bool mosaicSettingsToFile(File *settingsFile,
                          RTK_Settings_Types type,
                          int settingsIndex);

#endif  // __GNSS_MOSAIC_H__
