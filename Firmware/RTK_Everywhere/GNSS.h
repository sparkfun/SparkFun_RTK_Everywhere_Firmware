/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
GNSS.h

  Declarations and definitions for the GNSS layer
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#ifndef __GNSS_H__
#define __GNSS_H__

// GNSS receiver type detected in Facet FP
typedef enum
{
    GNSS_RECEIVER_LG290P = 0,
    GNSS_RECEIVER_MOSAIC_X5,
    GNSS_RECEIVER_X20P,
    GNSS_RECEIVER_UM980,
    GNSS_RECEIVER_F9P,
    // Add new values above this line
    GNSS_RECEIVER_UNKNOWN,
} gnssReceiverType_e;

class GNSS
{
  protected:
    double _altitude;          // Altitude in meters
    double _geoidalSeparation; // Geoidal separation in meters
    float _horizontalAccuracy; // Horizontal position accuracy in meters
    double _latitude;          // Latitude in degrees
    double _longitude;         // Longitude in degrees

    uint8_t _day;   // Day number
    uint8_t _month; // Month number
    uint16_t _year;
    uint8_t _hour; // Hours for 24 hour clock
    uint8_t _minute;
    uint8_t _second;
    uint8_t _leapSeconds;
    uint16_t _millisecond; // Limited to first two digits
    uint32_t _nanosecond;

    uint8_t _satellitesInView;
    uint8_t _fixType;
    uint8_t _carrierSolution;

    bool _validDate; // True when date is valid
    bool _validTime; // True when time is valid
    bool _confirmedDate;
    bool _confirmedTime;
    bool _fullyResolved;
    uint32_t _tAcc;

    unsigned long _pvtArrivalMillis;
    bool _pvtUpdated;

    unsigned long _autoBaseStartTimer; // Tracks how long the base auto / averaging mode has been running

  public:
    // Constructor
    GNSS() : _leapSeconds(18), _pvtArrivalMillis(0), _pvtUpdated(0), _satellitesInView(0)
    {
    }

    // If we have decryption keys, configure module
    virtual void applyPointPerfectKeys() = 0;

    // Set RTCM for base mode to defaults (1005/1074/1084/1094/1124 1Hz & 1230 0.1Hz)
    virtual void baseRtcmDefault() = 0;

    // Reset to Low Bandwidth Link (1074/1084/1094/1124 0.5Hz & 1005/1230 0.1Hz)
    virtual void baseRtcmLowDataRate() = 0;

    // Check if a given baud rate is supported by this module
    virtual bool baudIsAllowed(uint32_t baudRate) = 0;
    virtual uint32_t baudGetMinimum() = 0;
    virtual uint32_t baudGetMaximum() = 0;

    // Connect to GNSS and identify particulars
    virtual void begin() = 0;

    // Setup TM2 time stamp input as need
    // Outputs:
    //   Returns true when an external event occurs and false if no event
    virtual bool beginExternalEvent() = 0;

    virtual bool checkNMEARates() = 0;

    virtual bool checkPPPRates() = 0;

    // On platforms that support / need it (i.e. mosaic-X5), refresh the
    // COM port by sending an escape sequence or similar to make the
    // GNSS snap out of it...
    // Outputs:
    //   Returns true if successful and false upon failure
    virtual bool comPortRefresh();

    // Setup the general configuration of the GNSS
    // Not Rover or Base specific (ie, baud rates)
    // Outputs:
    //   Returns true if successfully configured and false upon failure
    virtual bool configure() = 0;

    // Configure the Base
    // Outputs:
    //   Returns true if successfully configured and false upon failure
    virtual bool configureBase() = 0;

    // Configure specific aspects of the receiver for NTP mode
    virtual bool configureNtpMode() = 0;

    // Configure the Rover
    // Outputs:
    //   Returns true if successfully configured and false upon failure
    virtual bool configureRover() = 0;

    // Configure the RTCM 1033 Antenna Description
    // Outputs:
    //   Returns true if successfully configured and false upon failure
    virtual bool configureRtcm1033() = 0;

    // Responds with the messages supported on this platform
    // Inputs:
    //   returnText: String to receive message names
    // Returns message names in the returnText string
    virtual void createMessageList(String &returnText) = 0;

    // Responds with the RTCM/Base messages supported on this platform
    // Inputs:
    //   returnText: String to receive message names
    // Returns message names in the returnText string
    virtual void createMessageListBase(String &returnText) = 0;

    virtual void debuggingDisable() = 0;

    virtual void debuggingEnable() = 0;

    // Restore the GNSS to the factory settings
    virtual void factoryReset() = 0;

