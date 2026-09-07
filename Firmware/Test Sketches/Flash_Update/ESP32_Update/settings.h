/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
settings.h
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#ifndef __SETTINGS_H__
#define __SETTINGS_H__

// System can enter a variety of states
// See statemachine diagram at:
// https://lucid.app/lucidchart/53519501-9fa5-4352-aa40-673f88ca0c9b/edit?invitationId=inv_ebd4b988-513d-4169-93fd-c291851108f8
typedef enum
{
    STATE_ROVER_NOT_STARTED = 0,        //  0
    STATE_ROVER_CONFIG_WAIT,            //  1
    STATE_ROVER_NO_FIX,                 //  2
    STATE_ROVER_FIX,                    //  3
    STATE_ROVER_RTK_FLOAT,              //  4
    STATE_ROVER_RTK_FIX,                //  5

    STATE_BASE_CASTER_NOT_STARTED,      //  6, Set override flag
    STATE_BASE_ASSIST_NOT_STARTED,      //  7
    STATE_BASE_NOT_STARTED,             //  8
    STATE_BASE_CONFIG_WAIT,             //  9
    STATE_BASE_TEMP_SETTLE,             // 10, User has indicated base, but current pos accuracy is too low
    STATE_BASE_TEMP_SURVEY_STARTED,     // 11
    STATE_BASE_TEMP_TRANSMITTING,       // 12
    STATE_BASE_FIXED_NOT_STARTED,       // 13
    STATE_BASE_FIXED_TRANSMITTING,      // 14

    STATE_DISPLAY_SETUP,                // 15
    STATE_WEB_CONFIG_NOT_STARTED,       // 16
    STATE_WEB_CONFIG_WAIT_FOR_NETWORK,  // 17
    STATE_WEB_CONFIG,                   // 18
    STATE_PROFILE,                      // 19

    STATE_KEYS_REQUESTED,               // 20

    STATE_ESPNOW_PAIRING_NOT_STARTED,   // 21
    STATE_ESPNOW_PAIRING,               // 22

    STATE_NTPSERVER_NOT_STARTED,        // 23
    STATE_NTPSERVER_NO_SYNC,            // 24
    STATE_NTPSERVER_SYNC,               // 25

    STATE_SHUTDOWN,                     // 26

    STATE_NOT_SET,                      // 27, Must be last on list
} SystemState;
volatile SystemState systemState = STATE_NOT_SET;

// Branding support
typedef enum {
    BRAND_SPARKFUN = 0,
    BRAND_SPARKPNT,
    // Add new brands above this line
    BRAND_NUM
} RTKBrands_e;

#define DEFAULT_BRAND           BRAND_SPARKPNT

typedef struct
{
    const RTKBrands_e brand;
    const char name[9];
    const uint8_t logoWidth;
    const uint8_t logoHeight;
    const uint8_t * const logoPointer;
} RTKBrandAttribute;

RTKBrandAttribute RTKBrandAttributes[RTKBrands_e::BRAND_NUM] = {
    { BRAND_SPARKFUN, "SparkFun", logoSparkFun_Width, logoSparkFun_Height, logoSparkFun },
    { BRAND_SPARKPNT, "SparkPNT", logoSparkPNT_Width, logoSparkPNT_Height, logoSparkPNT },
};
const int RTKBrandAttributesEntries = sizeof(RTKBrandAttributes) / sizeof(RTKBrandAttributes[0]);

// Product Variant - Do NOT reorder and do NOT remove unused values!!!
// The label on the product lists the device ID which consists of the
// Bluetooth address followed by two digits of the ProductVariant below.
// This same ID value is used for the whitelists.  Skipped values represent
// unreleased or new products.
typedef enum
{
    RTK_ALL = -1,
    RTK_EVK = 0, // 0x00
    // RTK_FACET_V2 = 1, // 0x01 - No L-Band
    RTK_FACET_MOSAIC = 2, // 0x02
    RTK_TORCH = 3, // 0x03
    // RTK_FACET_V2_LBAND = 4, // 0x04
    RTK_POSTCARD = 5, // 0x05
    RTK_FACET_FP = 6, // 0x06
    RTK_TORCH_X2 = 7, // 0x07
    // Add new values above this line
    RTK_UNKNOWN
} ProductVariant;
ProductVariant productVariant = RTK_UNKNOWN;

