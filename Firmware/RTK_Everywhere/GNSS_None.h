/*------------------------------------------------------------------------------
GNSS_None.h

  Declarations and definitions for the empty GNSS layer
------------------------------------------------------------------------------*/

#ifndef __GNSS_None_H__
#define __GNSS_None_H__

class GNSS_None : public GNSS
{
  protected:
    // Set the minimum satellite signal level for navigation.
    bool setMinCno(uint8_t cnoValue)
    {
        return false;
    }

  public:
    // Constructor
    GNSS_None() : GNSS()
    {
    }

    // If we have decryption keys, configure module
    // Note: don't check online.lband_neo here. We could be using ip corrections
    void applyPointPerfectKeys()
    {
    }

    // Set RTCM for base mode to defaults (1005/1074/1084/1094/1124 1Hz & 1230 0.1Hz)
    void baseRtcmDefault()
    {
    }

    // Reset to Low Bandwidth Link (1074/1084/1094/1124 0.5Hz & 1005/1230 0.1Hz)
    void baseRtcmLowDataRate()
    {
    }

    // Check if a given baud rate is supported by this module
    bool baudIsAllowed(uint32_t baudRate)
    {
        return false;
    }
    uint32_t baudGetMinimum()
    {
        return 0;
    }
    uint32_t baudGetMaximum()
    {
        return 0;
    }

    // Connect to GNSS and identify particulars
    void begin()
    {
    }

    // Setup TM2 time stamp input as need
    // Outputs:
    //   Returns true when an external event occurs and false if no event
    bool beginExternalEvent()
    {
        return false;
    }

    bool checkNMEARates()
    {
        return false;
    }

    bool checkPPPRates()
    {
        return false;
    }

    // Setup the general configuration of the GNSS
    // Not Rover or Base specific (ie, baud rates)
    // Outputs:
    //   Returns true if successfully configured and false upon failure
    bool configure()
    {
        return false;
    }

    // Configure the Base
    // Outputs:
    //   Returns true if successfully configured and false upon failure
    bool configureBase()
    {
        return false;
    }

    // Configure specific aspects of the receiver for NTP mode
    bool configureNtpMode()
    {
        return false;
    }

    // Configure the Rover
    // Outputs:
    //   Returns true if successfully configured and false upon failure
    bool configureRover()
    {
        return false;
    }

    // Responds with the messages supported on this platform
    // Inputs:
    //   returnText: String to receive message names
    // Returns message names in the returnText string
    void createMessageList(String &returnText)
    {
    }

    // Responds with the RTCM/Base messages supported on this platform
    // Inputs:
    //   returnText: String to receive message names
    // Returns message names in the returnText string
    void createMessageListBase(String &returnText)
    {
    }

    // Responds with the messages supported on this platform
    // Inputs:
    //   returnText: String to receive message names
    // Returns message names in the returnText string
    void getMessageList(String &returnText)
    {
    }

    // Responds with the RTCM/Base messages supported on this platform
    // Inputs:
    //   returnText: String to receive message names
    // Returns message names in the returnText string
    void getMessageListBase(String &returnText)
    {
    }

    void debuggingDisable()
    {
    }

    void debuggingEnable()
    {
    }

    void enableGgaForNtrip()
    {
    }

    // Enable RTCM 1230. This is the GLONASS bias sentence and is transmitted
    // even if there is no GPS fix. We use it to test serial output.
    // Outputs:
    //   Returns true if successfully started and false upon failure
    bool enableRTCMTest()
    {
        return false;
    }

    // Restore the GNSS to the factory settings
    void factoryReset()
    {
    }

    uint16_t fileBufferAvailable()
    {
        return 0;
    }

    uint16_t fileBufferExtractData(uint8_t *fileBuffer, int fileBytesToRead)
    {
        return 0;
    }

    // Start the base using fixed coordinates
    // Outputs:
    //   Returns true if successfully started and false upon failure
    bool fixedBaseStart()
    {
        return false;
    }

    bool fixRateIsAllowed(uint32_t fixRateMs)
    {
        return false;
    }

    //Return min/max rate in ms
    uint32_t fixRateGetMinimumMs()
    {
        return 0;
    }

    uint32_t fixRateGetMaximumMs()
    {
        return 0;
    }

    // Return the number of active/enabled messages
    uint8_t getActiveMessageCount()
    {
        return 0;
    }

    // Return the number of active/enabled RTCM messages
    uint8_t getActiveRtcmMessageCount()
    {
        return 0;
    }

    // Get the altitude
    // Outputs:
    //   Returns the altitude in meters or zero if the GNSS is offline
    double getAltitude()
    {
        return _altitude;
    }