    virtual uint16_t fileBufferAvailable() = 0;

    virtual uint16_t fileBufferExtractData(uint8_t *fileBuffer, int fileBytesToRead) = 0;

    // Start the base using fixed coordinates
    // Outputs:
    //   Returns true if successfully started and false upon failure
    virtual bool fixedBaseStart() = 0;

    virtual bool fixRateIsAllowed(uint32_t fixRateMs) = 0;

    // Return min/max rate in ms
    virtual uint32_t fixRateGetMinimumMs() = 0;

    virtual uint32_t fixRateGetMaximumMs() = 0;

    // Return the number of active/enabled messages
    virtual uint8_t getActiveMessageCount() = 0;

    // Return the number of active/enabled RTCM messages
    virtual uint8_t getActiveRtcmMessageCount() = 0;

    // Get the altitude
    // Outputs:
    //   Returns the altitude in meters or zero if the GNSS is offline
    virtual double getAltitude() = 0;

    // Returns the carrier solution or zero if not online
    virtual uint8_t getCarrierSolution() = 0;

    virtual uint32_t getDataBaudRate() = 0;

    // Returns the day number or zero if not online
    virtual uint8_t getDay() = 0;

    // Return the number of milliseconds since GNSS data was last updated
    virtual uint16_t getFixAgeMilliseconds() = 0;

    // Returns the fix type or zero if not online
    virtual uint8_t getFixType() = 0;

    // Returns the geoidal separation
    virtual double getGeoidalSeparation() = 0;

    // Returns the hours of 24 hour clock or zero if not online
    virtual uint8_t getHour() = 0;

    // Get the horizontal position accuracy
    // Outputs:
    //   Returns the horizontal position accuracy or zero if offline
    virtual float getHorizontalAccuracy() = 0;

    virtual const char *getId() = 0;

    // Get the latitude value
    // Outputs:
    //   Returns the latitude value or zero if not online
    virtual double getLatitude() = 0;

    // Query GNSS for current leap seconds
    virtual uint8_t getLeapSeconds() = 0;

    // Return the type of logging that matches the enabled messages - drives the logging icon
    virtual uint8_t getLoggingType() = 0;

    // Get the longitude value
    // Outputs:
    //   Returns the longitude value or zero if not online
    virtual double getLongitude() = 0;

    // Returns two digits of milliseconds or zero if not online
    virtual uint8_t getMillisecond() = 0;

    // Returns minutes or zero if not online
    virtual uint8_t getMinute() = 0;

    // Returns the current mode: Base/Rover/etc
    virtual uint8_t getMode() = 0;

    // Returns month number or zero if not online
    virtual uint8_t getMonth() = 0;

    // Returns nanoseconds or zero if not online
    virtual uint32_t getNanosecond() = 0;

    virtual uint32_t getRadioBaudRate() = 0;

    // Returns the seconds between solutions
    virtual double getRateS() = 0;

    virtual const char *getRtcmDefaultString() = 0;

    virtual const char *getRtcmLowDataRateString() = 0;

    // Returns the number of satellites in view or zero if offline
    virtual uint8_t getSatellitesInView() = 0;

    // Returns seconds or zero if not online
    virtual uint8_t getSecond() = 0;

    // Get the survey-in mean accuracy
    // Outputs:
    //   Returns the mean accuracy or zero (0)
    virtual float getSurveyInMeanAccuracy() = 0;

    // Return the number of seconds the survey-in process has been running
    virtual int getSurveyInObservationTime() = 0;

    // Returns timing accuracy or zero if not online
    virtual uint32_t getTimeAccuracy() = 0;

    // Sets the four version number parts
    virtual bool getVersion(uint16_t &major, uint8_t &minor, uint8_t &patch, uint8_t &revision) = 0;

    // Returns full year, ie 2023, not 23.
    virtual uint16_t getYear() = 0;

    // Helper functions for the current mode as read from the GNSS receiver
    // Not to be confused with inRoverMode() and inBaseMode() used in States.ino
    virtual bool gnssInBaseFixedMode() = 0;
    virtual bool gnssInBaseSurveyInMode() = 0;
    virtual bool gnssInRoverMode() = 0;

    // Indicate if there are any additional settings specific to this GNSS
    // This governs setGnssSpecificConfiguration() and menuGnssSpecificConfiguration()
    virtual bool hasGnssSpecificConfiguration();

    // Antenna Short / Open detection
    virtual bool isAntennaShorted();
    virtual bool isAntennaOpen();

    virtual bool isBlocking() = 0;

    // Date is confirmed once we have GNSS fix
    virtual bool isConfirmedDate() = 0;