// allVariants - Do NOT remove, MUST match the contents of ProductVariant
// without the RTK_ALL value!!!
static const ProductVariant allVariants[] = { RTK_EVK, RTK_FACET_MOSAIC, RTK_TORCH, RTK_POSTCARD, RTK_FACET_FP, RTK_TORCH_X2, RTK_UNKNOWN};
#define productVariantCount (sizeof(allVariants) / sizeof(allVariants[0]))

typedef enum
{
    RTK_HOUSING_EVK = 0,    // EVK with SPK6615H
    RTK_HOUSING_FACET,      // Facet mosaic-X5
    RTK_HOUSING_FP,         // Facet FP - tilt possible
    RTK_HOUSING_POSTCARD,   // Postcard with SPK-6E helical
    RTK_HOUSING_TORCH,      // Torch - with tilt
    RTK_HOUSING_TX2,        // Torch X2 - no tilt
    // Add new housing variants above this line
    RTK_HOUSING_MAX_NONE,
} ProductVariantHousing;

typedef struct
{
    const ProductVariantHousing housing;
    const float antennaPhaseCenter_mm;
    const bool tiltPossible;
    const char *leverArm;
    const char *installAngle;
    const char *gnssCard;
} productHousingProperties;

const productHousingProperties productHousingPropertiesTable[] =
{
    {RTK_HOUSING_EVK,       42.0,   false,  "", "", ""}, // Default to NGS calibrated SPK6615H APC, average of L1/L2
    {RTK_HOUSING_FACET,     68.5,   false,  "", "", ""}, // Default to L-Band element APC, average of L1/L2
    {RTK_HOUSING_FP,        58.3,   true,   "LEVER_ARM2=0.03391,0.00272,0.02370", "INSTALL_ANGLE=0,180,0", "GNSS_CARD=OEM"}, // NGS calibrated average of L1/L2
    {RTK_HOUSING_POSTCARD,  37.5,   false,  "", "", ""}, // APC of SPK-6E helical L1/L2/L5 antenna
    {RTK_HOUSING_TORCH,     129.0,  true,   "LEVER_ARM=-0.00678,-0.01073,-0.0314", "", "GNSS_CARD=UNICORE"}, // Default to Torch helical APC, NGS calibrated average of L1/L2
    {RTK_HOUSING_TX2,       129.0,  false,  "", "", ""}, // Default to Torch helical APC, NGS calibrated average of L1/L2
    {RTK_HOUSING_MAX_NONE,  0.0,    false,  "", "", ""},
};
const int productHousingEntries = sizeof(productHousingPropertiesTable) / sizeof(productHousingPropertiesTable[0]);

typedef enum
{
    TILT_NOT_PRESENT = 0,
    TILT_DISABLED,
    TILT_OFFLINE,
    TILT_STARTED,
    TILT_INITIALIZED,
    TILT_CORRECTING,
    TILT_REQUEST_STOP,
} TiltState;

