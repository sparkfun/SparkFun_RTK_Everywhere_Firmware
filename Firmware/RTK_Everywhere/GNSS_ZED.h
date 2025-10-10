/*------------------------------------------------------------------------------
GNSS_ZED.h

  Declarations and definitions for the GNSS_ZED implementation
------------------------------------------------------------------------------*/

#ifndef __GNSS_ZED_H__
#define __GNSS_ZED_H__

#ifdef COMPILE_ZED

#include <SparkFun_u-blox_GNSS_v3.h> //http://librarymanager/All#SparkFun_u-blox_GNSS_v3

// Each constellation will have its config key, enable, and a visible name
typedef struct
{
    uint32_t configKey;
    uint8_t gnssID;
    bool enabled;
    char textName[30];
} ubxConstellation;

// These are the allowable constellations to receive from and log (if enabled)
// Tested with u-center v21.02
#define MAX_UBX_CONSTELLATIONS 6 // Should be (sizeof(settings.ubxConstellations)/sizeof(ubxConstellation)). Tricky...

#define UBX_ID_NOT_AVAILABLE 0xFF

// These are the allowable messages to broadcast and log (if enabled)

// Struct to describe the necessary info for each type of UBX message
// Each message will have a key, ID, class, visible name, and various info about which platforms the message is
// supported on Message rates are store within NVM
typedef struct
{
    const uint32_t msgConfigKey;
    const uint8_t msgID;
    const uint8_t msgClass;
    const uint8_t msgDefaultRate;
    const char msgTextName[20];
    const uint32_t filterMask;
    const uint16_t f9pFirmwareVersionSupported; // The minimum version this message is supported. 0 = all versions. 9999
                                                // = Not supported
    const uint16_t f9rFirmwareVersionSupported;
} ubxMsg;

