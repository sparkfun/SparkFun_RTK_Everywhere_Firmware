double tiltLatitude = 0;
double tiltLongitude = 0;
double tiltAltitude = 0;
float tiltPositionAccuracy = 0;
uint32_t tiltStatus = 0;

void tiltUpdate()
{
    if (tiltSupported == true)
    {
        if (settings.enableTiltCompensation == true && online.imu == true)
        {
            // Poll IMU at 5Hz to get latest sensor readings
            if (millis() - lastTiltCheck > 200) // Update our internal variables at 5Hz
            {
                lastTiltCheck = millis();

                if (tiltSensor->updateNavi() == IM19_RESULT_OK)
                {
                    tiltLatitude = tiltSensor->getNaviLatitude();
                    tiltLongitude = tiltSensor->getNaviLongitude();
                    tiltAltitude = tiltSensor->getNaviAltitude();
                    tiltPositionAccuracy = tiltSensor->getNaviPositionAccuracy();
                    tiltStatus = tiltSensor->getNaviStatus();
                }
            }
        }
        else if (settings.enableTiltCompensation == false && online.imu == true)
        {
            tiltStop(); // If user has disabled the device, shut it down
        }
        else if (online.imu == false)
        {
            // Try multiple times to configure IM19
            for (int x = 0; x < 3; x++)
            {
                tiltBegin(); // Start IMU
                if (online.imu == true)
                    break;
                log_d("Tilt sensor failed to configure. Trying again.");
            }

            if (online.imu == false) // If we failed to begin, disable future attempts
                tiltSupported = false;
        }
    }
}

// Start communication with the IM19 IMU
void tiltBegin()
{
    if (tiltSupported == false)
        return;

    tiltSensor = new IM19();

    SerialForTilt = new HardwareSerial(2); // Use UART2 on the ESP32 to receive IMU corrections

    const int pin_UART2_RX = 16;
    const int pin_UART2_TX = 17;

    SerialForTilt->setRxBufferSize(1024 * 1);

    // We must start the serial port before handing it over to the library
    SerialForTilt->begin(115200, SERIAL_8N1, pin_UART2_RX, pin_UART2_TX);

    tiltSensor->enableDebugging(); // Print all debug to Serial

    if (tiltSensor->begin(*SerialForTilt) == false) // Give the serial port over to the library
    {
        log_d("Tilt sensor failed to respond.");
        return;
    }
    systemPrintln("Tilt sensor online.");

    tiltSensor->setNaviFreshLimitMs(200); // Mark data as expired after 200ms. We need 5Hz.

    // tiltSensor->factoryReset();

    // while (tiltSensor->isConnected() == false)
    // {
    //     systemPrintln("waiting for sensor to reset");
    //     delay(1000);
    // }

    bool result = true;

    // Use serial port 3 as the serial port for communication with GNSS
    result &= tiltSensor->sendCommand("GNSS_PORT=PHYSICAL_UART3");

    // Use serial port 1 as the main output and combines navigation data output
    result &= tiltSensor->sendCommand("NAVI_OUTPUT=UART1,ON");

    // Enable GNSS monitoring - Used for debug
    // result &= tiltSensor->sendCommand("GNSS_OUTPUT=UART1,ON");

    // Set the distance of the IMU from center line - x:6.78mm y:10.73mm z:19.25mm
    if (productVariant == RTK_TORCH)
        result &= tiltSensor->sendCommand("LEVER_ARM=-0.00678,-0.01073,-0.01925");

    // Set the overall length of the GNSS setup: rod length + internal length 96.45 + antenna POC 19.25
    char clubVector[strlen("CLUB_VECTOR=0,0,1.916") + 1];
    float arp = 0.0;
    if (productVariant == RTK_TORCH)
        arp = 0.116; // In m

    snprintf(clubVector, sizeof(clubVector), "CLUB_VECTOR=0,0,%0.3f", settings.tiltPoleLength + arp);
    result &= tiltSensor->sendCommand("CLUB_VECTOR=0,0,1.916");

    // Configure interface type. This allows IM19 to receive Unicore style binary messages
    result &= tiltSensor->sendCommand("GNSS_CARD=UNICORE");

    // Configure as tilt measurement mode
    result &= tiltSensor->sendCommand("WORK_MODE=152");

    // Complete installation angle estimation in tilt measurement applications
    result &= tiltSensor->sendCommand("AUTO_FIX=ENABLE");

    // Trigger IMU on PPS rising edge from UM980
    result &= tiltSensor->sendCommand("SET_PPS_EDGE=RISING");

    // Nathan: AT+HIGH_RATE=[ENABLE | DISABLE] - try to slow down NAVI
    result &= tiltSensor->sendCommand("HIGH_RATE=DISABLE");

    // Nathan: AT+MAG_AUTO_SAVE=ENABLE
    result &= tiltSensor->sendCommand("MAG_AUTO_SAVE=ENABLE");

    if (result == true)
    {
        if (tiltSensor->saveConfiguration() == true)
        {
            log_d("IM19 configuration saved");
            online.imu = true;
        }
    }
}

void tiltStop()
{
    // Free the resources
    delete tiltSensor;
    tiltSensor = nullptr;

    delete SerialForTilt;
    SerialForTilt = nullptr;

    online.imu = false;
}
