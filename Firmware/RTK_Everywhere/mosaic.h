#ifndef _RTK_EVERYWHERE_MOSAIC_H
#define _RTK_EVERYWHERE_MOSAIC_H

typedef struct {
    const uint16_t ID;
    const bool fixedLength;
    const uint16_t length; // Padded to modulo-4
    const char name[19];
} mosaicExpectedID;

const mosaicExpectedID mosaicExpectedIDs[] = {
    { 4007, true, 96, "PVTGeodetic" },
    { 4097, false, 0, "EncapsulatedOutput" },
    { 5914, true, 24, "ReceiverTime" },
};

#define MAX_MOSAIC_EXPECTED_SBF (sizeof(mosaicExpectedIDs) / sizeof(mosaicExpectedID))

// "A Stream is defined as a list of messages that should be output with the same interval on one
//  connection descriptor (Cd). In other words, one Stream is associated with one Cd and one Interval."
// Allocate this many streams for NMEA messages: Stream1, Stream2
// We actually need four times this many as COM1, COM2, USB1 and DSK1 all need their own individual streams
// COM1 uses streams 1 & 2; COM2 uses 3 & 4; USB1 uses 5 & 6; DSK1 uses 7 & 8
#define MOSAIC_NUM_NMEA_STREAMS 2 // X5 supports 10 streams in total
#define MOSAIC_DEFAULT_NMEA_STREAM_INTERVALS {MOSAIC_MSG_RATE_MSEC500,MOSAIC_MSG_RATE_SEC1}

// Output SBF PVTGeodetic and ReceiverTime on this stream - on COM1 only
#define MOSAIC_SBF_PVT_STREAM ((4 * MOSAIC_NUM_NMEA_STREAMS) + 1)

// Output ExtEvent event timing messages - to DSK1 only
// EventA is available on the Data port - via the multiplexer
// EventB is connected to ESP32 Pin 18 (on v1.1 PCB only)
#define MOSAIC_SBF_EXTEVENT_STREAM (MOSAIC_SBF_PVT_STREAM + 1)

enum mosaicFileDuration_e {
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
    const uint16_t minutes;
} mosaicFileDuration;

const mosaicFileDuration mosaicFileDurations[] = {
    { "hour1", "IGS1H", 60 },
    { "hour6", "IGS6H", 360 },
    { "hour24", "IGS24H", 1440 },
    { "minute15", "IGS15M", 15 },
};

#define MAX_MOSAIC_FILE_DURATIONS (sizeof(mosaicFileDurations) / sizeof(mosaicFileDuration))

enum mosaicObsInterval_e {
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
    const uint8_t seconds;
} mosaicObsInterval;

const mosaicObsInterval mosaicObsIntervals[] = {
    { "sec1", 1 },
    { "sec2", 2 },
    { "sec5", 5 },
    { "sec10", 10 },
    { "sec15", 15 },
    { "sec30", 30 },
    { "sec60", 60 },
};

#define MAX_MOSAIC_OBS_INTERVALS (sizeof(mosaicObsIntervals) / sizeof(mosaicObsInterval))

enum mosaicCOMBaud {
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
    { "baud4800", 4800 },
    { "baud9600", 9600 },
    { "baud19200", 19200 },
    { "baud38400", 38400 },
    { "baud57600", 57600 },
    { "baud115200", 115200 },
    { "baud230400", 230400 },
    { "baud460800", 460800 },
    { "baud921600", 921600 },
};

#define MAX_MOSAIC_COM_RATES (sizeof(mosaicComRates) / sizeof(mosaicComRate))

enum mosaicPpsIntervals {
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
    const uint32_t interval_us;
} mosaicPPSInterval;

const mosaicPPSInterval mosaicPPSIntervals[] = {
    { "msec10", 10000 },
    { "msec20", 20000 },
    { "msec50", 50000 },
    { "msec100", 100000 },
    { "msec200", 200000 },
    { "msec250", 250000 },
    { "msec500", 500000 },
    { "sec1", 1000000 },
    { "sec2", 2000000 },
    { "sec4", 4000000 },
    { "sec5", 5000000 },
    { "sec10", 10000000 },
    { "sec30", 30000000 },
    { "sec60", 60000000 },
};

#define MAX_MOSAIC_PPS_INTERVALS (sizeof(mosaicPPSIntervals) / sizeof(mosaicPPSInterval))

enum mosaicConstellations {
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
    {"GPS","GPS"},
    {"GLONASS","GLONASS"},
    {"GALILEO","Galileo"},
    {"SBAS","SBAS"},
    {"BEIDOU","BeiDou"},
    {"QZSS","QZSS"},
    {"NAVIC","NAVIC"},
};