// Static array containing all the compatible messages
const ubxMsg ubxMessages[] = {
    // AID

    // ESF
    {UBLOX_CFG_MSGOUT_UBX_ESF_ALG_UART1, UBX_ESF_ALG, UBX_CLASS_ESF, 0, "ESF_ALG", 0, 9999,
     120}, // Not supported on F9P
    {UBLOX_CFG_MSGOUT_UBX_ESF_INS_UART1, UBX_ESF_INS, UBX_CLASS_ESF, 0, "ESF_INS", 0, 9999, 120},
    {UBLOX_CFG_MSGOUT_UBX_ESF_MEAS_UART1, UBX_ESF_MEAS, UBX_CLASS_ESF, 0, "ESF_MEAS", 0, 9999, 120},
    {UBLOX_CFG_MSGOUT_UBX_ESF_RAW_UART1, UBX_ESF_RAW, UBX_CLASS_ESF, 0, "ESF_RAW", 0, 9999, 120},
    {UBLOX_CFG_MSGOUT_UBX_ESF_STATUS_UART1, UBX_ESF_STATUS, UBX_CLASS_ESF, 0, "ESF_STATUS", 0, 9999, 120},

    // HNR

    // LOG
    // F9P supports LOG_INFO at 112

    // MON
    {UBLOX_CFG_MSGOUT_UBX_MON_COMMS_UART1, UBX_MON_COMMS, UBX_CLASS_MON, 0, "MON_COMMS", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_MON_HW_UART1, UBX_MON_HW, UBX_CLASS_MON, 0, "MON_HW", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_MON_HW2_UART1, UBX_MON_HW2, UBX_CLASS_MON, 0, "MON_HW2", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_MON_HW3_UART1, UBX_MON_HW3, UBX_CLASS_MON, 0, "MON_HW3", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_MON_IO_UART1, UBX_MON_IO, UBX_CLASS_MON, 0, "MON_IO", 0, 112, 120},

    {UBLOX_CFG_MSGOUT_UBX_MON_MSGPP_UART1, UBX_MON_MSGPP, UBX_CLASS_MON, 0, "MON_MSGPP", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_MON_RF_UART1, UBX_MON_RF, UBX_CLASS_MON, 0, "MON_RF", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_MON_RXBUF_UART1, UBX_MON_RXBUF, UBX_CLASS_MON, 0, "MON_RXBUF", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_MON_RXR_UART1, UBX_MON_RXR, UBX_CLASS_MON, 0, "MON_RXR", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_MON_SPAN_UART1, UBX_MON_SPAN, UBX_CLASS_MON, 0, "MON_SPAN", 0, 113,
     120}, // Not supported F9P 112

    {UBLOX_CFG_MSGOUT_UBX_MON_SYS_UART1, UBX_MON_SYS, UBX_CLASS_MON, 0, "MON_SYS", 0, 9999,
     130}, // Not supported F9R 121, F9P 112, 113, 120, 130, 132
    {UBLOX_CFG_MSGOUT_UBX_MON_TXBUF_UART1, UBX_MON_TXBUF, UBX_CLASS_MON, 0, "MON_TXBUF", 0, 112, 120},

    // NAV2
    // F9P not supported 112, 113, 120. Supported starting 130. F9P 130, 132 supports all but not EELL, PVAT, TIMENAVIC
    // F9R not supported 120. Supported starting 130. F9R 130 supports EELL, PVAT but not SVIN, TIMENAVIC.
    {UBLOX_CFG_MSGOUT_UBX_NAV2_CLOCK_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_CLOCK", 0, 130, 130},
    {UBLOX_CFG_MSGOUT_UBX_NAV2_COV_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_COV", 0, 130, 130},
    {UBLOX_CFG_MSGOUT_UBX_NAV2_DOP_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_DOP", 0, 130, 130},
    {UBLOX_CFG_MSGOUT_UBX_NAV2_EELL_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_EELL", 0, 9999,
     130}, // Not supported F9P
    {UBLOX_CFG_MSGOUT_UBX_NAV2_EOE_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_EOE", 0, 130, 130},

    {UBLOX_CFG_MSGOUT_UBX_NAV2_ODO_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_ODO", 0, 130, 130},
    {UBLOX_CFG_MSGOUT_UBX_NAV2_POSECEF_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_POSECEF", 0, 130, 130},
    {UBLOX_CFG_MSGOUT_UBX_NAV2_POSLLH_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_POSLLH", 0, 130, 130},
    {UBLOX_CFG_MSGOUT_UBX_NAV2_PVAT_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_PVAT", 0, 9999,
     130}, // Not supported F9P
    {UBLOX_CFG_MSGOUT_UBX_NAV2_PVT_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_PVT", 0, 130, 130},

    {UBLOX_CFG_MSGOUT_UBX_NAV2_SAT_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_SAT", 0, 130, 130},
    {UBLOX_CFG_MSGOUT_UBX_NAV2_SBAS_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_SBAS", 0, 130, 130},
    {UBLOX_CFG_MSGOUT_UBX_NAV2_SIG_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_SIG", 0, 130, 130},
    {UBLOX_CFG_MSGOUT_UBX_NAV2_SLAS_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_SLAS", 0, 130, 130},
    {UBLOX_CFG_MSGOUT_UBX_NAV2_STATUS_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_STATUS", 0, 130, 130},

    //{UBLOX_CFG_MSGOUT_UBX_NAV2_SVIN_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_SVIN", 0, 9999, 9999},
    ////No support yet
    {UBLOX_CFG_MSGOUT_UBX_NAV2_TIMEBDS_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_TIMEBDS", 0, 130, 130},
    {UBLOX_CFG_MSGOUT_UBX_NAV2_TIMEGAL_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_TIMEGAL", 0, 130, 130},
    {UBLOX_CFG_MSGOUT_UBX_NAV2_TIMEGLO_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_TIMEGLO", 0, 130, 130},
    {UBLOX_CFG_MSGOUT_UBX_NAV2_TIMEGPS_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_TIMEGPS", 0, 130, 130},

    {UBLOX_CFG_MSGOUT_UBX_NAV2_TIMELS_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_TIMELS", 0, 130, 130},
    //{UBLOX_CFG_MSGOUT_UBX_NAV2_TIMENAVIC_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_TIMENAVIC", 0, 9999,
    // 9999}, //No support yet
    {UBLOX_CFG_MSGOUT_UBX_NAV2_TIMEQZSS_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_TIMEQZSS", 0, 130, 130},
    {UBLOX_CFG_MSGOUT_UBX_NAV2_TIMEUTC_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_TIMEUTC", 0, 130, 130},
    {UBLOX_CFG_MSGOUT_UBX_NAV2_VELECEF_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_VELECEF", 0, 130, 130},

    {UBLOX_CFG_MSGOUT_UBX_NAV2_VELNED_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV2_VELNED", 0, 130, 130},

    // NAV
    //{UBLOX_CFG_MSGOUT_UBX_NAV_AOPSTATUS_UART1, UBX_NAV_AOPSTATUS, UBX_CLASS_NAV, 0, "NAV_AOPSTATUS", 0, 9999,
    // 9999}, //Not supported on F9R 121 or F9P 112, 113, 120, 130, 132
    {UBLOX_CFG_MSGOUT_UBX_NAV_ATT_UART1, UBX_NAV_ATT, UBX_CLASS_NAV, 0, "NAV_ATT", 0, 9999,
     120}, // Not supported on F9P 112, 113, 120, 130, 132
    {UBLOX_CFG_MSGOUT_UBX_NAV_CLOCK_UART1, UBX_NAV_CLOCK, UBX_CLASS_NAV, 0, "NAV_CLOCK", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_NAV_COV_UART1, UBX_NAV_COV, UBX_CLASS_NAV, 0, "NAV_COV", 0, 112, 120},
    //{UBLOX_CFG_MSGOUT_UBX_NAV_DGPS_UART1, UBX_NAV_DGPS, UBX_CLASS_NAV, 0, "NAV_DGPS", 0, 9999, 9999}, //Not
    // supported on F9R 121 or F9P 112, 113, 120, 130, 132
    {UBLOX_CFG_MSGOUT_UBX_NAV_DOP_UART1, UBX_NAV_DOP, UBX_CLASS_NAV, 0, "NAV_DOP", 0, 112, 120},

    {UBLOX_CFG_MSGOUT_UBX_NAV_EELL_UART1, UBX_NAV_EELL, UBX_CLASS_NAV, 0, "NAV_EELL", 0, 9999,
     120}, // Not supported on F9P
    {UBLOX_CFG_MSGOUT_UBX_NAV_EOE_UART1, UBX_NAV_EOE, UBX_CLASS_NAV, 0, "NAV_EOE", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_NAV_GEOFENCE_UART1, UBX_NAV_GEOFENCE, UBX_CLASS_NAV, 0, "NAV_GEOFENCE", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_NAV_HPPOSECEF_UART1, UBX_NAV_HPPOSECEF, UBX_CLASS_NAV, 0, "NAV_HPPOSECEF", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_NAV_HPPOSLLH_UART1, UBX_NAV_HPPOSLLH, UBX_CLASS_NAV, 0, "NAV_HPPOSLLH", 0, 112, 120},

    //{UBLOX_CFG_MSGOUT_UBX_NAV_NMI_UART1, UBX_NAV_NMI, UBX_CLASS_NAV, 0, "NAV_NMI", 0, 9999, 9999}, //Not supported
    // on F9R 121 or F9P 112, 113, 120, 130, 132
    {UBLOX_CFG_MSGOUT_UBX_NAV_ODO_UART1, UBX_NAV_ODO, UBX_CLASS_NAV, 0, "NAV_ODO", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_NAV_ORB_UART1, UBX_NAV_ORB, UBX_CLASS_NAV, 0, "NAV_ORB", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_NAV_PL_UART1, UBX_NAV_PL, UBX_CLASS_NAV, 0, "NAV_PL", 0, 9999,
     130}, // Not supported F9R 121 or F9P 112, 113, 120, 130, 132
    {UBLOX_CFG_MSGOUT_UBX_NAV_POSECEF_UART1, UBX_NAV_POSECEF, UBX_CLASS_NAV, 0, "NAV_POSECEF", 0, 112, 120},

    {UBLOX_CFG_MSGOUT_UBX_NAV_POSLLH_UART1, UBX_NAV_POSLLH, UBX_CLASS_NAV, 0, "NAV_POSLLH", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_NAV_PVAT_UART1, UBX_NAV_PVAT, UBX_CLASS_NAV, 0, "NAV_PVAT", 0, 9999,
     121}, // Not supported on F9P 112, 113, 120, 130, F9R 120
    {UBLOX_CFG_MSGOUT_UBX_NAV_PVT_UART1, UBX_NAV_PVT, UBX_CLASS_NAV, 0, "NAV_PVT", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_NAV_TIMEQZSS_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NAV, 0, "NAV_QZSS", 0, 113,
     130}, // Not supported F9R 121 or F9P 112
    {UBLOX_CFG_MSGOUT_UBX_NAV_RELPOSNED_UART1, UBX_NAV_RELPOSNED, UBX_CLASS_NAV, 0, "NAV_RELPOSNED", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_NAV_SAT_UART1, UBX_NAV_SAT, UBX_CLASS_NAV, 0, "NAV_SAT", 0, 112, 120},

    {UBLOX_CFG_MSGOUT_UBX_NAV_SBAS_UART1, UBX_NAV_SBAS, UBX_CLASS_NAV, 0, "NAV_SBAS", 0, 113,
     120}, // Not supported F9P 112
    {UBLOX_CFG_MSGOUT_UBX_NAV_SIG_UART1, UBX_NAV_SIG, UBX_CLASS_NAV, 0, "NAV_SIG", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_NAV_SLAS_UART1, UBX_NAV_SLAS, UBX_CLASS_NAV, 0, "NAV_SLAS", 0, 113,
     130}, // Not supported F9R 121 or F9P 112
           //{UBLOX_CFG_MSGOUT_UBX_NAV_SOL_UART1, UBX_NAV_SOL, UBX_CLASS_NAV, 0, "NAV_SOL", 0, 9999, 9999}, //Not
           // supported F9R 121 or F9P 112, 113, 120, 130, 132
    {UBLOX_CFG_MSGOUT_UBX_NAV_STATUS_UART1, UBX_NAV_STATUS, UBX_CLASS_NAV, 0, "NAV_STATUS", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_NAV_SVIN_UART1, UBX_NAV_SVIN, UBX_CLASS_NAV, 0, "NAV_SVIN", 0, 112,
     9999}, // Not supported on F9R 120, 121, 130
    //{UBLOX_CFG_MSGOUT_UBX_NAV_SVINFO_UART1, UBX_NAV_SVINFO, UBX_CLASS_NAV, 0, "NAV_SVINFO", 0, 9999, 9999}, //Not
    // supported F9R 120 or F9P 112, 113, 120, 130, 132
    {UBLOX_CFG_MSGOUT_UBX_NAV_TIMEBDS_UART1, UBX_NAV_TIMEBDS, UBX_CLASS_NAV, 0, "NAV_TIMEBDS", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_NAV_TIMEGAL_UART1, UBX_NAV_TIMEGAL, UBX_CLASS_NAV, 0, "NAV_TIMEGAL", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_NAV_TIMEGLO_UART1, UBX_NAV_TIMEGLO, UBX_CLASS_NAV, 0, "NAV_TIMEGLO", 0, 112, 120},

    {UBLOX_CFG_MSGOUT_UBX_NAV_TIMEGPS_UART1, UBX_NAV_TIMEGPS, UBX_CLASS_NAV, 0, "NAV_TIMEGPS", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_NAV_TIMELS_UART1, UBX_NAV_TIMELS, UBX_CLASS_NAV, 0, "NAV_TIMELS", 0, 112, 120},
    //{UBLOX_CFG_MSGOUT_UBX_NAV_TIMENAVIC_UART1, UBX_NAV_TIMENAVIC, UBX_CLASS_NAV, 0, "NAV_TIMENAVIC", 0, 9999,
    // 9999}, //Not supported F9R 121 or F9P 132
    {UBLOX_CFG_MSGOUT_UBX_NAV_TIMEUTC_UART1, UBX_NAV_TIMEUTC, UBX_CLASS_NAV, 0, "NAV_TIMEUTC", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_NAV_VELECEF_UART1, UBX_NAV_VELECEF, UBX_CLASS_NAV, 0, "NAV_VELECEF", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_NAV_VELNED_UART1, UBX_NAV_VELNED, UBX_CLASS_NAV, 0, "NAV_VELNED", 0, 112, 120},

    // NMEA NAV2
    // F9P not supported 112, 113, 120. Supported starting 130.
    // F9R not supported 120. Supported starting 130.
    {UBLOX_CFG_MSGOUT_NMEA_NAV2_ID_GGA_UART1, UBX_NMEA_GGA, UBX_CLASS_NMEA, 0, "NMEANAV2_GGA",
     SFE_UBLOX_FILTER_NMEA_GGA, 130, 130},
    {UBLOX_CFG_MSGOUT_NMEA_NAV2_ID_GLL_UART1, UBX_NMEA_GLL, UBX_CLASS_NMEA, 0, "NMEANAV2_GLL",
     SFE_UBLOX_FILTER_NMEA_GLL, 130, 130},
    {UBLOX_CFG_MSGOUT_NMEA_NAV2_ID_GNS_UART1, UBX_NMEA_GNS, UBX_CLASS_NMEA, 0, "NMEANAV2_GNS",
     SFE_UBLOX_FILTER_NMEA_GNS, 130, 130},
    {UBLOX_CFG_MSGOUT_NMEA_NAV2_ID_GSA_UART1, UBX_NMEA_GSA, UBX_CLASS_NMEA, 0, "NMEANAV2_GSA",
     SFE_UBLOX_FILTER_NMEA_GSA, 130, 130},
    {UBLOX_CFG_MSGOUT_NMEA_NAV2_ID_RMC_UART1, UBX_NMEA_RMC, UBX_CLASS_NMEA, 0, "NMEANAV2_RMC",
     SFE_UBLOX_FILTER_NMEA_RMC, 130, 130},

    {UBLOX_CFG_MSGOUT_NMEA_NAV2_ID_VTG_UART1, UBX_NMEA_VTG, UBX_CLASS_NMEA, 0, "NMEANAV2_VTG",
     SFE_UBLOX_FILTER_NMEA_VTG, 130, 130},
    {UBLOX_CFG_MSGOUT_NMEA_NAV2_ID_ZDA_UART1, UBX_NMEA_ZDA, UBX_CLASS_NMEA, 0, "NMEANAV2_ZDA",
     SFE_UBLOX_FILTER_NMEA_ZDA, 130, 130},

    // NMEA
    {UBLOX_CFG_MSGOUT_NMEA_ID_DTM_UART1, UBX_NMEA_DTM, UBX_CLASS_NMEA, 0, "NMEA_DTM", SFE_UBLOX_FILTER_NMEA_DTM, 112,
     120},
    {UBLOX_CFG_MSGOUT_NMEA_ID_GBS_UART1, UBX_NMEA_GBS, UBX_CLASS_NMEA, 0, "NMEA_GBS", SFE_UBLOX_FILTER_NMEA_GBS, 112,
     120},
    {UBLOX_CFG_MSGOUT_NMEA_ID_GGA_UART1, UBX_NMEA_GGA, UBX_CLASS_NMEA, 1, "NMEA_GGA", SFE_UBLOX_FILTER_NMEA_GGA, 112,
     120},
    {UBLOX_CFG_MSGOUT_NMEA_ID_GLL_UART1, UBX_NMEA_GLL, UBX_CLASS_NMEA, 0, "NMEA_GLL", SFE_UBLOX_FILTER_NMEA_GLL, 112,
     120},
    {UBLOX_CFG_MSGOUT_NMEA_ID_GNS_UART1, UBX_NMEA_GNS, UBX_CLASS_NMEA, 0, "NMEA_GNS", SFE_UBLOX_FILTER_NMEA_GNS, 112,
     120},

    {UBLOX_CFG_MSGOUT_NMEA_ID_GRS_UART1, UBX_NMEA_GRS, UBX_CLASS_NMEA, 0, "NMEA_GRS", SFE_UBLOX_FILTER_NMEA_GRS, 112,
     120},
    {UBLOX_CFG_MSGOUT_NMEA_ID_GSA_UART1, UBX_NMEA_GSA, UBX_CLASS_NMEA, 1, "NMEA_GSA", SFE_UBLOX_FILTER_NMEA_GSA, 112,
     120},
    {UBLOX_CFG_MSGOUT_NMEA_ID_GST_UART1, UBX_NMEA_GST, UBX_CLASS_NMEA, 1, "NMEA_GST", SFE_UBLOX_FILTER_NMEA_GST, 112,
     120},
    {UBLOX_CFG_MSGOUT_NMEA_ID_GSV_UART1, UBX_NMEA_GSV, UBX_CLASS_NMEA, 4, "NMEA_GSV", SFE_UBLOX_FILTER_NMEA_GSV, 112,
     120}, // Default to 1 update every 4 fixes
    {UBLOX_CFG_MSGOUT_NMEA_ID_RLM_UART1, UBX_NMEA_RLM, UBX_CLASS_NMEA, 0, "NMEA_RLM", SFE_UBLOX_FILTER_NMEA_RLM, 113,
     120}, // No F9P 112 support

    {UBLOX_CFG_MSGOUT_NMEA_ID_RMC_UART1, UBX_NMEA_RMC, UBX_CLASS_NMEA, 1, "NMEA_RMC", SFE_UBLOX_FILTER_NMEA_RMC, 112,
     120},
    {UBLOX_CFG_MSGOUT_NMEA_ID_THS_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_NMEA, 0, "NMEA_THS", SFE_UBLOX_FILTER_NMEA_THS,
     9999, 120}, // Not supported F9P 112, 113, 120, 130, 132
    {UBLOX_CFG_MSGOUT_NMEA_ID_VLW_UART1, UBX_NMEA_VLW, UBX_CLASS_NMEA, 0, "NMEA_VLW", SFE_UBLOX_FILTER_NMEA_VLW, 112,
     120},
    {UBLOX_CFG_MSGOUT_NMEA_ID_VTG_UART1, UBX_NMEA_VTG, UBX_CLASS_NMEA, 0, "NMEA_VTG", SFE_UBLOX_FILTER_NMEA_VTG, 112,
     120},
    {UBLOX_CFG_MSGOUT_NMEA_ID_ZDA_UART1, UBX_NMEA_ZDA, UBX_CLASS_NMEA, 0, "NMEA_ZDA", SFE_UBLOX_FILTER_NMEA_ZDA, 112,
     120},

    // PUBX
    // F9P support 130
    {UBLOX_CFG_MSGOUT_PUBX_ID_POLYP_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_PUBX, 0, "PUBX_POLYP", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_PUBX_ID_POLYS_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_PUBX, 0, "PUBX_POLYS", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_PUBX_ID_POLYT_UART1, UBX_ID_NOT_AVAILABLE, UBX_CLASS_PUBX, 0, "PUBX_POLYT", 0, 112, 120},

    // RTCM
    {UBLOX_CFG_MSGOUT_RTCM_3X_TYPE1005_UART1, UBX_RTCM_1005, UBX_RTCM_MSB, 1, "RTCM_1005",
     SFE_UBLOX_FILTER_RTCM_TYPE1005, 112, 9999}, // Not supported on F9R
    {UBLOX_CFG_MSGOUT_RTCM_3X_TYPE1074_UART1, UBX_RTCM_1074, UBX_RTCM_MSB, 1, "RTCM_1074",
     SFE_UBLOX_FILTER_RTCM_TYPE1074, 112, 9999},
    {UBLOX_CFG_MSGOUT_RTCM_3X_TYPE1077_UART1, UBX_RTCM_1077, UBX_RTCM_MSB, 0, "RTCM_1077",
     SFE_UBLOX_FILTER_RTCM_TYPE1077, 112, 9999},
    {UBLOX_CFG_MSGOUT_RTCM_3X_TYPE1084_UART1, UBX_RTCM_1084, UBX_RTCM_MSB, 1, "RTCM_1084",
     SFE_UBLOX_FILTER_RTCM_TYPE1084, 112, 9999},
    {UBLOX_CFG_MSGOUT_RTCM_3X_TYPE1087_UART1, UBX_RTCM_1087, UBX_RTCM_MSB, 0, "RTCM_1087",
     SFE_UBLOX_FILTER_RTCM_TYPE1087, 112, 9999},

    {UBLOX_CFG_MSGOUT_RTCM_3X_TYPE1094_UART1, UBX_RTCM_1094, UBX_RTCM_MSB, 1, "RTCM_1094",
     SFE_UBLOX_FILTER_RTCM_TYPE1094, 112, 9999},
    {UBLOX_CFG_MSGOUT_RTCM_3X_TYPE1097_UART1, UBX_RTCM_1097, UBX_RTCM_MSB, 0, "RTCM_1097",
     SFE_UBLOX_FILTER_RTCM_TYPE1097, 112, 9999},
    {UBLOX_CFG_MSGOUT_RTCM_3X_TYPE1124_UART1, UBX_RTCM_1124, UBX_RTCM_MSB, 1, "RTCM_1124",
     SFE_UBLOX_FILTER_RTCM_TYPE1124, 112, 9999},
    {UBLOX_CFG_MSGOUT_RTCM_3X_TYPE1127_UART1, UBX_RTCM_1127, UBX_RTCM_MSB, 0, "RTCM_1127",
     SFE_UBLOX_FILTER_RTCM_TYPE1127, 112, 9999},
    {UBLOX_CFG_MSGOUT_RTCM_3X_TYPE1230_UART1, UBX_RTCM_1230, UBX_RTCM_MSB, 10, "RTCM_1230",
     SFE_UBLOX_FILTER_RTCM_TYPE1230, 112, 9999},

    {UBLOX_CFG_MSGOUT_RTCM_3X_TYPE4072_0_UART1, UBX_RTCM_4072_0, UBX_RTCM_MSB, 0, "RTCM_4072_0",
     SFE_UBLOX_FILTER_RTCM_TYPE4072_0, 112, 9999},
    {UBLOX_CFG_MSGOUT_RTCM_3X_TYPE4072_1_UART1, UBX_RTCM_4072_1, UBX_RTCM_MSB, 0, "RTCM_4072_1",
     SFE_UBLOX_FILTER_RTCM_TYPE4072_1, 112, 9999},

    // RXM
    //{UBLOX_CFG_MSGOUT_UBX_RXM_ALM_UART1, UBX_RXM_ALM, UBX_CLASS_RXM, 0, "RXM_ALM", 0, 9999, 9999}, //Not supported
    // F9R 121 or F9P 112, 113, 120, 130, 132
    {UBLOX_CFG_MSGOUT_UBX_RXM_COR_UART1, UBX_RXM_COR, UBX_CLASS_RXM, 0, "RXM_COR", 0, 9999,
     130}, // Not supported F9R 121 or F9P 112, 113, 120, 130, 132
    //{UBLOX_CFG_MSGOUT_UBX_RXM_EPH_UART1, UBX_RXM_EPH, UBX_CLASS_RXM, 0, "RXM_EPH", 0, 9999, 9999},
    // Not supported F9R 121 or F9P 112, 113, 120, 130, 132
    //{UBLOX_CFG_MSGOUT_UBX_RXM_IMES_UART1, UBX_RXM_IMES, UBX_CLASS_RXM, 0, "RXM_IMES", 0, 9999, 9999},
    // Not supported F9R 121 or F9P 112, 113, 120, 130, 132
    //{UBLOX_CFG_MSGOUT_UBX_RXM_MEAS20_UART1, UBX_RXM_MEAS20, UBX_CLASS_RXM, 0, "RXM_MEAS20", 0, 9999, 9999},
    // Not supported F9R 121 or F9P 112, 113, 120, 130, 132
    //{UBLOX_CFG_MSGOUT_UBX_RXM_MEAS50_UART1, UBX_RXM_MEAS50, UBX_CLASS_RXM, 0, "RXM_MEAS50", 0, 9999, 9999},
    // Not supported F9R 121 or F9P 112, 113, 120, 130, 132
    //{UBLOX_CFG_MSGOUT_UBX_RXM_MEASC12_UART1, UBX_RXM_MEASC12, UBX_CLASS_RXM, 0, "RXM_MEASC12", 0, 9999, 9999},
    // Not supported F9R 121 or F9P 112, 113, 120, 130, 132
    //{UBLOX_CFG_MSGOUT_UBX_RXM_MEASD12_UART1, UBX_RXM_MEASD12, UBX_CLASS_RXM, 0, "RXM_MEASD12", 0, 9999, 9999},
    // Not supported F9R 121 or F9P 112, 113, 120, 130, 132
    {UBLOX_CFG_MSGOUT_UBX_RXM_MEASX_UART1, UBX_RXM_MEASX, UBX_CLASS_RXM, 0, "RXM_MEASX", 0, 112, 120},
    //{UBLOX_CFG_MSGOUT_UBX_RXM_PMP_UART1, UBX_RXM_PMP, UBX_CLASS_RXM, 0, "RXM_PMP", 0, 9999, 9999},
    // Not supported F9R 121 or F9P 112, 113, 120, 130, 132
    //{UBLOX_CFG_MSGOUT_UBX_RXM_QZSSL6_UART1, UBX_RXM_QZSSL6, UBX_CLASS_RXM, 0, "RXM_QZSSL6", 0, 9999, 9999},
    // Not supported F9R 121, F9P 112, 113, 120, 130
    {UBLOX_CFG_MSGOUT_UBX_RXM_RAWX_UART1, UBX_RXM_RAWX, UBX_CLASS_RXM, 0, "RXM_RAWX", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_RXM_RLM_UART1, UBX_RXM_RLM, UBX_CLASS_RXM, 0, "RXM_RLM", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_RXM_RTCM_UART1, UBX_RXM_RTCM, UBX_CLASS_RXM, 0, "RXM_RTCM", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_RXM_SFRBX_UART1, UBX_RXM_SFRBX, UBX_CLASS_RXM, 0, "RXM_SFRBX", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_RXM_SPARTN_UART1, UBX_RXM_SPARTN, UBX_CLASS_RXM, 0, "RXM_SPARTN", 0, 9999,
     121}, // Not supported F9R 120 or F9P 112, 113, 120, 130
    //{UBLOX_CFG_MSGOUT_UBX_RXM_SVSI_UART1, UBX_RXM_SVSI, UBX_CLASS_RXM, 0, "RXM_SVSI", 0, 9999, 9999},
    // Not supported F9R 121 or F9P 112, 113, 120, 130, 132
    //{UBLOX_CFG_MSGOUT_UBX_RXM_TM_UART1, UBX_RXM_TM, UBX_CLASS_RXM, 0, "RXM_TM", 0, 9999, 9999},
    // Not supported F9R 121 or F9P 112, 113, 120, 130, 132

    // SEC
    // No support F9P 112.

    // TIM
    //{UBLOX_CFG_MSGOUT_UBX_TIM_SVIN_UART1, UBX_TIM_SVIN, UBX_CLASS_TIM, 0, "TIM_SVIN", 0, 9999, 9999}, //Appears on
    // F9P 132 but not supported
    {UBLOX_CFG_MSGOUT_UBX_TIM_TM2_UART1, UBX_TIM_TM2, UBX_CLASS_TIM, 0, "TIM_TM2", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_TIM_TP_UART1, UBX_TIM_TP, UBX_CLASS_TIM, 0, "TIM_TP", 0, 112, 120},
    {UBLOX_CFG_MSGOUT_UBX_TIM_VRFY_UART1, UBX_TIM_VRFY, UBX_CLASS_TIM, 0, "TIM_VRFY", 0, 112, 120},
};

#define MAX_UBX_MSG (sizeof(ubxMessages) / sizeof(ubxMsg))
#define MAX_UBX_MSG_RTCM (12)

#define MAX_SET_MESSAGES_RETRIES 5 // Try up to five times to set all the messages. Occasionally fails if set to 2

// Struct to describe the necessary info for each UBX command
// Each command will have a key, and minimum F9P/F9R versions that support that command
typedef struct
{
    const uint32_t cmdKey;
    const char cmdTextName[30];
    const uint16_t f9pFirmwareVersionSupported; // The minimum version this message is supported. 0 = all versions. 9999
                                                // = Not supported
    const uint16_t f9rFirmwareVersionSupported;
} ubxCmd;

// Static array containing all the compatible commands
const ubxCmd ubxCommands[] = {
    {UBLOX_CFG_TMODE_MODE, "CFG_TMODE_MODE", 0, 9999}, // Survey mode is only available on ZED-F9P modules

    {UBLOX_CFG_UART1OUTPROT_RTCM3X, "CFG_UART1OUTPROT_RTCM3X", 0, 9999}, // RTCM not supported on F9R
    {UBLOX_CFG_UART1INPROT_SPARTN, "CFG_UART1INPROT_SPARTN", 120,
     9999}, // Supported on F9P 120 and up. Not supported on F9R 120.

    {UBLOX_CFG_UART2OUTPROT_RTCM3X, "CFG_UART2OUTPROT_RTCM3X", 0, 9999}, // RTCM not supported on F9R
    {UBLOX_CFG_UART2INPROT_SPARTN, "CFG_UART2INPROT_SPARTN", 120, 9999}, //

    {UBLOX_CFG_SPIOUTPROT_RTCM3X, "CFG_SPIOUTPROT_RTCM3X", 0, 9999}, // RTCM not supported on F9R
    {UBLOX_CFG_SPIINPROT_SPARTN, "CFG_SPIINPROT_SPARTN", 120, 9999}, //

    {UBLOX_CFG_I2COUTPROT_RTCM3X, "CFG_I2COUTPROT_RTCM3X", 0, 9999}, // RTCM not supported on F9R
    {UBLOX_CFG_I2CINPROT_SPARTN, "CFG_I2CINPROT_SPARTN", 120, 9999}, //

    {UBLOX_CFG_USBOUTPROT_RTCM3X, "CFG_USBOUTPROT_RTCM3X", 0, 9999}, // RTCM not supported on F9R
    {UBLOX_CFG_USBINPROT_SPARTN, "CFG_USBINPROT_SPARTN", 120, 9999}, //

    {UBLOX_CFG_NAV2_OUT_ENABLED, "CFG_NAV2_OUT_ENABLED", 130,
     130}, // Supported on F9P 130 and up. Supported on F9R 130 and up.
    {UBLOX_CFG_NAVSPG_INFIL_MINCNO, "CFG_NAVSPG_INFIL_MINCNO", 0, 0}, //
};

#define MAX_UBX_CMD (sizeof(ubxCommands) / sizeof(ubxCmd))

class GNSS_ZED : GNSS
{
  private:
    // Use Michael's lock/unlock methods to prevent the GNSS UART task from
    // calling checkUblox during a sendCommand and waitForResponse.
    // Also prevents pushRawData from being called.
    //
    // Revert to a simple bool lock. The Mutex was causing occasional panics caused by
    // vTaskPriorityDisinheritAfterTimeout in lock() (I think possibly / probably caused by the GNSS not being pinned to
    // one core?
    bool iAmLocked = false;

    SFE_UBLOX_GNSS_SUPER *_zed = nullptr; // Library class instance

    // Record rxBytes so we can tell if Radio Ext (COM2) is receiving correction data.
    // On the mosaic, we know that InputLink will arrive at 1Hz. But on the ZED, UBX-MON-COMMS
    // is tied to the navigation rate. To keep it simple, record the last time rxBytes
    // was seen to increase and use that for corrections timeout. This is updated by the
    // UBX-MON-COMMS callback. isCorrRadioExtPortActive returns true if the bytes-received has
    // increased in the previous settings.correctionsSourcesLifetime_s
    uint32_t _radioExtBytesReceived_millis;

    // Given a sub type (ie "RTCM", "NMEA") present menu showing messages with this subtype
    // Controls the messages that get broadcast over Bluetooth and logged (if enabled)
    void menuMessagesSubtype(uint8_t *localMessageRate, const char *messageType);

  public:
    // Constructor
    GNSS_ZED() : _radioExtBytesReceived_millis(0), GNSS()
    {
    }

    uint8_t aStatus = SFE_UBLOX_ANTENNA_STATUS_DONTKNOW;

    // If we have decryption keys, configure module
    // Note: don't check online.lband_neo here. We could be using ip corrections
    void applyPointPerfectKeys();

    // Set RTCM for base mode to defaults (1005/1074/1084/1094/1124 1Hz & 1230 0.1Hz)
    void baseRtcmDefault();

    // Reset to Low Bandwidth Link (1074/1084/1094/1124 0.5Hz & 1005/1230 0.1Hz)
    virtual void baseRtcmLowDataRate();

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

    // Setup the timepulse output on the PPS pin for external triggering
    // Outputs
    //   Returns true if the pin was successfully setup and false upon
    //   failure
    bool beginPPS();

    bool checkNMEARates();

    bool checkPPPRates();

    // Configure the Base
    // Outputs:
    //   Returns true if successfully configured and false upon failure
    bool configureBase();

    // Configure specific aspects of the receiver for NTP mode
    bool configureNtpMode();

    // Setup the general configuration of the GNSS
    // Not Rover or Base specific (ie, baud rates)
    // Outputs:
    //   Returns true if successfully configured and false upon failure
    bool configureGNSS();

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

    void enableGgaForNtrip();

    // Enable RTCM 1230. This is the GLONASS bias sentence and is transmitted
    // even if there is no GPS fix. We use it to test serial output.
    // Outputs:
    //   Returns true if successfully started and false upon failure
    bool enableRTCMTest();

    // Restore the GNSS to the factory settings
    void factoryReset();

    uint16_t fileBufferAvailable();

    uint16_t fileBufferExtractData(uint8_t *fileBuffer, int fileBytesToRead);

    // Start the base using fixed coordinates
    // Outputs:
    //   Returns true if successfully started and false upon failure
    bool fixedBaseStart();

    // Return the number of active/enabled messages
    uint8_t getActiveMessageCount();

    // Return the number of active/enabled RTCM messages
    uint8_t getActiveRtcmMessageCount();

    // Get the altitude
    // Outputs:
    //   Returns the altitude in meters or zero if the GNSS is offline
    double getAltitude();

    // Returns the carrier solution or zero if not online
    uint8_t getCarrierSolution();

    virtual uint32_t getDataBaudRate();

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

    // Given the name of a message, return the array number
    uint8_t getMessageNumberByName(const char *msgName);

    // Given the name of a message, find it, and return the rate
    uint8_t getMessageRateByName(const char *msgName);

    // Returns two digits of milliseconds or zero if not online
    uint8_t getMillisecond();

    // Returns minutes or zero if not online
    uint8_t getMinute();

    // Returns month number or zero if not online
    uint8_t getMonth();

    // Returns nanoseconds or zero if not online
    uint32_t getNanosecond();

    // Count the number of NAV2 messages with rates more than 0. Used for determining if we need the enable
    // the global NAV2 feature.
    uint8_t getNAV2MessageCount();

    virtual uint32_t getRadioBaudRate();

    // Returns the seconds between solutions
    double getRateS();

    const char *getRtcmDefaultString();

    const char *getRtcmLowDataRateString();

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

    bool isPppConverged();

    bool isPppConverging();

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

    // Disable data output from the NEO
    bool lBandCommunicationDisable();

    // Enable data output from the NEO
    bool lBandCommunicationEnable();

    bool lock(void);

    bool lockCreate(void);

    void lockDelete(void);

    // Controls the constellations that are used to generate a fix and logged
    void menuConstellations();

    void menuMessageBaseRtcm();

    // Control the messages that get broadcast over Bluetooth and logged (if enabled)
    void menuMessages();

    // Print the module type and firmware version
    void printModuleInfo();

    // Send correction data to the GNSS
    // Inputs:
    //   dataToSend: Address of a buffer containing the data
    //   dataLength: The number of valid data bytes in the buffer
    // Outputs:
    //   Returns the number of correction data bytes written
    int pushRawData(uint8_t *dataToSend, int dataLength);

    uint16_t rtcmBufferAvailable();

    // If L-Band is available, but encrypted, allow RTCM through other sources (radio, ESP-NOW) to GNSS receiver
    void rtcmOnGnssDisable();

    // If LBand is being used, ignore any RTCM that may come in from the GNSS
    void rtcmOnGnssEnable();

    uint16_t rtcmRead(uint8_t *rtcmBuffer, int rtcmBytesToRead);

    // Save the current configuration
    // Outputs:
    //   Returns true when the configuration was saved and false upon failure
    bool saveConfiguration();

    // Set the baud rate on the designated port
    bool setBaudRate(uint8_t uartNumber, uint32_t baudRate); // From the super class

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

    // Given a unique string, find first and last records containing that string in message array
    void setMessageOffsets(const ubxMsg *localMessage, const char *messageType, int &startOfBlock, int &endOfBlock);

    // Given the name of a message, find it, and set the rate
    bool setMessageRateByName(const char *msgName, uint8_t msgRate);

    // Enable all the valid messages for this platform
    bool setMessages(int maxRetries);

    // Enable all the valid messages for this platform over the USB port
    bool setMessagesUsb(int maxRetries);

    // Set the minimum satellite signal level for navigation.
    bool setMinCnoRadio(uint8_t cnoValue);

    // Set the dynamic model to use for RTK
    // Inputs:
    //   modelNumber: Number of the model to use, provided by radio library
    bool setModel(uint8_t modelNumber);

    // Given the name of a message, find it, and set the rate
    bool setNmeaMessageRateByName(const char *msgName, uint8_t msgRate);

    // Specify the interval between solutions
    // Inputs:
    //   secondsBetweenSolutions: Number of seconds between solutions
    // Outputs:
    //   Returns true if the rate was successfully set and false upon
    //   failure
    bool setRate(double secondsBetweenSolutions);

    bool setTalkerGNGGA();

    // Hotstart GNSS to try to get RTK lock
    bool softwareReset();

    bool standby();

    // Callback to save the high precision data
    // Inputs:
    //   ubxDataStruct: Address of an UBX_NAV_HPPOSLLH_data_t structure
    //                  containing the high precision position data
    void storeHPdataRadio(UBX_NAV_HPPOSLLH_data_t *ubxDataStruct);

    // Callback to save the PVT data
    void storePVTdataRadio(UBX_NAV_PVT_data_t *ubxDataStruct);

    // Callback to store MON-COMMS information
    void storeMONCOMMSdataRadio(UBX_MON_COMMS_data_t *ubxDataStruct);

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

    int ubxConstellationIDToIndex(int id);

    void unlock(void);

    // Poll routine to update the GNSS state
    void update();

    void updateCorrectionsSource(uint8_t source);
};

#endif // COMPILE_ZED
#endif // __GNSS_ZED_H__
