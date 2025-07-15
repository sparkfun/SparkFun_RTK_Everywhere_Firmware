/*------------------------------------------------------------------------------
GNSS_LG290P.h

  Declarations and definitions for the LG290P GNSS receiver and the GNSS_LG290P class
------------------------------------------------------------------------------*/

#ifndef __GNSS_LG290P_H__
#define __GNSS_LG290P_H__

#ifdef COMPILE_LG290P

#include <SparkFun_LG290P_GNSS.h> //http://librarymanager/All#SparkFun_LG290P

// Constellations monitored/used for fix
// Available constellations: GPS, BDS, GLO, GAL, QZSS, NavIC
const char *lg290pConstellationNames[] = {
    "GPS", "GLONASS", "Galileo", "BeiDou", "QZSS", "NavIC",
};

#define MAX_LG290P_CONSTELLATIONS (6)

// Struct to describe support messages on the LG290P
// Each message will have the serial command and its default value
typedef struct
{
    const char msgTextName[11];
    const float msgDefaultRate;
    const uint8_t firmwareVersionSupported; // The minimum version this message is supported.
                                            // 0 = all versions.
                                            // 9999 = Not supported
} lg290pMsg;

// Static array containing all the compatible messages
// Rate = Output once every N position fix(es).
const lg290pMsg lgMessagesNMEA[] = {
    {"RMC", 1, 0}, {"GGA", 1, 0}, {"GSV", 1, 0}, {"GSA", 1, 0}, {"VTG", 1, 0},
    {"GLL", 1, 0}, {"GBS", 0, 4}, {"GNS", 0, 4}, {"GST", 1, 4}, {"ZDA", 0, 4},
};

const lg290pMsg lgMessagesRTCM[] = {
    {"RTCM3-1005", 1, 0}, {"RTCM3-1006", 0, 0},

    {"RTCM3-1019", 0, 0},

    {"RTCM3-1020", 0, 0},

    {"RTCM3-1033", 0, 4}, // v4 and above

    {"RTCM3-1041", 0, 0}, {"RTCM3-1042", 0, 0}, {"RTCM3-1044", 0, 0}, {"RTCM3-1046", 0, 0},

    {"RTCM3-107X", 1, 0}, {"RTCM3-108X", 1, 0}, {"RTCM3-109X", 1, 0}, {"RTCM3-111X", 1, 0},
    {"RTCM3-112X", 1, 0}, {"RTCM3-113X", 1, 0},
};

// Quectel Proprietary messages
const lg290pMsg lgMessagesPQTM[] = {
    {"EPE", 0, 0},
    {"PVT", 0, 0},
};

#define MAX_LG290P_NMEA_MSG (sizeof(lgMessagesNMEA) / sizeof(lg290pMsg))
#define MAX_LG290P_RTCM_MSG (sizeof(lgMessagesRTCM) / sizeof(lg290pMsg))
#define MAX_LG290P_PQTM_MSG (sizeof(lgMessagesPQTM) / sizeof(lg290pMsg))

enum lg290p_Models
{
    // LG290P does not have models
    LG290P_DYN_MODEL_SURVEY = 0,
    LG290P_DYN_MODEL_UAV,
    LG290P_DYN_MODEL_AUTOMOTIVE,
};

class GNSS_LG290P : GNSS
{
  private:
    LG290P *_lg290p; // Library class instance

  protected:
    bool configureOnce();

    // Setup the general configuration of the GNSS
    // Not Rover or Base specific (ie, baud rates)
    // Outputs:
    //   Returns true if successfully configured and false upon failure
    bool configureGNSS();

    // Turn on all the enabled NMEA messages on COM3
    bool enableNMEA();

    // Turn on all the enabled RTCM Rover messages on COM3
    bool enableRTCMRover();

    // Turn on all the enabled RTCM Base messages on COM3
    bool enableRTCMBase();

    uint8_t getActiveNmeaMessageCount();

    // Given the name of an NMEA message, return the array number
    uint8_t getNmeaMessageNumberByName(const char *msgName);

    // Given the name of an RTCM message, return the array number
    uint8_t getRtcmMessageNumberByName(const char *msgName);

    // Return true if the GPGGA message is active
    bool isGgaActive();

    // Given a sub type (ie "RTCM", "NMEA") present menu showing messages with this subtype
    // Controls the messages that get broadcast over Bluetooth and logged (if enabled)
    void menuMessagesSubtype(int *localMessageRate, const char *messageType);

    // Set the minimum satellite signal level for navigation.
    bool setMinCnoRadio(uint8_t cnoValue);

  public:
    // Constructor
    GNSS_LG290P() : GNSS()
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

    bool disableSurveyIn(bool saveAndReset);

    void enableGgaForNtrip();

    // Enable RTCM 1230. This is the GLONASS bias sentence and is transmitted
    // even if there is no GPS fix. We use it to test serial output.
    // Outputs:
    //   Returns true if successfully started and false upon failure
    bool enableRTCMTest();

    bool enterConfigMode(unsigned long waitForSemaphoreTimeout_millis);

    bool exitConfigMode();

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

    // Returns 0 - Unknown, 1 - Rover, 2 - Base
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

    uint8_t getSurveyInMode();

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

    // If LBand is being used, ignore any RTCM that may come in from the GNSS
    void rtcmOnGnssDisable();

    // If L-Band is available, but encrypted, allow RTCM through other sources (radio, ESP-Now) to GNSS receiver
    void rtcmOnGnssEnable();

    uint16_t rtcmRead(uint8_t *rtcmBuffer, int rtcmBytesToRead);

    // Save the current configuration
    // Outputs:
    //   Returns true when the configuration was saved and false upon failure
    bool saveConfiguration();

    // Set the baud rate on the GNSS port that interfaces between the ESP32 and the GNSS
    // This just sets the GNSS side
    // Used during Bluetooth testing
    // Inputs:
    //   baudRate: The desired baudrate
    bool setBaudrate(uint32_t baudRate);

    // Enable all the valid constellations and bands for this platform
    bool setConstellations();

    // Enable / disable corrections protocol(s) on the Radio External port
    // Always update if force is true. Otherwise, only update if enable has changed state
    bool setCorrRadioExtPort(bool enable, bool force);

    bool setDataBaudRate(uint32_t baud);

    // Set the elevation in degrees
    // Inputs:
    //   elevationDegrees: The elevation value in degrees
    bool setElevation(uint8_t elevationDegrees);

    // Enable all the valid messages for this platform
    bool setMessages(int maxRetries);

    // Enable all the valid messages for this platform over the USB port
    bool setMessagesUsb(int maxRetries);

    // Set the dynamic model to use for RTK
    // Inputs:
    //   modelNumber: Number of the model to use, provided by radio library
    bool setModel(uint8_t modelNumber);

    bool setRadioBaudRate(uint32_t baud);

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

    // Reset the survey-in operation
    // Outputs:
    //   Returns true if the survey-in operation was reset successfully
    //   and false upon failure
    bool surveyInReset();

    // Start the survey-in operation
    // Outputs:
    //   Return true if successful and false upon failure
    bool surveyInStart();

    // If we have received serial data from the LG290P outside of the library (ie, from processUart1Message task)
    // we can pass data back into the LG290P library to allow it to update its own variables
    void lg290pUpdate(uint8_t *incomingBuffer, int bufferLength);

    // Return the baud rate of UART2, connected to the ESP32 UART1
    uint32_t getCommBaudRate();

    // Poll routine to update the GNSS state
    void update();
};

#endif // COMPILE_LG290P
#endif // __GNSS_LG290P_H__
