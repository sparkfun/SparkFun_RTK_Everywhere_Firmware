// These are the callbacks that get regularly called, globals are updated
void storePVTdata(UBX_NAV_PVT_data_t *ubxDataStruct)
{
    altitude = ubxDataStruct->height / 1000.0;

    gnssDay = ubxDataStruct->day;
    gnssMonth = ubxDataStruct->month;
    gnssYear = ubxDataStruct->year;

    gnssHour = ubxDataStruct->hour;
    gnssMinute = ubxDataStruct->min;
    gnssSecond = ubxDataStruct->sec;
    gnssNano = ubxDataStruct->nano;
    mseconds = ceil((ubxDataStruct->iTOW % 1000) / 10.0); // Limit to first two digits

    numSV = ubxDataStruct->numSV;
    fixType = ubxDataStruct->fixType;
    carrSoln = ubxDataStruct->flags.bits.carrSoln;

    validDate = ubxDataStruct->valid.bits.validDate;
    validTime = ubxDataStruct->valid.bits.validTime;
    fullyResolved = ubxDataStruct->valid.bits.fullyResolved;
    tAcc = ubxDataStruct->tAcc;
    confirmedDate = ubxDataStruct->flags2.bits.confirmedDate;
    confirmedTime = ubxDataStruct->flags2.bits.confirmedTime;

    pvtArrivalMillis = millis();
    pvtUpdated = true;
}

void storeHPdata(UBX_NAV_HPPOSLLH_data_t *ubxDataStruct)
{
    horizontalAccuracy = ((float)ubxDataStruct->hAcc) / 10000.0; // Convert hAcc from mm*0.1 to m

    latitude = ((double)ubxDataStruct->lat) / 10000000.0;
    latitude += ((double)ubxDataStruct->latHp) / 1000000000.0;
    longitude = ((double)ubxDataStruct->lon) / 10000000.0;
    longitude += ((double)ubxDataStruct->lonHp) / 1000000000.0;
}

void storeTIMTPdata(UBX_TIM_TP_data_t *ubxDataStruct)
{
    uint32_t tow = ubxDataStruct->week - SFE_UBLOX_JAN_1ST_2020_WEEK; // Calculate the number of weeks since Jan 1st
                                                                      // 2020
    tow *= SFE_UBLOX_SECS_PER_WEEK;     // Convert weeks to seconds
    tow += SFE_UBLOX_EPOCH_WEEK_2086;   // Add the TOW for Jan 1st 2020
    tow += ubxDataStruct->towMS / 1000; // Add the TOW for the next TP

    uint32_t us = ubxDataStruct->towMS % 1000; // Extract the milliseconds
    us *= 1000;                                // Convert to microseconds

    double subMS = ubxDataStruct->towSubMS; // Get towSubMS (ms * 2^-32)
    subMS *= pow(2.0, -32.0);               // Convert to milliseconds
    subMS *= 1000;                          // Convert to microseconds

    us += (uint32_t)subMS; // Add subMS

    timTpEpoch = tow;
    timTpMicros = us;
    timTpArrivalMillis = millis();
    timTpUpdated = true;
}

void storeMONHWdata(UBX_MON_HW_data_t *ubxDataStruct)
{
    aStatus = ubxDataStruct->aStatus;
}

void storeRTCM1005data(RTCM_1005_data_t *rtcmData1005)
{
    ARPECEFX = rtcmData1005->AntennaReferencePointECEFX;
    ARPECEFY = rtcmData1005->AntennaReferencePointECEFY;
    ARPECEFZ = rtcmData1005->AntennaReferencePointECEFZ;
    ARPECEFH = 0;
    newARPAvailable = true;
}

void storeRTCM1006data(RTCM_1006_data_t *rtcmData1006)
{
    ARPECEFX = rtcmData1006->AntennaReferencePointECEFX;
    ARPECEFY = rtcmData1006->AntennaReferencePointECEFY;
    ARPECEFZ = rtcmData1006->AntennaReferencePointECEFZ;
    ARPECEFH = rtcmData1006->AntennaHeight;
    newARPAvailable = true;
}