// Product Properties Table
// ========================
// name is used to create the BT broadcast deviceName
// It is formed from: the brand, the name, the 6-character serial number (two MAC octets plus productVariant)
// Limit name to 12 chars max - to keep the total broadcast length to 28 chars or less
// E.g.: "SparkPNT Facet v2 LB-ABCD04" is 27 chars
// SparkPNT Facet FP-ABCD06
// SparkPNT Torch X2-ABCD07
// name is also used to create the title for the menus etc.. Include "RTK" if rtkPrefix is true
// displayName is displayed on the OLED. Keep short - and adjust to match the width of the OLED
// filePrefix is the settings and log file prefix
// platformProvision is assembled into the PointPerfect ZTP request (largely deprecated)
// If rtkPrefix is true, "RTK" is included before the name: on the OLED, in the serial menus, etc.
// productPlanUID is a 16 character unique identifier for the product plan associated with the MFi accessory
// defaultSystemState is the default system state - if the actual previous state is unknown
// platformRegistration is the web page for product registration
typedef struct
{
    ProductVariant productVariant;
    const float r1; // First resistor value in K Ohms, zero = no resistor
    const float r2; // Second resistor value in K OHms, zero = no resistor
    const float tolerancePercentage;  // Resistor tolerance
    const RTKBrands_e brand;
    const ProductVariantHousing housing;
    const char *name;
    const char *displayName;
    const char *filePrefix;
    const char *platformProvision;
    const bool rtkPrefix;
    const char *productPlanUID;
    const SystemState defaultSystemState;
    const char *platformRegistration;
} productProperties;

const productProperties productPropertiesTable[] =
{
    //productVariant    r1    r2    Tol %   brand           housing                 name            displayName filePrefix              platformProvision   rtkPfx  productPlanUID      defaultSystemState          platformRegistration
    //==============    ==    ==    =====   =====           =======                 ====            =========== ==========              =================   ======  ==============      ==================          ====================
    { RTK_EVK,          1,    10,   17.5,   BRAND_SPARKFUN, RTK_HOUSING_EVK,        "EVK",          "EVK",      "SFE_EVK",              "EVK",              true,   "0000000000000000", STATE_ROVER_NOT_STARTED,    "https://www.sparkfun.com/rtk_evk_registration" },
    { RTK_FACET_MOSAIC, 1,    4.7,  10,     BRAND_SPARKPNT, RTK_HOUSING_FACET,      "Facet X5",     "Facet X5", "SFE_Facet_mosaic",     "Facet mosaicX5",   true,   "0000000000000000", STATE_ROVER_NOT_STARTED,    "https://www.sparkfun.com/rtk_facet_mosaic_registration" },
    { RTK_TORCH,        0,    0,    0,      BRAND_SPARKPNT, RTK_HOUSING_TORCH,      "Torch",        "Torch",    "SFE_Torch",            "Torch",            true,   "0000000000000000", STATE_ROVER_NOT_STARTED,    "https://www.sparkfun.com/rtk_torch_registration" },
    { RTK_POSTCARD,     3.3,  10,   8.5,    BRAND_SPARKFUN, RTK_HOUSING_POSTCARD,   "Postcard",     "Postcard", "SFE_Postcard",         "Postcard",         true,   "e9e877bb278140f0", STATE_ROVER_NOT_STARTED,    "https://www.sparkfun.com/rtk_postcard_registration" },
    { RTK_FACET_FP,     10,   20,   8.5,    BRAND_SPARKPNT, RTK_HOUSING_FP,         "FP",           "FP",       "SFE_FP",               "FP",               false,  "e9e877bb278140f0", STATE_ROVER_NOT_STARTED,    "https://www.sparkfun.com/rtk_facet_fp_registration" },
    { RTK_TORCH_X2,     8.2,  3.3,  8.5,    BRAND_SPARKPNT, RTK_HOUSING_TX2,        "TX2",          "TX2",      "SFE_TX2",              "TX2",              false,  "3407c7ca3d6b4984", STATE_ROVER_NOT_STARTED,    "https://www.sparkfun.com/tx2_registration" },
    { RTK_UNKNOWN,      0,    0,    0,      DEFAULT_BRAND,  RTK_HOUSING_MAX_NONE,   "Unknown",      "Unknown",  "SFE_Unknown",          "Unknown",          true,   "0000000000000000", STATE_ROVER_NOT_STARTED,    "Unknown" },
};
const int productPropertiesEntries = sizeof(productPropertiesTable) / sizeof(productPropertiesTable[0]);