    // Returns the carrier solution or zero if not online
    uint8_t getCarrierSolution()
    {
        return 0;
    }

    uint32_t getDataBaudRate()
    {
        return 0;
    }

    // Returns the day number or zero if not online
    uint8_t getDay()
    {
        return _day;
    }

    // Return the number of milliseconds since GNSS data was last updated
    uint16_t getFixAgeMilliseconds()
    {
        return 0;
    }

    // Returns the fix type or zero if not online
    uint8_t getFixType()
    {
        return _fixType;
    }

    // Returns the hours of 24 hour clock or zero if not online
    uint8_t getHour()
    {
        return _hour;
    }

    // Get the horizontal position accuracy
    // Outputs:
    //   Returns the horizontal position accuracy or zero if offline
    float getHorizontalAccuracy()
    {
        return _horizontalAccuracy;
    }

    const char *getId()
    {
        return "No compiled";
    }

    // Get the latitude value
    // Outputs:
    //   Returns the latitude value or zero if not online
    double getLatitude()
    {
        return _latitude;
    }

    // Query GNSS for current leap seconds
    uint8_t getLeapSeconds()
    {
        return _leapSeconds;
    }

    // Return the type of logging that matches the enabled messages - drives the logging icon
    uint8_t getLoggingType()
    {
        return 0;
    }

    // Get the longitude value
    // Outputs:
    //   Returns the longitude value or zero if not online
    double getLongitude()
    {
        return _longitude;
    }

    // Returns two digits of milliseconds or zero if not online
    uint8_t getMillisecond()
    {
        return _millisecond;
    }

    // Returns minutes or zero if not online
    uint8_t getMinute()
    {
        return _minute;
    }

    // Returns month number or zero if not online
    uint8_t getMonth()
    {
        return _month;
    }

    // Returns nanoseconds or zero if not online
    uint32_t getNanosecond()
    {
        return 0;
    }

    uint32_t getRadioBaudRate()
    {
        return 0;
    }

    // Returns the seconds between solutions
    double getRateS()
    {
        return 1.0;
    }

    const char *getRtcmDefaultString()
    {
        return "Not compiled";
    }

    const char *getRtcmLowDataRateString()
    {
        return "Not compiled";
    }

    // Returns the number of satellites in view or zero if offline
    uint8_t getSatellitesInView()
    {
        return _satellitesInView;
    }

    // Returns seconds or zero if not online
    uint8_t getSecond()
    {
        return _second;
    }

    // Get the survey-in mean accuracy
    // Outputs:
    //   Returns the mean accuracy or zero (0)
    float getSurveyInMeanAccuracy()
    {
        return _horizontalAccuracy;
    }

    // Return the number of seconds the survey-in process has been running
    int getSurveyInObservationTime()
    {
        return 0;
    }

    float getSurveyInStartingAccuracy()
    {
        return 0;
    }

    // Returns timing accuracy or zero if not online
    uint32_t getTimeAccuracy()
    {
        return 0;
    }

    // Returns full year, ie 2023, not 23.
    uint16_t getYear()
    {
        return _year;
    }

    bool isBlocking()
    {
        return false;
    }

    // Date is confirmed once we have GNSS fix
    bool isConfirmedDate()
    {
        return false;
    }

    // Date is confirmed once we have GNSS fix
    bool isConfirmedTime()
    {
        return false;
    }

    // Returns true if data is arriving on the Radio Ext port
    bool isCorrRadioExtPortActive()
    {
        return false;
    }

    // Return true if GNSS receiver has a higher quality DGPS fix than 3D
    bool isDgpsFixed()
    {
        return false;
    }

    // Some functions (L-Band area frequency determination) merely need
    // to know if we have a valid fix, not what type of fix
    // This function checks to see if the given platform has reached
    // sufficient fix type to be considered valid
    bool isFixed()
    {
        return false;
    }

    // Used in tpISR() for time pulse synchronization
    bool isFullyResolved()
    {
        return false;
    }

    bool isPppConverged()
    {
        return false;
    }

    bool isPppConverging()
    {
        return false;
    }

    // Some functions (L-Band area frequency determination) merely need
    // to know if we have an RTK Fix.  This function checks to see if the
    // given platform has reached sufficient fix type to be considered valid
    bool isRTKFix()
    {
        return false;
    }

    // Some functions (L-Band area frequency determination) merely need
    // to know if we have an RTK Float.  This function checks to see if
    // the given platform has reached sufficient fix type to be considered
    // valid
    bool isRTKFloat()
    {
        return false;
    }

