/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
settings.h
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#ifndef __SETTINGS_H__
#define __SETTINGS_H__

// Product Variant used as part of device ID and whitelists. Do not reorder.
typedef enum
{
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

// Must match the contents of ProductVariant
static const ProductVariant allVariants[] =
{
    RTK_EVK,
    RTK_FACET_MOSAIC,
    RTK_TORCH,
    RTK_POSTCARD,
    RTK_FACET_FP,
    RTK_TORCH_X2,
    RTK_UNKNOWN
};
#define productVariantCount (sizeof(allVariants) / sizeof(allVariants[0]))

typedef struct
{
    ProductVariant productVariant;
    const char *name;
} productProperties;

const productProperties productPropertiesTable[] =
{
    //productVariant        name
    //==============        ====
    { RTK_EVK,              "EVK"},
    { RTK_FACET_MOSAIC,     "Facet X5"},
    { RTK_FACET_FP,         "FP"},
    { RTK_POSTCARD,         "Postcard"},
    { RTK_TORCH,            "Torch"},
    { RTK_TORCH_X2,         "TX2"},
    { RTK_UNKNOWN,          "Unknown"},
};
const int productPropertiesEntries = sizeof(productPropertiesTable) / sizeof(productPropertiesTable[0]);

typedef struct _WIFI_NETWORK
{
    const char * ssid;
    const char * password;
} WIFI_NETWORK;

#define MAX_WIFI_NETWORKS 4

struct Settings
{
    bool enableHeapReport = false; // Turn on to display free heap
    bool debugFirmwareUpdate = false;
    bool enableImuDebug = false; // Turn on to display IMU library debug messages
} settings;

// Indicate which peripherals are present on a given platform
struct struct_present
{
    bool ethernet_ws5500 = false;

    bool gnss_um980 = false;
    bool gnss_zedf9p = false;
    bool gnss_mosaicX5 = false; // L-Band is implicit
    bool gnss_lg290p = false;
    bool gnss_zedx20p = false;

    bool imu_im19 = false;

    bool microSd = false;

} present;

struct struct_online
{
    bool microSD = false;
} online;

#endif  // __SETTINGS_H__
