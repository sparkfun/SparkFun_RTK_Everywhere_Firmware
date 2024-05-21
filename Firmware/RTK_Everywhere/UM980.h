#ifndef _RTK_EVERYWHERE_UM980_H
#define _RTK_EVERYWHERE_UM980_H

/*
  Unicore defaults:
  RTCM1006 10
  RTCM1074 1
  RTCM1084 1
  RTCM1094 1
  RTCM1124 1
  RTCM1033 10
*/

// Each constellation will have its config command text, enable, and a visible name
typedef struct
{
    char textName[30];
    char textCommand[5];
} um980ConstellationCommand;

// Constellations monitored/used for fix
// Available constellations: GPS, BDS, GLO, GAL, QZSS
// SBAS and IRNSS don't seem to be supported
const um980ConstellationCommand um980ConstellationCommands[] = {
    {"BeiDou", "BDS"},
    {"Galileo", "GAL"},
    {"GLONASS", "GLO"},
    {"GPS", "GPS"},
    {"QZSS", "QZSS"},
};

#define MAX_UM980_CONSTELLATIONS (sizeof(um980ConstellationCommands) / sizeof(um980ConstellationCommand))

// Struct to describe support messages on the UM980
// Each message will have the serial command and its default value
typedef struct
{
    const char msgTextName[9];
    const float msgDefaultRate;
} um980Msg;

// Static array containing all the compatible messages
const um980Msg umMessagesNMEA[] = {
    // NMEA
    {"GPDTM", 0}, {"GPGBS", 0},   {"GPGGA", 0.5}, {"GPGLL", 0}, {"GPGNS", 0},

    {"GPGRS", 0}, {"GPGSA", 0.5}, {"GPGST", 0.5}, {"GPGSV", 1}, {"GPRMC", 0.5},

    {"GPROT", 0}, {"GPTHS", 0},   {"GPVTG", 0},   {"GPZDA", 0},
};

const um980Msg umMessagesRTCM[] = {

    // RTCM
    {"RTCM1001", 0},  {"RTCM1002", 0}, {"RTCM1003", 0}, {"RTCM1004", 0}, {"RTCM1005", 1},
    {"RTCM1006", 0},  {"RTCM1007", 0}, {"RTCM1009", 0}, {"RTCM1010", 0},

    {"RTCM1011", 0},  {"RTCM1012", 0}, {"RTCM1013", 0}, {"RTCM1019", 0},

    {"RTCM1020", 0},

    {"RTCM1033", 10},

    {"RTCM1042", 0},  {"RTCM1044", 0}, {"RTCM1045", 0}, {"RTCM1046", 0},

    {"RTCM1071", 0},  {"RTCM1072", 0}, {"RTCM1073", 0}, {"RTCM1074", 1}, {"RTCM1075", 0},
    {"RTCM1076", 0},  {"RTCM1077", 0},

    {"RTCM1081", 0},  {"RTCM1082", 0}, {"RTCM1083", 0}, {"RTCM1084", 1}, {"RTCM1085", 0},
    {"RTCM1086", 0},  {"RTCM1087", 0},

    {"RTCM1091", 0},  {"RTCM1092", 0}, {"RTCM1093", 0}, {"RTCM1094", 1}, {"RTCM1095", 0},
    {"RTCM1096", 0},  {"RTCM1097", 0},

    {"RTCM1104", 0},

    {"RTCM1111", 0},  {"RTCM1112", 0}, {"RTCM1113", 0}, {"RTCM1114", 0}, {"RTCM1115", 0},
    {"RTCM1116", 0},  {"RTCM1117", 0},

    {"RTCM1121", 0},  {"RTCM1122", 0}, {"RTCM1123", 0}, {"RTCM1124", 1}, {"RTCM1125", 0},
    {"RTCM1126", 0},  {"RTCM1127", 0},
};

#define MAX_UM980_NMEA_MSG (sizeof(umMessagesNMEA) / sizeof(um980Msg))
#define MAX_UM980_RTCM_MSG (sizeof(umMessagesRTCM) / sizeof(um980Msg))

enum um980_Models
{
    UM980_DYN_MODEL_SURVEY = 0,
    UM980_DYN_MODEL_UAV,
    UM980_DYN_MODEL_AUTOMOTIVE,
};

#endif
