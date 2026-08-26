/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Work_Arounds.h
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

// Firmware version
#define FIRMWARE_VERSION_MAJOR  99
#define FIRMWARE_VERSION_MINOR  99

// Menu support
const uint16_t menuTimeout = 60 * 10; // Menus will exit/timeout after this number of seconds

// Product support
#define productVariantProperties getProductPropertiesFromVariant(productVariant)

// Serial output routines
#define systemPrint             Serial.print
#define systemPrintf            Serial.printf
#define systemPrintln           Serial.println

// Time constants
#define HOURS_IN_A_DAY 24L
#define MINUTES_IN_AN_HOUR 60L
#define SECONDS_IN_A_MINUTE 60L
#define MILLISECONDS_IN_A_SECOND 1000L
#define MILLISECONDS_IN_A_MINUTE (SECONDS_IN_A_MINUTE * MILLISECONDS_IN_A_SECOND)
#define MILLISECONDS_IN_AN_HOUR (MINUTES_IN_AN_HOUR * MILLISECONDS_IN_A_MINUTE)
#define MILLISECONDS_IN_A_DAY (HOURS_IN_A_DAY * MILLISECONDS_IN_AN_HOUR)

#define SECONDS_IN_AN_HOUR (MINUTES_IN_AN_HOUR * SECONDS_IN_A_MINUTE)
#define SECONDS_IN_A_DAY (HOURS_IN_A_DAY * SECONDS_IN_AN_HOUR)

// Network constants
#define NETCONSUMER_DEVICE_OTA          0
#define NETWORK_ANY                     0
#define WIFI_IP_ADDRESS_TIMEOUT_MSEC    (90 * MILLISECONDS_IN_A_SECOND)