#define productVariantProperties getProductPropertiesFromVariant(productVariant)
#define variantHousingProperties getProductHousingPropertiesFromVariant(productVariant)

// OLED Displays
typedef enum
{
    DISPLAY_64x48,
    DISPLAY_128x64,
    DISPLAY_184x88, // Facet FP e-Paper (SSD168x via I2C-SPI bridge)
    // Add new displays above this line
    DISPLAY_MAX_NONE // This represents the maximum numbers of display and also "no display"
} DisplayType;

// Data port mux (RTK Facet) can enter one of four different connections
typedef enum
{
    MUX_GNSS_UART = 0,
    MUX_PPS_EVENTTRIGGER,
    MUX_I2C_WT,
    MUX_ADC_DAC,
} muxConnectionType_e;

// This is all the settings that can be set on RTK Product. It's recorded to NVM and the config file.
// Avoid reordering. The order of these variables is mimicked in NVM/record/parse/create/update/get
struct Settings
{
    bool debugFirmwareUpdate = false;
    int uartReceiveBufferSize = 1024 * 2; // This buffer is filled automatically as the UART receives characters
    bool enableHeapReport = true; // Turn on to display free heap
    int16_t serialTimeoutGNSS = 1; // In ms - used during serialGNSS->begin. Number of ms to pass of no data before
                                   // hardware serial reports data available.
} settings;

// Indicate which peripherals are present on a given platform
struct struct_present
{
    bool psram_2mb = false;
    bool psram_4mb = false;

    bool cellular_lara = false;
    bool ethernet_ws5500 = false;
    bool radio_lora = false;
    bool gnss_to_uart = false;
    bool gnss_to_uart2 = false;

    bool gnss_um980 = false;
    bool gnss_zedf9p = false;
    bool gnss_mosaicX5 = false; // L-Band is implicit
    bool gnss_lg290p = false;
    bool gnss_zedx20p = false;

    // A GNSS TP interrupt - for accurate clock setting
    // The GNSS UBX PVT message is sent ahead of the top-of-second
    // The rising edge of the TP signal indicates the true top-of-second
    bool timePulseInterrupt = false;

    bool imu_im19 = false;
    bool imu_zedf9r = false;

    bool microSd = false;
    bool mosaicMicroSd = false;
    bool microSdCardDetectLow = false; // Card detect low = SD in place
    bool microSdCardDetectHigh = false; // Card detect high = SD in place
    bool microSdCardDetectGpioExpanderHigh = false; // Card detect on GPIO5, high = SD in place

    bool i2c0BusSpeed_400 = false;
    bool i2c1BusSpeed_400 = false;
    bool i2c1 = false;
    bool display_i2c0 = false;
    bool display_i2c1 = false;
    DisplayType display_type = DISPLAY_MAX_NONE;
    bool displayInverted = false;

    bool fuelgauge_max17048 = false;
    bool fuelgauge_bq40z50 = false;
    bool charger_mp2762a = false;
    bool charger_mcp73833 = false;

    bool beeper = false;
    bool encryption_atecc608a = false;
    bool portDataMux = false;
    bool peripheralPowerControl = false;
    bool laraPowerControl = false;
    bool antennaShortOpen = false;

    bool button_mode = false; // EVK has a dedicated Mode button but no power
    bool button_function = false; // Facet FP has both power and Function buttons
    bool button_powerHigh = false; // Button is pressed when high
    bool button_powerLow = false; // Button is pressed when low
    bool gpioExpanderButtons = false; // Available on Portability shield
    bool fastPowerOff = false;
    bool invertedFastPowerOff = false; // Needed for Facet mosaic v11

    bool needsExternalPpl = false;

