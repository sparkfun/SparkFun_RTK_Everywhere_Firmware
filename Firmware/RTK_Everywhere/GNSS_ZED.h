/*------------------------------------------------------------------------------
GNSS_ZED.h

  Declarations and definitions for the GNSS_ZED implementation
------------------------------------------------------------------------------*/

#ifndef __GNSS_ZED_H__
#define __GNSS_ZED_H__

#include <SparkFun_u-blox_GNSS_v3.h> //http://librarymanager/All#SparkFun_u-blox_GNSS_v3

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

    SFE_UBLOX_GNSS_SUPER * _zed = nullptr; // Don't instantiate until we know what gnssPlatform we're on

  public:

    // If we have decryption keys, configure module
    // Note: don't check online.lband_neo here. We could be using ip corrections
    void applyPointPerfectKeys();

    // Set RTCM for base mode to defaults (1005/1074/1084/1094/1124 1Hz & 1230 0.1Hz)
    void baseRtcmDefault();

    // Reset to Low Bandwidth Link (1074/1084/1094/1124 0.5Hz & 1005/1230 0.1Hz)
    virtual void baseRtcmLowDataRate();

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
    bool configureRadio();

    // Configure the Rover
    // Outputs:
    //   Returns true if successfully configured and false upon failure
    bool configureRover();

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

    const char * getId();

    // Get the latitude value
    // Outputs:
    //   Returns the latitude value or zero if not online
    double getLatitude();

    // Query GNSS for current leap seconds
    uint8_t getLeapSeconds();

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

    const char * getRtcmDefaultString();

    const char * getRtcmLowDataRateString();

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

    bool isBlocking();

    // Date is confirmed once we have GNSS fix
    bool isConfirmedDate();

    // Date is confirmed once we have GNSS fix
    bool isConfirmedTime();

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

    // If L-Band is available, but encrypted, allow RTCM through other sources (radio, ESP-Now) to GNSS receiver
    void rtcmOnGnssDisable();

    // If LBand is being used, ignore any RTCM that may come in from the GNSS
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

    bool setDataBaudRate(uint32_t baud);

    // Set the elevation in degrees
    // Inputs:
    //   elevationDegrees: The elevation value in degrees
    bool setElevation(uint8_t elevationDegrees);

    // Given a unique string, find first and last records containing that string in message array
    void setMessageOffsets(const ubxMsg *localMessage, const char *messageType, int &startOfBlock, int &endOfBlock);

    // Given the name of a message, find it, and set the rate
    bool setMessageRateByName(const char *msgName, uint8_t msgRate);

    // Enable all the valid messages for this platform
    bool setMessages(int maxRetries);

    // Enable all the valid messages for this platform over the USB port
    bool setMessagesUsb(int maxRetries);

    // Set the minimum satellite signal level for navigation.
    bool setMinCnoRadio (uint8_t cnoValue);

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

    // Callback to save the high precision data
    // Inputs:
    //   ubxDataStruct: Address of an UBX_NAV_HPPOSLLH_data_t structure
    //                  containing the high precision position data
    void storeHPdataRadio(UBX_NAV_HPPOSLLH_data_t *ubxDataStruct);

    // Callback to save the PVT data
    void storePVTdataRadio(UBX_NAV_PVT_data_t *ubxDataStruct);

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

#endif  // __GNSS_ZED_H__
