/*
Soft_AP_Channels.ino

Sketch to verify channel switching during Soft AP operation
*/

#include <esp_wifi.h>
#include <Network.h>
#include <WiFi.h>

#define CHANNEL_CHANGE_MSEC     (10 * 1000)

IPAddress apDhcpFirstAddress = IPAddress("0.0.0.0");
IPAddress apDnsAddress = IPAddress("0.0.0.0");
IPAddress apGatewayAddress = IPAddress("0.0.0.0");
IPAddress apIpAddress = IPAddress("192.168.100.1");
byte apMacAddress[6];
const char * apPassword = "Password";   // WiFi network password
const char * apSsid = "Test";           // WiFi network name
IPAddress apSubnetMask = IPAddress("255.255.255.0");
bool apUserConnected;
const char * hostName = "test-AP";
uint32_t lastChannelChangeMsec;
volatile bool staConnected;
bool waitForScanComplete;
volatile bool wifiScanDone;
int wifiChannel;

bool RTK_CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC = false;

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void setup()
{
    char data;
    int length;

    // Initialize serial and wait for port to open:
    Serial.begin(115200);
    while (!Serial);  // Wait for native USB serial port to connect
    Serial.println();
    Serial.println(__FILE__);

    // Read the WiFi network name (SSID)
/*
    length = 0;
    do
    {
        Serial.println("Please enter the WiFi network name (SSID):");
        do
        {
            while (!Serial.available());
            data = Serial.read();
            if ((data == '\n') || (data == '\r'))
                break;
            apSsid[length++] = data;
        } while (1);
        apSsid[length] = 0;
    } while (!length);
    Serial.printf("AP SSID: %s\n", apSsid);
    Serial.println();

    // Read the WiFi network password
    length = 0;
    do
    {
        Serial.println("Please enter the WiFi network password:");
        do
        {
            while (!Serial.available());
            data = Serial.read();
            if ((data == '\n') || (data == '\r'))
                break;
            apPassword[length++] = data;
        } while (1);
        apPassword[length] = 0;
    } while (!length);
    Serial.printf("Password: %s\n", apPassword);
    Serial.println();
*/

    // Start the WiFi soft AP
    do
    {
        // Start WiFi in the soft AP mode
        if (WiFi.mode((wifi_mode_t)WIFI_MODE_APSTA) == false)
            reportFatalError("Failed to set WiFi AP mode!");

        // Start the soft AP protocol
        if (esp_wifi_set_protocol(WIFI_IF_AP,
                                  WIFI_PROTOCOL_11B
                                  | WIFI_PROTOCOL_11G
                                  | WIFI_PROTOCOL_11N) != ESP_OK)
            reportFatalError("Failed to set the Soft AP protocol!");

        // Start the WiFi Station protocol
        if (esp_wifi_set_protocol(WIFI_IF_STA,
                                  WIFI_PROTOCOL_11B
                                  | WIFI_PROTOCOL_11G
                                  | WIFI_PROTOCOL_11N) != ESP_OK)
            reportFatalError("Failed to set the Station protocol!");

        // Set the soft AP subnet mask, IP, gateway, DNS, and first DHCP addresses
        if (WiFi.AP.config(IPAddress(apIpAddress),
                           IPAddress(apGatewayAddress),
                           IPAddress(apSubnetMask),
                           IPAddress(apDhcpFirstAddress),
                           IPAddress(apDnsAddress)) == false)
        {
            reportFatalError("Failed to set the Soft AP IP address!");
        }

        // Set the soft AP SSID and password
        wifiChannel = 1;
        if (WiFi.AP.create(apSsid, apPassword, wifiChannel) == false)
            reportFatalError("Failed to set the Soft AP SSID and password!");
        Serial.printf("Soft AP using channel %d\r\n", wifiChannel);

        // Get the soft AP MAC address
        WiFi.AP.macAddress(apMacAddress);

        // Display the host name
        Serial.printf("Host name: %s\r\n", hostName);

        // Set the soft AP host name
        if (WiFi.AP.setHostname(hostName) == false)
            reportFatalError("Failed to set the Soft AP host name!");
    } while (0);

    // Handle the network events
    Network.onEvent(networkEvent);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void loop()
{
    uint32_t currentMsec;
    static uint32_t lastChannelChangeMsec;
    int nextWifiChannel;

    // Wait for the WiFi scan to complete
    if (wifiScanDone)
    {
        waitForScanComplete = false;

        // Stop use of WiFi
        if (WiFi.mode((wifi_mode_t)WIFI_MODE_AP) == false)
            reportFatalError("Failed to stop WiFi STA mode!");
    }

    if (waitForScanComplete)
        return;

    // Attempt to change the channel
    currentMsec = millis();
    if ((currentMsec - lastChannelChangeMsec) >= CHANNEL_CHANGE_MSEC)
    {
        lastChannelChangeMsec = currentMsec;
        nextWifiChannel = 0;
        switch(wifiChannel)
        {
        case 0:
            nextWifiChannel = 1;
            break;

        case 1:
            nextWifiChannel = 6;
            break;

        case 4:
            nextWifiChannel = 6;
            break;

        case 6:
            nextWifiChannel = 11;
            break;

        case 11:
            nextWifiChannel = 1;

            // Attempt to scan WiFi for access points
            if (esp_wifi_scan_start(nullptr, false) != ESP_OK)
                Serial.printf("Failed to start WiFi scan!\r\n");
            else
            {
                Serial.printf("Started WiFi scan\r\n");
                waitForScanComplete = true;
            }
            break;
        }

        // Attempt to set the next WiFi channel
        if (esp_wifi_set_channel(wifiChannel, WIFI_SECOND_CHAN_NONE) != ESP_OK)
        {
            if (wifiChannel == 0)
                Serial.printf("ERROR: Failed to set the WiFi channel %d\r\n",
                               nextWifiChannel);
            else
                Serial.printf("ERROR: Failed to set the WiFi channel %d, stuck on channel %d\r\n",
                              nextWifiChannel, wifiChannel);
            if (waitForScanComplete || staConnected)
                Serial.printf("Documented restriction in esp_wifi_set_channel Attention 2\r\n");
            if (apUserConnected)
                Serial.printf("Documented restriction in esp_wifi_set_channel Attention 3\r\n");
        }
        else
        {
            if (wifiChannel == 0)
                Serial.printf("Soft AP switching from unknown channel to channel %d\r\n",
                               nextWifiChannel);
            else
                Serial.printf("Soft AP switching from channel %d to channel %d\r\n",
                               wifiChannel, nextWifiChannel);
            wifiChannel = nextWifiChannel;
        }
    }

    // Restart WiFi station
    if (wifiScanDone)
    {
        wifiScanDone = false;

        // Restart WiFi station
        if (WiFi.mode((wifi_mode_t)WIFI_MODE_APSTA) == false)
            reportFatalError("Failed to restart WiFi STA mode!");

        // Start the WiFi Station protocol
        if (esp_wifi_set_protocol(WIFI_IF_STA,
                                  WIFI_PROTOCOL_11B
                                  | WIFI_PROTOCOL_11G
                                  | WIFI_PROTOCOL_11N) != ESP_OK)
            reportFatalError("Failed to set the Station protocol!");

        // Set the soft AP SSID and password
        if (WiFi.AP.create(apSsid, apPassword, nextWifiChannel) == false)
            reportFatalError("Failed to set the Soft AP SSID and password!");
        else
        {
            wifiChannel = nextWifiChannel;
            Serial.printf("Soft AP using channel %d\r\n", wifiChannel);
        }
    }
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// Process network events
void networkEvent(arduino_event_id_t event, arduino_event_info_t info)
{
    int index;

    Serial.printf("event: %d (%s)\r\n", event, Network.eventName(event));

    // Process the event
    switch (event)
    {
    // WiFi
    case ARDUINO_EVENT_WIFI_OFF:
        break;

    case ARDUINO_EVENT_WIFI_READY:
        break;

    case ARDUINO_EVENT_WIFI_SCAN_DONE:
        wifiScanDone = true;
        break;

    case ARDUINO_EVENT_WIFI_STA_START:
        break;

    case ARDUINO_EVENT_WIFI_STA_STOP:
        break;

    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
        staConnected = true;
        break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        staConnected = false;
        break;

    case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE:
        break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP6:
        break;

    case ARDUINO_EVENT_WIFI_STA_LOST_IP:
        break;

    case ARDUINO_EVENT_WIFI_AP_START:
        break;

    case ARDUINO_EVENT_WIFI_AP_STOP:
        break;

    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
        apUserConnected = true;
        break;

    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
        apUserConnected = false;
        break;

    case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
        break;

    case ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED:
        break;

    case ARDUINO_EVENT_WIFI_AP_GOT_IP6:
        break;
    }
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void displayIpAddress(IPAddress ip) {

  // Display the IP address
  Serial.println(ip.toString().c_str());
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void displayWiFiIpAddress() {

  // Display the IP address
  Serial.print ("IP Address: ");
  IPAddress ip = WiFi.localIP();
  displayIpAddress(ip);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void displayWiFiMacAddress() {
  // Display the MAC address
  byte mac[6];
  WiFi.macAddress(mac);
  Serial.print("MAC address: ");
  Serial.print(mac[5], HEX);
  Serial.print(":");
  Serial.print(mac[4], HEX);
  Serial.print(":");
  Serial.print(mac[3], HEX);
  Serial.print(":");
  Serial.print(mac[2], HEX);
  Serial.print(":");
  Serial.print(mac[1], HEX);
  Serial.print(":");
  Serial.println(mac[0], HEX);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void displayWiFiNetwork() {

  // Display the SSID
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // Display the receive signal strength
  long rssi = WiFi.RSSI();
  Serial.print("Signal strength (RSSI):");
  Serial.println(rssi);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
void displayWiFiSubnetMask() {

  // Display the subnet mask
  Serial.print ("Subnet Mask: ");
  IPAddress ip = WiFi.subnetMask();
  displayIpAddress(ip);
}

//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// Print the error message every 15 seconds
void reportFatalError(const char *errorMsg)
{
    // Empty the FIFO of any incoming data
    while (Serial.available())
        Serial.read();
    while (1)
    {
        // Allow carriage return to reset the system
        if (Serial.available() && (Serial.read() == '\r'))
        {
            Serial.println("System reset");
            Serial.flush();
            ESP.restart();
        }

        // Periodically display the halted message
        Serial.print("HALTED: ");
        Serial.print(errorMsg);
        Serial.println();
        sleep(15);
    }
}