    // Date is confirmed once we have GNSS fix
    virtual bool isConfirmedTime() = 0;

    // Return true if GNSS receiver has a higher quality DGPS fix than 3D
    virtual bool isDgpsFixed() = 0;

    // Returns 0 if corrections can not be arriving on the selected port
    // Returns 1 if corrections are assumed to be arriving on the selected port
    // Returns 2 if corrections truly are arriving on the selected port
    virtual int isExternalCorrectionActive(uint8_t port) = 0;

    // Some functions merely need to know if we have an RTK Float.
    // This function checks to see if the given platform has reached sufficient
    // fix type to be considered valid.
    virtual bool isFixed() = 0;

    // Used in tpISR() for time pulse synchronization
    virtual bool isFullyResolved() = 0;

    virtual bool isPppConverged() = 0;

    virtual bool isPppConverging() = 0;

    // Some functions merely need to know if we have an RTK Float.
    // This function checks to see if the given platform has reached sufficient
    // fix type to be considered valid.
    virtual bool isRTKFix() = 0;

    // Some functions merely need to know if we have an RTK Float.
    // This function checks to see if the given platform has reached sufficient
    // fix type to be considered valid.
    virtual bool isRTKFloat() = 0;

    // Determine if the survey-in operation is complete
    // Outputs:
    //   Returns true if the survey-in operation is complete and false
    //   if the operation is still running
    virtual bool isSurveyInComplete() = 0;

    // Date will be valid if the RTC is reporting (regardless of GNSS fix)
    virtual bool isValidDate() = 0;

    // Time will be valid if the RTC is reporting (regardless of GNSS fix)
    virtual bool isValidTime() = 0;

    // Controls the constellations that are used to generate a fix and logged
    virtual void menuConstellations() = 0;

    // Configure any settings specific to this GNSS
    virtual void menuGnssSpecificConfiguration();

    virtual void menuMessageBaseRtcm() = 0;

    // Control the messages that get broadcast over Bluetooth and logged (if enabled)
    virtual void menuMessages() = 0;

    // Print the module type and firmware version
    virtual void printModuleInfo() = 0;

    // Send correction data to the GNSS
    // Inputs:
    //   dataToSend: Address of a buffer containing the data
    //   dataLength: The number of valid data bytes in the buffer
    // Outputs:
    //   Returns the number of correction data bytes written
    virtual int pushRawData(uint8_t *dataToSend, int dataLength) = 0;

    // Hardware or software reset the GNSS receiver
    virtual bool reset() = 0;

    virtual uint16_t rtcmBufferAvailable() = 0;

    // If LBand is being used, ignore any RTCM that may come in from the GNSS
    virtual void rtcmOnGnssDisable() = 0;

    // If L-Band is available, but encrypted, allow RTCM through other sources (radio, ESP-NOW) to GNSS receiver
    virtual void rtcmOnGnssEnable() = 0;

    virtual uint16_t rtcmRead(uint8_t *rtcmBuffer, int rtcmBytesToRead) = 0;

    virtual bool setBaudRate(uint8_t uartNumber, uint32_t baudRate) = 0;

    virtual bool setBaudRateComm(uint32_t baud) = 0;

    virtual bool setBaudRateData(uint32_t baud) = 0;

    virtual bool setBaudRateRadio(uint32_t baud) = 0;

    // Save the current configuration
    // Outputs:
    //   Returns true when the configuration was saved and false upon failure
    virtual bool saveConfiguration() = 0;

    // Enable all the valid constellations and bands for this platform
    virtual bool setConstellations() = 0;

    // Enable / disable external corrections protocol(s) on the chosen port
    // Always update if force is true. Otherwise, only update if enable has changed state
    virtual bool setExternalCorrections(uint8_t port, bool enable, bool force, const char *debug = nullptr) = 0;

    // Set the elevation in degrees
    // Inputs:
    //   elevationDegrees: The elevation value in degrees
    virtual bool setElevation(uint8_t elevationDegrees) = 0;

    // Configure any additional settings specific to this GNSS
    virtual bool setGnssSpecificConfiguration();

    virtual bool setPppService() = 0;

    // Configure any logging settings - currently mosaic-X5 specific
    virtual bool setLogging() = 0;

    // Enable/disable messages according to the NMEA array
    virtual bool setMessagesNMEA() = 0;

    // Enable/disable messages according to the RTCM Base array
    virtual bool setMessagesRTCMBase() = 0;

    // Enable/disable messages according to the NMEA array
    virtual bool setMessagesRTCMRover() = 0;

    // Turn on all the enabled Extra/Other messages
    virtual bool setMessagesOther() = 0;

