/*------------------------------------------------------------------------------
GNSS.h

  Declarations and definitions for the GNSS layer
------------------------------------------------------------------------------*/

#ifndef __GNSS_H__
#define __GNSS_H__

class GNSS
{
  protected:
    float _altitude;           // Altitude in meters
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

    bool _corrRadioExtPortEnabled = false;

    unsigned long _autoBaseStartTimer; // Tracks how long the base auto / averaging mode has been running

    // Setup the general configuration of the GNSS
    // Not Rover or Base specific (ie, baud rates)
    // Outputs:
    //   Returns true if successfully configured and false upon failure
    virtual bool configureGNSS();

    // Set the minimum satellite signal level for navigation.
    virtual bool setMinCnoRadio(uint8_t cnoValue);

  public:
    // Constructor
    GNSS() : _leapSeconds(18), _pvtArrivalMillis(0), _pvtUpdated(0), _satellitesInView(0)
    {
    }

    // If we have decryption keys, configure module
    // Note: don't check online.lband_neo here. We could be using ip corrections
    virtual void applyPointPerfectKeys();

    // Set RTCM for base mode to defaults (1005/1074/1084/1094/1124 1Hz & 1230 0.1Hz)
    virtual void baseRtcmDefault();

    // Reset to Low Bandwidth Link (1074/1084/1094/1124 0.5Hz & 1005/1230 0.1Hz)
    virtual void baseRtcmLowDataRate();

    // Connect to GNSS and identify particulars
    virtual void begin();

    // Setup TM2 time stamp input as need
    // Outputs:
    //   Returns true when an external event occurs and false if no event
    virtual bool beginExternalEvent();

    // Setup the timepulse output on the PPS pin for external triggering
    // Outputs
    //   Returns true if the pin was successfully setup and false upon
    //   failure
    virtual bool beginPPS();

    virtual bool checkNMEARates();

    virtual bool checkPPPRates();

    // Setup the general configuration of the GNSS
    // Not Rover or Base specific (ie, baud rates)
    // Outputs:
    //   Returns true if successfully configured and false upon failure
    bool configure();

    // Configure the Base
    // Outputs:
    //   Returns true if successfully configured and false upon failure
    virtual bool configureBase();

    // Configure specific aspects of the receiver for NTP mode
    virtual bool configureNtpMode();

    // Configure the Rover
    // Outputs:
    //   Returns true if successfully configured and false upon failure
    virtual bool configureRover();

    virtual void debuggingDisable();

    virtual void debuggingEnable();

    virtual void enableGgaForNtrip();

    // Enable RTCM 1230. This is the GLONASS bias sentence and is transmitted
    // even if there is no GPS fix. We use it to test serial output.
    // Outputs:
    //   Returns true if successfully started and false upon failure
    virtual bool enableRTCMTest();

    // Restore the GNSS to the factory settings
    virtual void factoryReset();

    virtual uint16_t fileBufferAvailable();

    virtual uint16_t fileBufferExtractData(uint8_t *fileBuffer, int fileBytesToRead);

    // Start the base using fixed coordinates
    // Outputs:
    //   Returns true if successfully started and false upon failure
    virtual bool fixedBaseStart();

    // Return the number of active/enabled messages
    virtual uint8_t getActiveMessageCount();

    // Return the number of active/enabled RTCM messages
    virtual uint8_t getActiveRtcmMessageCount();

    // Get the altitude
    // Outputs:
    //   Returns the altitude in meters or zero if the GNSS is offline
    virtual double getAltitude();

    // Returns the carrier solution or zero if not online
    virtual uint8_t getCarrierSolution();

    virtual uint32_t getDataBaudRate();

    // Returns the day number or zero if not online
    virtual uint8_t getDay();

    // Return the number of milliseconds since GNSS data was last updated
    virtual uint16_t getFixAgeMilliseconds();

    // Returns the fix type or zero if not online
    virtual uint8_t getFixType();

    // Returns the hours of 24 hour clock or zero if not online
    virtual uint8_t getHour();

    // Get the horizontal position accuracy
    // Outputs:
    //   Returns the horizontal position accuracy or zero if offline
    virtual float getHorizontalAccuracy();

    virtual const char *getId();

    // Get the latitude value
    // Outputs:
    //   Returns the latitude value or zero if not online
    virtual double getLatitude();

    // Query GNSS for current leap seconds
    virtual uint8_t getLeapSeconds();

    // Return the type of logging that matches the enabled messages - drives the logging icon
    virtual uint8_t getLoggingType();

    // Get the longitude value
    // Outputs:
    //   Returns the longitude value or zero if not online
    virtual double getLongitude();

    // Returns two digits of milliseconds or zero if not online
    virtual uint8_t getMillisecond();

    // Get the minimum satellite signal level for navigation.
    uint8_t getMinCno();

    // Returns minutes or zero if not online
    virtual uint8_t getMinute();

    // Returns month number or zero if not online
    virtual uint8_t getMonth();

    // Returns nanoseconds or zero if not online
    virtual uint32_t getNanosecond();

    virtual uint32_t getRadioBaudRate();

    // Returns the seconds between solutions
    virtual double getRateS();

    virtual const char *getRtcmDefaultString();

    virtual const char *getRtcmLowDataRateString();

    // Returns the number of satellites in view or zero if offline
    virtual uint8_t getSatellitesInView();

    // Returns seconds or zero if not online
    virtual uint8_t getSecond();