    bool pppCapable = false; // Device has the capability to do PPP corrections, currently B2b or E6 HAS
    bool multipathMitigation = false; // UM980 has MPM, other platforms do not
    bool minCN0 = false; // ZED, mosaic, UM980 have minCN0. LG290P does on version >= v5.
    bool minElevation = false; // ZED, mosaic, UM980 have minElevation. LG290P does on versions >= v5.
    bool dynamicModel = false; // ZED, mosaic, UM980 have dynamic models. LG290P does with firmware v2.01.
    bool gpioExpanderSwitches = false; // Used on Facet FP
    bool loraDedicatedUart = false; // Platforms may have a dedicated or shared UART interface to the LoRa radio

    const char *gnssUpdatePort = ""; // "CH342 Channel A" etc.

    bool rtcm1033AntennaDescription = false; // RTCM 1033 Antenna Descriptor - supported on X5, LG290P and UM980
} present;

// Monitor which devices on the device are on or offline.
struct struct_online
{
    bool batteryCharger_mp2762a = false;
    bool batteryFuelGauge = false;
    bool bluetooth = false;
    bool powerButton = false;
    bool functionButton = false;
    bool display = false;
    bool ethernetNTPServer = false; // EthernetUDP
    bool fs = false;
    bool gnss = false;
    bool gpioExpanderButtons = false;
    bool gpioExpanderSwitches = false;
    bool httpClient = false;
    bool i2c = false;
    bool lband_gnss = false;
    bool pointPerfectKeysApplied = false;
    bool logging = false;
    bool microSD = false;
    bool mqttClient = false;
    bool ntripClient = false;
    bool otaClient = false;
    bool ppl = false;
    bool psram = false;
    bool radio_lora = false;
    bool rtc = false;
    bool serialOutput = false;
    bool tcpClient = false;
    bool tcpServer = false;
    bool udpServer = false;
    bool webServer = false;
    bool authenticationCoPro = false; // MFi authentication
    bool imu_im19 = false;
} online;

// AWS certificate for PointPerfect API
static const char *AWS_PUBLIC_CERT = R"=====(
-----BEGIN CERTIFICATE-----
MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF
ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6
b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL
MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv
b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj
ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM
9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw
IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6
VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L
93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm
jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC
AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA
A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI
U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs
N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv
o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU
5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy
rqXRfboQnoZsG4q5WTP468SQvvG5
-----END CERTIFICATE-----
)=====";