    // Determine if the survey-in operation is complete
    // Outputs:
    //   Returns true if the survey-in operation is complete and false
    //   if the operation is still running
    bool isSurveyInComplete()
    {
        return false;
    }

    // Date will be valid if the RTC is reporting (regardless of GNSS fix)
    bool isValidDate()
    {
        return false;
    }

    // Time will be valid if the RTC is reporting (regardless of GNSS fix)
    bool isValidTime()
    {
        return false;
    }

    // Controls the constellations that are used to generate a fix and logged
    void menuConstellations()
    {
    }

    void menuMessageBaseRtcm()
    {
    }

    // Control the messages that get broadcast over Bluetooth and logged (if enabled)
    void menuMessages()
    {
    }

    void modifyGst(char *nmeaSentence, uint16_t *sentenceLength)
    {
    }

    // Print the module type and firmware version
    void printModuleInfo()
    {
    }

    // Send correction data to the GNSS
    // Inputs:
    //   dataToSend: Address of a buffer containing the data
    //   dataLength: The number of valid data bytes in the buffer
    // Outputs:
    //   Returns the number of correction data bytes written
    int pushRawData(uint8_t *dataToSend, int dataLength)
    {
        return dataLength;
    }

    // Hardware or software reset the GNSS receiver
    bool reset()
    {
        return true;
    }

    uint16_t rtcmBufferAvailable()
    {
        return 0;
    }

    // If LBand is being used, ignore any RTCM that may come in from the GNSS
    void rtcmOnGnssDisable()
    {
    }

    // If L-Band is available, but encrypted, allow RTCM through other sources (radio, ESP-NOW) to GNSS receiver
    void rtcmOnGnssEnable()
    {
    }

    uint16_t rtcmRead(uint8_t *rtcmBuffer, int rtcmBytesToRead)
    {
        return 0;
    }

    // Save the current configuration
    // Outputs:
    //   Returns true when the configuration was saved and false upon failure
    bool saveConfiguration()
    {
        return false;
    }

    bool setBaudRate(uint8_t uartNumber, uint32_t baudRate)
    {
        return false;
    }

    bool setBaudRateData(uint32_t baud)
    {
        return true;
    }

    bool setBaudRateComm(uint32_t baud)
    {
        return true;
    }

    bool setBaudRateRadio(uint32_t baud)
    {
        return true;
    }

    // Enable all the valid constellations and bands for this platform
    bool setConstellations()
    {
        return true;
    }

    // Enable / disable corrections protocol(s) on the Radio External port
    bool setCorrRadioExtPort(bool enable, bool force)
    {
        return true;
    }

    // Set the elevation in degrees
    // Inputs:
    //   elevationDegrees: The elevation value in degrees
    bool setElevation(uint8_t elevationDegrees)
    {
        return true;
    }

    // Control whether HAS E6 is used in location fixes or not
    bool setHighAccuracyService(bool enableGalileoHas)
    {
        return true;
    }

    // Configure NMEA messages
    bool setMessagesNMEA()
    {
        return true;
    }

    // Configure RTCM Base messages
    bool setMessagesRTCMBase()
    {
        return true;
    }

    // Configure RTCM Base messages
    bool setMessagesRTCMRover()
    {
        return true;
    }

    // Set the dynamic model to use for RTK
    // Inputs:
    //   modelNumber: Number of the model to use, provided by radio library
    bool setModel(uint8_t modelNumber)
    {
        return true;
    }

    bool setMultipathMitigation(bool enableMultipathMitigation)
    {
        return true;
    }

    // Given the name of a message, find it, and set the rate
    bool setNmeaMessageRateByName(const char *msgName, uint8_t msgRate)
    {
        return true;
    }

    // Configure the Pulse-per-second pin based on user settings
    bool setPPS()
    {
        return false;
    }

    // Specify the interval between solutions
    // Inputs:
    //   secondsBetweenSolutions: Number of seconds between solutions
    // Outputs:
    //   Returns true if the rate was successfully set and false upon
    //   failure
    bool setRate(double secondsBetweenSolutions)
    {
        return true;
    }

    bool setTalkerGNGGA()
    {
        return true;
    }

    bool setTilt()
    {
        return true;
    }

    bool standby()
    {
        return true;
    }

    // Reset the survey-in operation
    // Outputs:
    //   Returns true if the survey-in operation was reset successfully
    //   and false upon failure
    bool surveyInReset()
    {
        return true;
    }

    // Start the survey-in operation
    // Outputs:
    //   Return true if successful and false upon failure
    bool surveyInStart()
    {
        return true;
    }

    // Poll routine to update the GNSS state
    void update()
    {
    }
};

#endif // __GNSS_None_H__