    // Get the survey-in mean accuracy
    // Outputs:
    //   Returns the mean accuracy or zero (0)
    virtual float getSurveyInMeanAccuracy();

    // Return the number of seconds the survey-in process has been running
    virtual int getSurveyInObservationTime();

    float getSurveyInStartingAccuracy();

    // Returns timing accuracy or zero if not online
    virtual uint32_t getTimeAccuracy();

    // Returns full year, ie 2023, not 23.
    virtual uint16_t getYear();

    virtual bool isBlocking();

    // Date is confirmed once we have GNSS fix
    virtual bool isConfirmedDate();

    // Date is confirmed once we have GNSS fix
    virtual bool isConfirmedTime();

    // Returns true if data is arriving on the Radio Ext port
    virtual bool isCorrRadioExtPortActive();

    // Return true if GNSS receiver has a higher quality DGPS fix than 3D
    virtual bool isDgpsFixed();

    // Some functions (L-Band area frequency determination) merely need
    // to know if we have a valid fix, not what type of fix
    // This function checks to see if the given platform has reached
    // sufficient fix type to be considered valid
    virtual bool isFixed();

    // Used in tpISR() for time pulse synchronization
    virtual bool isFullyResolved();

    virtual bool isPppConverged();

    virtual bool isPppConverging();

    // Some functions (L-Band area frequency determination) merely need
    // to know if we have an RTK Fix.  This function checks to see if the
    // given platform has reached sufficient fix type to be considered valid
    virtual bool isRTKFix();

    // Some functions (L-Band area frequency determination) merely need
    // to know if we have an RTK Float.  This function checks to see if
    // the given platform has reached sufficient fix type to be considered
    // valid
    virtual bool isRTKFloat();

    // Determine if the survey-in operation is complete
    // Outputs:
    //   Returns true if the survey-in operation is complete and false
    //   if the operation is still running
    virtual bool isSurveyInComplete();

    // Date will be valid if the RTC is reporting (regardless of GNSS fix)
    virtual bool isValidDate();

    // Time will be valid if the RTC is reporting (regardless of GNSS fix)
    virtual bool isValidTime();

    // Controls the constellations that are used to generate a fix and logged
    virtual void menuConstellations();

    virtual void menuMessageBaseRtcm();

    // Control the messages that get broadcast over Bluetooth and logged (if enabled)
    virtual void menuMessages();

    // Print the module type and firmware version
    virtual void printModuleInfo();

    // Send correction data to the GNSS
    // Inputs:
    //   dataToSend: Address of a buffer containing the data
    //   dataLength: The number of valid data bytes in the buffer
    // Outputs:
    //   Returns the number of correction data bytes written
    virtual int pushRawData(uint8_t *dataToSend, int dataLength);

    virtual uint16_t rtcmBufferAvailable();

    // If LBand is being used, ignore any RTCM that may come in from the GNSS
    virtual void rtcmOnGnssDisable();

    // If L-Band is available, but encrypted, allow RTCM through other sources (radio, ESP-Now) to GNSS receiver
    virtual void rtcmOnGnssEnable();

    virtual uint16_t rtcmRead(uint8_t *rtcmBuffer, int rtcmBytesToRead);

    // Save the current configuration
    // Outputs:
    //   Returns true when the configuration was saved and false upon failure
    virtual bool saveConfiguration();

    // Set the baud rate on the GNSS port that interfaces between the ESP32 and the GNSS
    // This just sets the GNSS side
    // Used during Bluetooth testing
    // Inputs:
    //   baudRate: The desired baudrate
    virtual bool setBaudrate(uint32_t baudRate);

    // Enable all the valid constellations and bands for this platform
    virtual bool setConstellations();

    // Enable / disable corrections protocol(s) on the Radio External port
    // Always update if force is true. Otherwise, only update if enable has changed state
    virtual bool setCorrRadioExtPort(bool enable, bool force);

    virtual bool setDataBaudRate(uint32_t baud);

    // Set the elevation in degrees
    // Inputs:
    //   elevationDegrees: The elevation value in degrees
    virtual bool setElevation(uint8_t elevationDegrees);

    // Enable all the valid messages for this platform
    virtual bool setMessages(int maxRetries);

    // Enable all the valid messages for this platform over the USB port
    virtual bool setMessagesUsb(int maxRetries);

    // Set the minimum satellite signal level for navigation.
    bool setMinCno(uint8_t cnoValue);

    // Set the dynamic model to use for RTK
    // Inputs:
    //   modelNumber: Number of the model to use, provided by radio library
    virtual bool setModel(uint8_t modelNumber);

    virtual bool setRadioBaudRate(uint32_t baud);

    // Specify the interval between solutions
    // Inputs:
    //   secondsBetweenSolutions: Number of seconds between solutions
    // Outputs:
    //   Returns true if the rate was successfully set and false upon
    //   failure
    virtual bool setRate(double secondsBetweenSolutions);

    virtual bool setTalkerGNGGA();

    // Hotstart GNSS to try to get RTK lock
    virtual bool softwareReset();

    virtual bool standby();

    // Reset the survey-in operation
    // Outputs:
    //   Returns true if the survey-in operation was reset successfully
    //   and false upon failure
    virtual bool surveyInReset();

    // Start the survey-in operation
    // Outputs:
    //   Return true if successful and false upon failure
    virtual bool surveyInStart();

    // Poll routine to update the GNSS state
    virtual void update();
};

#endif // __GNSS_H__
