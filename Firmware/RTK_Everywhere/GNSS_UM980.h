
/*------------------------------------------------------------------------------
GNSS_UM980.h

  Declarations and definitions for the UM980 GNSS receiver and the GNSS_UM980 class
------------------------------------------------------------------------------*/

#ifndef __GNSS_UM980_H__
#define __GNSS_UM980_H__

#ifdef COMPILE_UM980

#include <SparkFun_Unicore_GNSS_Arduino_Library.h> //http://librarymanager/All#SparkFun_Unicore_GNSS

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
    {"BeiDou", "BDS"}, {"Galileo", "GAL"}, {"GLONASS", "GLO"}, {"GPS", "GPS"}, {"QZSS", "QZSS"},
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
// Rate = Reports per second
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

class GNSS_UM980 : GNSS
{
  private:
    UM980 *_um980; // Library class instance

  protected:
    bool configureOnce();

    // Setup the general configuration of the GNSS
    // Not Rover or Base specific (ie, baud rates)
    // Outputs:
    //   Returns true if successfully configured and false upon failure
    bool configure();

    // Turn off all NMEA and RTCM
    void disableAllOutput();

    uint8_t getActiveNmeaMessageCount();

    // Given the name of an NMEA message, return the array number
    uint8_t getNmeaMessageNumberByName(const char *msgName);

    // Given the name of an RTCM message, return the array number
    uint8_t getRtcmMessageNumberByName(const char *msgName);

    // Return true if the GPGGA message is active
    bool isGgaActive();

    // Given a sub type (ie "RTCM", "NMEA") present menu showing messages with this subtype
    // Controls the messages that get broadcast over Bluetooth and logged (if enabled)
    void menuMessagesSubtype(float *localMessageRate, const char *messageType);

    bool setHighAccuracyService(bool enableGalileoHas);

    // Set the minimum satellite signal level for navigation.
    bool setMinCno(uint8_t cnoValue);

  public:
    // Constructor
    GNSS_UM980() : GNSS()
    {
    }

    // If we have decryption keys, configure module
    // Note: don't check online.lband_neo here. We could be using ip corrections
    void applyPointPerfectKeys();

    // Set RTCM for base mode to defaults (1005/1074/1084/1094/1124 1Hz & 1230 0.1Hz)
    void baseRtcmDefault();

    // Reset to Low Bandwidth Link (1074/1084/1094/1124 0.5Hz & 1005/1230 0.1Hz)
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

    // Configure specific aspects of the receiver for NTP mode
    bool configureNtpMode();

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

    // Get the minimum satellite signal level for navigation.
    uint8_t getMinCno();

    // Returns minutes or zero if not online
    uint8_t getMinute();

    // Returns the current mode
    // 0 - Unknown, 1 - Rover Survey, 2 - Rover UAV, 3 - Rover Auto, 4 - Base Survey-in, 5 - Base fixed
    uint8_t getMode();

    // Returns month number or zero if not online
    uint8_t getMonth();

    // Returns nanoseconds or zero if not online
    uint32_t getNanosecond();

    uint32_t getRadioBaudRate();

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

    float getSurveyInStartingAccuracy();

    // Returns timing accuracy or zero if not online
    uint32_t getTimeAccuracy();

    // Returns full year, ie 2023, not 23.
    uint16_t getYear();

    // Returns true if the device is in Rover mode
    // Currently the only two modes are Rover or Base
    bool inRoverMode();

    bool isBlocking();

    // Date is confirmed once we have GNSS fix
    bool isConfirmedDate();

    // Date is confirmed once we have GNSS fix
    bool isConfirmedTime();

    // Returns true if data is arriving on the Radio Ext port
    bool isCorrRadioExtPortActive()
    {
        return false;
    }

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

    // Hardware or software reset the GNSS
    bool reset();

    // If LBand is being used, ignore any RTCM that may come in from the GNSS
    void rtcmOnGnssDisable();

    // If L-Band is available, but encrypted, allow RTCM through other sources (radio, ESP-NOW) to GNSS receiver
    void rtcmOnGnssEnable();

    uint16_t rtcmRead(uint8_t *rtcmBuffer, int rtcmBytesToRead);

    // Save the current configuration
    // Outputs:
    //   Returns true when the configuration was saved and false upon failure
    bool saveConfiguration();

    bool setBaudRate(uint8_t uartNumber, uint32_t baudRate); // From the super class

    // Set the baud rate on the GNSS port that interfaces between the ESP32 and the GNSS
    // Inputs:
    //   baudRate: The desired baudrate
    bool setBaudRateComm(uint32_t baudRate);

    bool setBaudRateData(uint32_t baudRate);

    bool setBaudRateRadio(uint32_t baudRate);

    // Enable all the valid constellations and bands for this platform
    bool setConstellations();

    // Enable / disable corrections protocol(s) on the Radio External port
    bool setCorrRadioExtPort(bool enable, bool force)
    {
        return true;
    }

    // Set the elevation in degrees
    // Inputs:
    //   elevationDegrees: The elevation value in degrees
    bool setElevation(uint8_t elevationDegrees);

    // Turn on all the enabled NMEA messages on COM3
    bool setMessagesNMEA();

    // Turn on all the enabled RTCM Rover messages on COM3
    bool setMessagesRTCMRover();

    // Turn on all the enabled RTCM Base messages on COM3
    bool setMessagesRTCMBase();

    // Set the dynamic model to use for RTK
    // Inputs:
    //   modelNumber: Number of the model to use, provided by radio library
    bool setModel(uint8_t modelNumber);

    bool setMultipathMitigation(bool enableMultipathMitigation);

    // Given the name of a message, find it, and set the rate
    bool setNmeaMessageRateByName(const char *msgName, uint8_t msgRate);

    // Configure the Pulse-per-second pin based on user settings
    bool setPPS();

    // Set all RTCM Rover message report rates to one value
    void setRtcmRoverMessageRates(uint8_t msgRate);

    // Given the name of a message, find it, and set the rate
    bool setRtcmRoverMessageRateByName(const char *msgName, uint8_t msgRate);

    // Specify the interval between solutions
    // Inputs:
    //   secondsBetweenSolutions: Number of seconds between solutions
    // Outputs:
    //   Returns true if the rate was successfully set and false upon
    //   failure
    bool setRate(double secondsBetweenSolutions);

    bool setTalkerGNGGA();

    // Enable/disable any output needed for tilt compensation
    bool setTilt();

    bool standby();

    // Reset the survey-in operation
    // Outputs:
    //   Returns true if the survey-in operation was reset successfully
    //   and false upon failure
    bool surveyInReset();

    // Start the survey-in operation
    // Outputs:
    //   Return true if successful and false upon failure
    bool surveyInStart();

    // If we have received serial data from the UM980 outside of the Unicore library (ie, from processUart1Message task)
    // we can pass data back into the Unicore library to allow it to update its own variables
    void unicoreHandler(uint8_t *buffer, int length);

    // Poll routine to update the GNSS state
    void update();
};

#endif // COMPILE_UM980
#endif // __GNSS_UM980_H__