#define MAX_MOSAIC_CONSTELLATIONS (sizeof(mosaicSignalConstellations) / sizeof(mosaicSignalConstellation))

// Enum to define message output rates
// Don't allow rate to be "off"
// If the user wants to disable a message, the stream (mosaicStreamIntervalsNMEA etc.) should be set to 0 instead
enum mosaicMessageRates {
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
} mosaicMsgRate;

// Static array containing all the mosaic message rates
const mosaicMsgRate mosaicMsgRates[] = {
    // { "off"},
    // { "OnChange"},
    { "msec10"},
    { "msec20"},
    { "msec40"},
    { "msec50"},
    { "msec100"},
    { "msec200"},
    { "msec500"},
    { "sec1"},
    { "sec2"},
    { "sec5"},
    { "sec10"},
    { "sec15"},
    { "sec30"},
    { "sec60"},
    { "min2"},
    { "min5"},
    { "min10"},
    { "min15"},
    { "min30"},
    { "min60"},
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
    {"ALM", 0}, {"AVR", 0}, {"DTM", 0},
    {"GBS", 0}, {"GFA", 0}, {"GGA", 1},
    {"GGK", 0}, {"GGQ", 0}, {"GLL", 0},
    {"GMP", 0}, {"GNS", 0}, {"GRS", 0},
    {"GSA", 1}, {"GST", 1}, {"GSV", 2},
    {"HDT", 0}, {"HRP", 0}, {"LLK", 0},
    {"LLQ", 0}, {"RBD", 0}, {"RBP", 0},
    {"RBV", 0}, {"RMC", 1}, {"ROT", 0},
    {"SNC", 0}, {"TFM", 0}, {"THS", 0},
    {"TXTbase", 0}, {"VTG", 0}, {"ZDA", 0},
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

enum mosaicRTCMv3IntervalGroups {
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
    { "RTCM1001|2", 1.0 },
    { "RTCM1003|4", 1.0 },
    { "RTCM1005|6", 1.0 },
    { "RTCM1007|8", 1.0 },
    { "RTCM1009|10", 1.0 },
    { "RTCM1011|12", 1.0 },
    { "RTCM1013", 1.0 },
    { "RTCM1019", 1.0 },
    { "RTCM1020", 1.0 },
    { "RTCM1029", 1.0 },
    { "RTCM1033", 10.0 },
    { "RTCM1042", 1.0 },
    { "RTCM1044", 1.0 },
    { "RTCM1045", 1.0 },
    { "RTCM1046", 1.0 },
    { "MSM1", 1.0 },
    { "MSM2", 1.0 },
    { "MSM3", 1.0 },
    { "MSM4", 1.0 },
    { "MSM5", 1.0 },
    { "MSM6", 1.0 },
    { "MSM7", 1.0 },
    { "RTCM1230", 1.0 },
};

#define MAX_MOSAIC_RTCM_V3_INTERVAL_GROUPS (sizeof(mosaicRTCMv3MsgIntervalGroups) / sizeof(mosaicRTCMv3MsgIntervalGroup))

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
} mosaicReceiverDynamic;

const mosaicReceiverDynamic mosaicReceiverDynamics[] = {
    {"Static"},
    {"Quasistatic"},
    {"Pedestrian"},
    {"Automotive"},
    {"RaceCar"},
    {"HeavyMachinery"},
    {"UAV"},
    {"Unlimited"},
};

#define MAX_MOSAIC_RX_DYNAMICS (sizeof(mosaicReceiverDynamics) / sizeof(mosaicReceiverDynamic))

void mosaicX5flushRX(unsigned long timeout = 0); // Header
bool mosaicX5waitCR(unsigned long timeout = 25); // Header
bool mosaicX5sendWithResponse(const char *message, const char *reply, unsigned long timeout = 1000, unsigned long wait = 25, char *response = nullptr, size_t responseSize = 0); // Header
bool mosaicX5sendWithResponse(String message, const char *reply, unsigned long timeout = 1000, unsigned long wait = 25, char *response = nullptr, size_t responseSize = 0); // Header
bool mosaicX5sendAndWaitForIdle(const char *message, const char *reply, unsigned long timeout = 1000, unsigned long idle = 25, char *response = nullptr, size_t responseSize = 0);
bool mosaicX5sendAndWaitForIdle(String message, const char *reply, unsigned long timeout = 1000, unsigned long idle = 25, char *response = nullptr, size_t responseSize = 0);

#endif