// ISRG Root X1 (Let's Encrypt). Used to validate raw.githubusercontent.com's server cert chain.
static const char GITHUB_RAW_PUBLIC_CERT[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";

//----------------------------------------
// Hardware connections
//----------------------------------------

#define PIN_UNDEFINED -1

// These pins are set in beginBoard()
int pin_debug = PIN_UNDEFINED;              // LED on EVK
int pin_batteryStatusLED = PIN_UNDEFINED;   // LED on Torch
int pin_baseStatusLED = PIN_UNDEFINED;      // LED on EVK
int pin_bluetoothStatusLED = PIN_UNDEFINED; // LED on Torch
int pin_gnssStatusLED = PIN_UNDEFINED;      // LED on Torch

int pin_muxA = PIN_UNDEFINED;
int pin_muxB = PIN_UNDEFINED;
int pin_mux1 = PIN_UNDEFINED;
int pin_mux2 = PIN_UNDEFINED;
int pin_mux3 = PIN_UNDEFINED;
int pin_mux4 = PIN_UNDEFINED;

int pin_modeButton = PIN_UNDEFINED;   // Mode button on EVK, Function button on Facet FP
int pin_powerButton = PIN_UNDEFINED;  // Power and general purpose button on Torch, Facet
int pin_powerFastOff = PIN_UNDEFINED; // Output on Facet
int pin_muxDAC = PIN_UNDEFINED;
int pin_muxADC = PIN_UNDEFINED;
int pin_peripheralPowerControl = PIN_UNDEFINED; // EVK and Facet mosaic

int pin_GnssEvent = PIN_UNDEFINED;   // Facet mosaic
int pin_GnssOnOff = PIN_UNDEFINED;   // Facet mosaic
int pin_chargerLED = PIN_UNDEFINED;  // Facet mosaic
int pin_chargerLED2 = PIN_UNDEFINED; // Facet mosaic
int pin_GnssReady = PIN_UNDEFINED;   // Facet mosaic

int pin_loraRadio_reset = PIN_UNDEFINED;
int pin_loraRadio_boot = PIN_UNDEFINED;
int pin_loraRadio_power = PIN_UNDEFINED;

int pin_Ethernet_CS = PIN_UNDEFINED;
int pin_Ethernet_Interrupt = PIN_UNDEFINED;
int pin_GNSS_CS = PIN_UNDEFINED;
int pin_GNSS_TimePulse = PIN_UNDEFINED;
int pin_GNSS_Reset = PIN_UNDEFINED;

// microSD card pins
int pin_PICO = PIN_UNDEFINED;
int pin_POCI = PIN_UNDEFINED;
int pin_SCK = PIN_UNDEFINED;
int pin_microSD_CardDetect = PIN_UNDEFINED;
int pin_microSD_CS = PIN_UNDEFINED;

int pin_I2C0_SDA = PIN_UNDEFINED;
int pin_I2C0_SCL = PIN_UNDEFINED;

// On EVK, Display is on separate I2C bus
int pin_I2C1_SDA = PIN_UNDEFINED;
int pin_I2C1_SCL = PIN_UNDEFINED;

int pin_GnssUart_RX = PIN_UNDEFINED;
int pin_GnssUart_TX = PIN_UNDEFINED;

int pin_GnssUart2_RX = PIN_UNDEFINED;
int pin_GnssUart2_TX = PIN_UNDEFINED;

int pin_Cellular_RX = PIN_UNDEFINED;
int pin_Cellular_TX = PIN_UNDEFINED;
int pin_Cellular_PWR_ON = PIN_UNDEFINED;
int pin_Cellular_Network_Indicator = PIN_UNDEFINED;
int pin_Cellular_Reset = PIN_UNDEFINED;
int pin_Cellular_RTS = PIN_UNDEFINED;
int pin_Cellular_CTS = PIN_UNDEFINED;

int pin_GNSS_DR_Reset = PIN_UNDEFINED;

int pin_IMU_RX = PIN_UNDEFINED;
int pin_IMU_TX = PIN_UNDEFINED;
int pin_IMU_Boot = PIN_UNDEFINED;

int pin_powerAdapterDetect = PIN_UNDEFINED;
int pin_usbSelect = PIN_UNDEFINED;
int pin_beeper = PIN_UNDEFINED;

bool cellularModemResetLow = false;
#define CELLULAR_MODEM_FC ESP_MODEM_FLOW_CONTROL_NONE
uint8_t laraPwrLowValue;
uint32_t laraTimer; // Backoff timer

int pin_gpioExpanderInterrupt = PIN_UNDEFINED;
const uint8_t gpioExpander_up = 0;
const uint8_t gpioExpander_down = 1;
const uint8_t gpioExpander_right = 2;
const uint8_t gpioExpander_left = 3;
const uint8_t gpioExpander_center = 4;
const uint8_t gpioExpander_cardDetect = 5;
const uint8_t gpioExpander_io6 = 6;
const uint8_t gpioExpander_io7 = 7;

const uint8_t gpioExpanderSwitch_S1 = 0; // Controls U16 switch 1: connect ESP UART0 to CH342 or SW2
const uint8_t gpioExpanderSwitch_S2 = 1; // Controls U17 switch 2: connect SW1 to RS232 Output or GNSS UART4
const uint8_t gpioExpanderSwitch_S3 = 2; // Controls U18 switch 3: connect ESP UART2 to GNSS UART3 or LoRa UART2
const uint8_t gpioExpanderSwitch_S4 = 3; // Controls U19 switch 4: connect GNSS UART2 to 4-pin JST TTL Serial or LoRa UART0
const uint8_t gpioExpanderSwitch_LoraEnable = 4; // LoRa_EN
const uint8_t gpioExpanderSwitch_GNSS_Reset = 5; // RST_GNSS
const uint8_t gpioExpanderSwitch_LoraBoot = 6;   // LoRa_BOOT0 - Used for bootloading the STM32 radio IC
const uint8_t gpioExpanderSwitch_S5 = 7;         // Controls U61 switch 5: connect GNSS UART1 to Port A of CH342
const uint8_t gpioExpanderNumSwitches = 8;

bool usbSerialIsSelected = true;      // Goes false when switch U18 is moved from CH34x to LoRa

//----------------------------------------
// Peripherals
//----------------------------------------

#include <SparkFun_I2C_Expander_Arduino_Library.h> // Click here to get the library: http://librarymanager/All#SparkFun_I2C_Expander_Arduino_Library
#include <SparkFun_IM19_IMU_Arduino_Library.h> //http://librarymanager/All#SparkFun_IM19_IMU
#include <SparkFun_LG290P_GNSS.h> //http://librarymanager/All#SparkFun_LG290P
#include <SparkFun_Unicore_GNSS_Arduino_Library.h> //http://librarymanager/All#SparkFun_Unicore_GNSS
#include <SparkFun_u-blox_GNSS_v3.h> //http://librarymanager/All#SparkFun_u-blox_GNSS_v3
#include <Wire.h>

TwoWire * i2c_0;
TwoWire * i2c_1;

HardwareSerial *uart2Serial; // Shared serial port between LoRa and Tilt

#define SerialForLoRa uart2Serial
#define SerialForTilt uart2Serial

//----------------------------------------
// Time measurement
//----------------------------------------

#define HOURS_IN_A_DAY 24L
#define MINUTES_IN_AN_HOUR 60L
#define SECONDS_IN_A_MINUTE 60L
#define MILLISECONDS_IN_A_SECOND 1000L
#define MILLISECONDS_IN_A_MINUTE (SECONDS_IN_A_MINUTE * MILLISECONDS_IN_A_SECOND)
#define MILLISECONDS_IN_AN_HOUR (MINUTES_IN_AN_HOUR * MILLISECONDS_IN_A_MINUTE)
#define MILLISECONDS_IN_A_DAY (HOURS_IN_A_DAY * MILLISECONDS_IN_AN_HOUR)

#define SECONDS_IN_AN_HOUR (MINUTES_IN_AN_HOUR * SECONDS_IN_A_MINUTE)
#define SECONDS_IN_A_DAY (HOURS_IN_A_DAY * SECONDS_IN_AN_HOUR)

//----------------------------------------
// IM19 IMU
//----------------------------------------

enum Im19UpdateResult
{
    IM19_UPDATE_FAILED = 0,
    IM19_UPDATE_SUCCESS,
    IM19_UPDATE_RETRY, // IM19 reports lost frames - caller should re-request only those byte ranges and call again
};

//----------------------------------------
// Over-The-Air (OTA) Updates
//----------------------------------------

#define OTA_DATA_TIMEOUT        (15 * MILLISECONDS_IN_A_SECOND)

const char * otaEqualSigns = "==================================================";

// Constants to parse GitHub directory listings
const char * otaRawHead = "/raw/refs/heads/main";
const char * otaTree = "},\"tree";
const char * otaFileTree = ":{\"fileTree\":{\"";
const char * otaItems = "\":{\"items\":[";
const char * otaListEnd = "]";
const char * otaName = "\"name\":\"";
const char * otaNameEnd = "\"";

bool otaDebugVerbose;
uint32_t otaFileBytes;

#endif // __SETTINGS_H__
