/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
OTA_Bootstrap.ino

  Production line bootstrap for SparkFun / SparkPNT RTK Everywhere products.

  Load this small firmware, open the serial port at 115200, and press u.
  The bootstrap identifies the product and its GNSS, LoRa and IMU, joins the production
  Wi-Fi network, downloads the product release firmware listed in the manifest
  (RTK-Everywhere-Variants.csv) and updates every subsystem. The ESP32 is updated last
  with the RTK Everywhere firmware, which replaces the bootstrap on the next boot.

  The update code is copied from RTK_Everywhere. OTA_Bootstrap_Notes.md lists where every
  piece came from and how to keep it in step with the firmware.
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

bool RTK_CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC = false; // Needed because of local BT TLS patch

#include "Bootstrap.h"

Preferences preferences;
String wifiSsid;
String wifiPassword;

void setup()
{
    Serial.begin(115200);
    delay(250);

    systemPrintln();
    systemPrintln(otaEqualSigns);
    systemPrintf("RTK Everywhere OTA Bootstrap v%s\r\n", BOOTSTRAP_VERSION);
    systemPrintln(otaEqualSigns);

    identifyBoard();
    if (productVariant == RTK_UNKNOWN)
    {
        systemPrintf("Device MAC: %02X%02X%02X%02X%02X%02X\r\n", btMACAddress[0], btMACAddress[1], btMACAddress[2],
                     btMACAddress[3], btMACAddress[4], btMACAddress[5]);
        reportFatalError("Product unknown. Unable to update this device.");
    }

    beginBoard();
    detectSubsystems();

    // Wi-Fi credentials: production defaults unless changed from the menu
    preferences.begin("otaBootstrap", false);
    wifiSsid = preferences.getString("ssid", DEFAULT_WIFI_SSID);
    wifiPassword = preferences.getString("password", DEFAULT_WIFI_PASSWORD);

    printMenu();
}

void loop()
{
    if (Serial.available() == 0)
    {
        delay(10);
        return;
    }

    char incoming = Serial.read();

    // Ignore line endings: a line-buffered terminal sends them after every key
    if ((incoming == '\r') || (incoming == '\n'))
        return;

    if ((incoming == 'u') || (incoming == 'U'))
    {
        runUpdate();
        printMenu();
    }
    else if (incoming == 's')
    {
        systemPrint("Enter the Wi-Fi SSID: ");
        String value = readLine();
        if (value.length())
        {
            wifiSsid = value;
            preferences.putString("ssid", wifiSsid);
        }
        printMenu();
    }
    else if (incoming == 'p')
    {
        systemPrint("Enter the Wi-Fi password: ");
        String value = readLine();
        if (value.length())
        {
            wifiPassword = value;
            preferences.putString("password", wifiPassword);
        }
        printMenu();
    }
    else if (incoming == 'w')
    {
        wifiSsid = DEFAULT_WIFI_SSID;
        wifiPassword = DEFAULT_WIFI_PASSWORD;
        preferences.remove("ssid");
        preferences.remove("password");
        printMenu();
    }
    else if (incoming == 'd')
    {
        settings.debugFirmwareUpdate = !settings.debugFirmwareUpdate;
        printMenu();
    }
    else if (incoming == 'r')
    {
        systemPrintln("Resetting the ESP32...");
        Serial.flush();
        delay(100);
        ESP.restart();
    }
}

void printMenu()
{
    systemPrintln();
    systemPrintf("Product: %s (%s), serial number %s\r\n", productName, platformPrefix, serialNumber);
    for (int subsystem = 0; subsystem < OTA_SUBSYSTEM_MAX; subsystem++)
    {
        const char *chip = subsystemChipName(subsystem);
        if (chip[0])
            systemPrintf("    %-5s %s%s\r\n", otaSubsystem[subsystem], chip,
                         otaGetSubsystemInfo(subsystem) ? "" : " (no update available)");
    }
    systemPrintln();
    systemPrintln("u) Update all subsystems");
    systemPrintf("s) Wi-Fi SSID: %s\r\n", wifiSsid.c_str());
    systemPrintf("p) Wi-Fi password: %s\r\n", wifiPassword.c_str());
    systemPrintln("w) Restore the default Wi-Fi network");
    systemPrintf("d) Debug output: %s\r\n", settings.debugFirmwareUpdate ? "on" : "off");
    systemPrintln("r) Reset the ESP32");
    systemPrint("> ");
}

// Read a line from the serial port, with echo and backspace
String readLine()
{
    String input;
    while (1)
    {
        if (Serial.available() == 0)
        {
            delay(10);
            continue;
        }
        char c = Serial.read();
        if ((c == '\r') || (c == '\n'))
        {
            if (input.length() == 0)
                continue; // Leftover line ending from the menu key
            break;
        }
        if ((c == '\b') || (c == 0x7F))
        {
            if (input.length())
            {
                input.remove(input.length() - 1);
                systemPrint("\b \b");
            }
            continue;
        }
        input += c;
        Serial.write(c);
    }
    systemPrintln();
    return input;
}

bool wifiConnect()
{
    systemPrintf("Connecting to Wi-Fi %s...\r\n", wifiSsid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());

    uint32_t startMsec = millis();
    while (WiFi.status() != WL_CONNECTED)
    {
        if ((millis() - startMsec) > WIFI_CONNECT_TIMEOUT_MSEC)
        {
            systemPrintln("ERROR: Failed to connect to Wi-Fi. Check the SSID and password (s and p).");
            WiFi.disconnect(true);
            return false;
        }
        delay(250);
    }
    systemPrintf("Wi-Fi connected, IP address %s\r\n", WiFi.localIP().toString().c_str());
    return true;
}

void runUpdate()
{
    // Throw away the rest of the key press (\r\n)
    delay(50);
    while (Serial.available())
        Serial.read();

    if (wifiConnect() == false)
    {
        beep(1, 1000, 0);
        return;
    }

    bool success = updateAllSubsystems();
    WiFi.disconnect(true);

    if (success)
    {
        beep(2, 100, 100);
        systemPrintln("Rebooting into the RTK Everywhere firmware. Goodbye!");
        Serial.flush();
        delay(1000);
        ESP.restart();
    }

    beep(1, 1000, 0);
    systemPrintln("Fix the problem above, then press u to try again.");
}
