# PointPerfect Menu

Torch: ![Feature Supported](img/Icons/GreenDot.png) 

![PointPerfect Menu](<img/Terminal/SparkFun RTK Everywhere - PointPerfect Menu.png>)

*Configuring PointPerfect settings over serial*

![PointPerfect Configuration Menu](<img/WiFi Config/SparkFun RTK WiFi Config - PointPerfect Menu.png>)

*PointPerfect Configuration Menu*

## Coverage

[![PointPerfect Coverage map including L-Band and IP delivery methods](<img/PointPerfect/SparkFun RTK Everywhere - PointPerfect Coverage Map Small.png>)](https://www.u-blox.com/en/pointperfect-service-coverage)

*PointPerfect Coverage map including L-Band and IP delivery methods*

SparkFun RTK devices are equipped to get corrections from a service called PointPerfect. 

PointPerfect has the following benefits and challenges:

* Most SparkFun RTK devices come with either a pre-paid subscription or one month of free access to PointPerfect. Please see the product details for your device. [Go here](https://www.sparkfun.com/rtk_torch_registration) to enable or renew your subscription.
* A SparkFun RTK device can obtain RTK Fix anywhere there is [coverage](https://www.u-blox.com/en/pointperfect-service-coverage). This includes the US contiguous 48 states, the EU, Korea, as well as parts of Australia, Brazil, and Canada. Note: L-Band coverage is not available in some of these areas.
* You don't need to be near a base station - the PPP-RTK model covers entire continents.
* Because PointPerfect uses a model instead of a dedicated base station, it is cheaper. However, the RTK Fix is not as accurate (3-6cm) as compared to getting corrections from a dedicated base station (2cm or better but depends on the baseline distance).
* Because PointPerfect uses a model instead of a dedicated base station, convergence times (the time to get to RTK Fix) can vary widely. Expect to wait multiple minutes for an RTK Fix, as opposed to corrections from a dedicated that can provide an RTK Fix in seconds.

PointPerfect corrections are obtained by two methods:

* **L-Band**: Corrections are transmitted from a geosynchronous satellite. Coverage areas are limited to the US contiguous 48 states and the EU. This delivery method requires special equipment (see the [RTK Facet L-Band](https://www.sparkfun.com/products/20000) for more information). No cellular or internet connection is required.

* **IP**: Corrections are transmitted over the internet. The RTK device will need access to a WiFi network. This is most commonly a hotspot on a cell phone so this delivery method is generally confined to areas with cellular and/or WiFi coverage.

## Registration

![Three stickers showing Device ID and QR code to registration page](<img/Torch/SparkFun RTK Torch - Device ID Stickers.png>)

*Three stickers showing Device ID and QR code to registration page*

All SparkFun RTK products must be registered before they are allowed on the PointPerfect network. To facilitate this, most products ship with a printed Device ID sticker and registration QR code included with the product. The QR code will prefill the registration page with the device's unique ID. If you do not have these materials, don't worry! Please visit the [registration page](https://www.sparkfun.com/rtk_torch_registration) and [obtain your device ID](https://docs.sparkfun.com/SparkFun_RTK_Everywhere_Firmware/menu_pointperfect/#obtaining-the-device-id) through the software interface.

## Keys

To gain access to the PointPerfect system, the device must be given WiFi. Once provided, the RTK device will automatically obtain **keys**. These keys allow the decryption of corrections.

PointPerfect keys are valid for a maximum of 56 days. During that time, the RTK device can operate normally without the need to update keys. However, when the keys are set to expire in 28 days or less, the RTK device will attempt to log in to WiFi at each power on. If WiFi is not available, it will continue normal operation. 

On RTK L-Band equipped devices, if the keys fully expire, the device will continue to receive the L-Band signal but will be unable to decrypt the signal. The RTK Facet L-Band will continue to have extraordinary accuracy (we've seen better than 0.15m HPA) but not the centimeter-level accuracy that comes with RTK.

**Note:** All RTK devices (including those equipped with L-Band) are capable of receiving RTCM corrections over traditional means including NTRIP data over Bluetooth or a serial radio. 

![Display showing 14 days until Keys Expire](img/Displays/SparkFun_RTK_LBand_DayToExpire.jpg)

*Display showing 14 days until keys expire*

On devices that have a display, the unit will display various prompts to aid the user in obtaining keys as needed.

## PointPerfect Serial Menu

![PointPerfect Menu](<img/Terminal/SparkFun RTK Everywhere - PointPerfect Menu.png>)

*PointPerfect Menu*

The *Days until keys expire* inform the user how many days the unit has until it needs to connect to WiFi to obtain new keys.

* **1** - Disable the use of PointPerfect corrections.

* **2** - Disable the automatic attempts at WiFi connections when key expiry is less than 28 days.

* **3** - Trigger an immediate attempt to connect over WiFi and provision the device (if no keys are available) or update the keys (if provisioning has already been completed).

* **4** - Display the Device ID. This is needed when a SparkFun RTK device needs to be added to the PointPerfect system. This is needed when first registering the device, or modifying a subscription. [Go here](https://www.sparkfun.com/rtk_torch_registration) to manage subscriptions.

* **c** - Clear the current keys.

* **k** - Bring up the Manual Key Entry menu.

## Obtaining the Device ID

The device ID is unique to each RTK device and must be entered by SparkFun into the PointPerfect network. 

![Device ID within the serial menu](<img/Terminal/SparkFun RTK Everywhere - PointPerfect Menu Device ID.png>)

*Device ID within the serial menu*

![Device ID within the WiFi Config page](<img/WiFi Config/SparkFun RTK PointPerfect Config.png>)

*Device ID within the WiFi Config page*

This ID can be obtained by using option **4** from the *PointPerfect* menu or by opening the PointPerfect section within the [WiFi Config](https://docs.sparkfun.com/SparkFun_RTK_Everywhere_Firmware/configure_with_wifi/) interface in the PointPerfect Configuration section.

## Manual Key Entry

![Manual Key Entry menu](<img/Terminal/SparkFun RTK Everywhere - PointPerfect Menu Manual Key Entry.png>)

*Manual Key Entry Menu*

Because of the length and complexity of the keys, we do not recommend you manually enter them. This menu is most helpful for displaying the current keys.

Option '1' will allow a user to enter their Device Profile Token. This is the token that is used to provision a device on a PointPerfect account. By default, users may use the SparkFun token but must pay SparkFun for the annual service fee. If an organization would like to administer its own devices, the token can be changed here.

## L-Band Decryption Icon

![Three-pronged satellite dish indicating L-Band reception](img/Displays/SparkFun_RTK_LBand_Indicator.jpg)

*Three-pronged satellite dish indicating L-Band reception*

On devices that have a display, upon successful reception and decryption of PointPerfect corrections delivered over L-Band, the satellite dish icon will increase to a three-pronged icon. As the unit's fix increases the cross-hair will indicate a basic 3D solution, a double blinking cross-hair will indicate a floating RTK solution, and a solid double cross-hair will indicate a fixed RTK solution.

## Error Messages

There are various messages that may be reported by the device. Here is a list of explanations and resolutions.

### No SSIDs

    Error: Please enter at least one SSID before getting keys

This message is seen when no WiFi network credentials (SSID and password) have been entered. The device needs WiFi to obtain the keys to decrypt the packets provided by PointPerfect. Enter your home/office/cellular hotspot WiFi SSID and password and try again.

### Not Whitelisted

    This device is not whitelisted. Please contact support@sparkfun.com to get your subscription activated. Please reference device ID: [device ID]

This message is seen whenever the PointPerfect service is not aware of the given device. Please use the [subscription form](https://www.sparkfun.com/rtk_torch_registration) or contact support@sparkfun.com with your device ID (see [Obtaining the Device ID](menu_pointperfect.md#obtaining-the-device-id) above).

### Device Deactivated

    This device has been deactivated. Please contact support@sparkfun.com to renew the PointPerfect subscription. Please reference device ID: [device ID]

This message is seen whenever the device's subscription has lapsed. Please use the [subscription form](https://www.sparkfun.com/pointperfect) or contact support@sparkfun.com with your device ID (see [Obtaining the Device ID](menu_pointperfect.md#obtaining-the-device-id) above).

### HTTP response error -11 - Read Timeout

The connection to PointPerfect did not respond. Please try again or try a different WiFi network or access point (AP).