    // Set the minimum satellite signal level for navigation.
    virtual bool setMinCN0(uint8_t cnoValue) = 0;

    // Set the dynamic model to use for RTK
    // Inputs:
    //   modelNumber: Number of the model to use, provided by radio library
    virtual bool setModel(uint8_t modelNumber) = 0;

    virtual bool setMultipathMitigation(bool enableMultipathMitigation) = 0;

    virtual bool setNmeaMessageRateByName(const char *msgName, uint8_t msgRate) = 0;

    // Configure the Pulse-per-second pin based on user settings
    virtual bool setPPS() = 0;

    // Specify the interval between solutions
    // Inputs:
    //   secondsBetweenSolutions: Number of seconds between solutions
    // Outputs:
    //   Returns true if the rate was successfully set and false upon
    //   failure
    virtual bool setRate(double secondsBetweenSolutions) = 0;

    // Enable/disable any output needed for tilt compensation
    virtual bool setTilt() = 0;

    virtual bool standby() = 0;

    // Antenna Short / Open detection
    virtual bool supportsAntennaShortOpen();

    // Reset the survey-in operation
    // Outputs:
    //   Returns true if the survey-in operation was reset successfully
    //   and false upon failure
    virtual bool surveyInReset() = 0;

    // Start the survey-in operation
    // Outputs:
    //   Return true if successful and false upon failure
    virtual bool surveyInStart() = 0;

    // Poll routine to update the GNSS state
    virtual void update() = 0;
};

// Request the GNSS configuration for a single-value GNSS setting following a set command
bool gnssCmdUpdateSetting(const char *settingName, void *settingData, int settingType);

// Update the constellations following a set command
bool gnssCmdUpdateConstellations(const char *settingName, void *settingData, int settingType);

// Update the message rates following a set command
bool gnssCmdUpdateMessageRates(const char *settingName, void *settingData, int settingType);

// Request the fixed base coordinates be re-applied following a set command
bool gnssCmdUpdateFixedBase(const char *settingName, void *settingData, int settingType);

// Fixed base coordinate tracking - used by fixedBaseStart() to detect changed coordinates
uint32_t gnssFixedBaseFingerprint();
bool gnssFixedBaseCoordinatesApplied();
void gnssFixedBaseMarkApplied();
void gnssFixedBaseSettingsChanged();

// Restore the GNSS to the factory settings
void gnssFactoryReset();

// Determine if the GNSS receiver is present
typedef bool (*GNSS_PRESENT)();

// Create the GNSS class instance
typedef void (*GNSS_NEW_CLASS)();

bool commandSettingChanged(const void *settingValue, size_t settingSize);
void commandSendExecuteListResponse(const char *settingName, const char *settingType, const char *settingValue);

// List available settings, their type in CSV, and value
typedef bool (*GNSS_COMMAND_LIST)(RTK_Settings_Types type, int settingsIndex, bool inCommands, int qualifier,
                                  char *settingName, size_t settingNameSize, char *settingValue);

// Add types to a JSON array
typedef void (*GNSS_COMMAND_TYPE_JSON)(JsonArray &command_types);

// Create string for settings
typedef bool (*GNSS_CREATE_STRING)(RTK_Settings_Types type, int settingsIndex, char *newSettings);

// Return setting value as a string
typedef bool (*GNSS_GET_SETTING_VALUE)(RTK_Settings_Types type, const char *suffix, int settingsIndex, int qualifier,
                                       char *settingValueStr);

// Update a setting value
typedef bool (*GNSS_NEW_SETTING_VALUE)(struct Settings * tempSettings, RTK_Settings_Types type, const char *suffix, int qualifier, double d);

// Write settings to a file
typedef bool (*GNSS_SETTING_TO_FILE)(char * line, size_t lineSize, RTK_Settings_Types type, int settingsIndex);

typedef struct _GNSS_SUPPORT_ROUTINES
{
    const char *name;
    const char *gnssModelIdentifier;
    gnssReceiverType_e _receiver;
    GNSS_PRESENT _present;
    int8_t _presentPriority; // -1 : no priority; 0 : highest priority; 1 : next highest
    GNSS_NEW_CLASS _newClass;
    GNSS_COMMAND_LIST _commandList;
    GNSS_COMMAND_TYPE_JSON _commandTypeJson;
    GNSS_CREATE_STRING _createString;
    GNSS_GET_SETTING_VALUE _getSettingValue;
    GNSS_NEW_SETTING_VALUE _newSettingValue;
    GNSS_SETTING_TO_FILE _settingToFile;
} GNSS_SUPPORT_ROUTINES;

#endif // __GNSS_H__
