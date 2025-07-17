# PointPerfect Menu

<!--
Compatibility Icons
====================================================================================

:material-radiobox-marked:{ .support-full title="Feature Supported" }
:material-radiobox-indeterminate-variant:{ .support-partial title="Feature Partially Supported" }
:material-radiobox-blank:{ .support-none title="Feature Not Supported" }
-->

<div class="grid cards fill" markdown>

- EVK: :material-radiobox-marked:{ .support-full title="Feature Supported" }
- Facet mosaic: :material-radiobox-marked:{ .support-full title="Feature Supported" }
- Postcard: :material-radiobox-marked:{ .support-full title="Feature Supported" }
- Torch: :material-radiobox-marked:{ .support-full title="Feature Supported" }

</div>

<figure markdown>

![PointPerfect Menu](<./img/Terminal/SparkFun RTK Everywhere - PointPerfect Menu.png>)
<figcaption markdown>
Configuring PointPerfect settings over serial
</figcaption>
</figure>

<figure markdown>

![PointPerfect Configuration Menu](<./img/WiFi Config/SparkFun RTK PointPerfect Config.png>)
<figcaption markdown>
PointPerfect L-Band North America Configuration Options
</figcaption>
</figure>

<figure markdown>

![PointPerfect Configuration Menu](<./img/WiFi Config/SparkFun RTK PointPerfect Config RTCM NTRIP.png>)
<figcaption markdown>
PointPerfect NTRIP/RTCM Configuration Options
</figcaption>
</figure>

## What is PointPerfect?

PointPerfect is a paid correction service offered by u-blox that allows a SparkFun RTK device to quickly achieve a high-precision RTK Fix. Because PointPerfect is one of the easiest and lowest-cost services currently available, the SparkFun RTK product line has been designed to make it easy to use this correction service. 

You are welcome to use any correction service! The SparkFun RTK devices are compatible with nearly all corrections services and public NTRIP Casters. See the section on [Correction Sources](correction_sources.md) for alternatives including some free options (where available).

*PointPerfect Flex* (previously known simply as *PointPerfect*) is just one service offered by u-blox/ThingStream. Other services (announced but not yet launched) are *PointPerfect Live* and *PointPerfect Global*. For the purposes of this article we will refer to *PointPerfect Flex* as *PointPerfect*.

PointPerfect corrections are obtained by two methods:

- **IP**: Corrections are transmitted over the internet. The RTK device will need access to a WiFi (or optionally an Ethernet/Cellular network on the RTK EVK). For WiFi, this is most commonly a hotspot on a cell phone so this delivery method is generally most common for areas with cellular and/or other WiFi coverage.
- **L-Band**: Corrections are transmitted from a geosynchronous satellite. Coverage is limited to the US contiguous 48 states. This delivery method requires special equipment thus this method is only compatible with the the [RTK EVK](https://www.sparkfun.com/products/24342) and [RTK Facet mosaic](https://www.sparkfun.com/sparkpnt-rtk-facet-mosaic-l-band.html). No cellular or internet connection is required.
    - Sadly, u-blox is suspending the North American L-Band service on December 31st, 2025. A global L-Band service has been announced but with no set availability date at this time.

PointPerfect has the following benefits and challenges:

- SparkFun RTK devices require a monthly paid subscription varying from $15 to $50 a month to access to PointPerfect. Please see the product details for your device. 
	- [RTK EVK Registration](https://www.sparkfun.com/rtk_evk_registration)
	- [RTK Facet mosaic Registration](https://www.sparkfun.com/rtk_facet_mosaic_registration)
	- [RTK Postcard Registration](https://www.sparkfun.com/rtk_postcard_registration)
	- [RTK Torch Registration](https://www.sparkfun.com/rtk_torch_registration)
- Using PointPerfect, a SparkFun RTK device can obtain an RTK Fix anywhere there is [coverage](https://www.u-blox.com/en/pointperfect-service-coverage). It depends on the type of service, but this generally includes the US contiguous 48 states, the EU, Korea, as well as parts of Australia, Brazil, and Canada.

- You don't need to be near a base station - the PPP-RTK model covers entire continents.
- Because PointPerfect uses a model instead of a dedicated base station, it is cheaper than many alternative correction services. However, the RTK Fix is not as accurate (3-6cm) as compared to getting corrections from a dedicated base station (2cm or better but depends on the baseline distance).
- Because PointPerfect uses a model instead of a dedicated base station, convergence times (the time to get to RTK Fix) can vary widely. Expect to wait multiple minutes for an RTK Fix, as opposed to corrections from a dedicated that can provide an RTK Fix in seconds.

## PointPerfect Service Types

### PointPerfect Flex RTCM/NTRIP

This service is commonly used with cellular hot spots. When this service is enabled, the device will attempt to connect to WiFi. If registration has been completed through SparkFun (see above), the device will be allowed to obtain NTRIP credentials. The NTRIP Client will be automatically turned on, and the device will quickly enter RTK Float, the RTK Fix states. Users report fast fix times with good accuracy.

<figure markdown>

![RTK Fix using WiFi to connect over WiFi](<./img/Displays/SparkFun RTK Wide WiFi NTRIP.png>)
<figcaption markdown>
RTK Fix using WiFi to connect over WiFi
</figcaption>
</figure>

In the above image we see the WiFi symbol indicating a high signal strength. The down arrow indicates the arrival of new data. The double cross hair indicates the device has an RTK Fix with a reported 5mm horizontal accuracy. 39 satellites are in view. The IP address of the device is shown. In the lower right corner the SNIP icon indicates the corrections source is NTRIP, and the page icon updates to indicate active logging. The **A4D2** indicates the device's Bluetooth MAC.

#### Credentials

PointPerfect Flex RTCM/NTRIP service uses a standard NTRIP interface to obtain corrections. Once registered, to gain access to the PointPerfect system, the device must be given WiFi. The RTK device will automatically obtain **credentials**. These credentials will be copied into the NTRIP Client, and the NTRIP Client will be turned on. The device will then use WiFi (usually a cellular hot spot) to connect to the PointPerfect NTRIP Caster and begin providing the SparkFun RTK device with corrections. Within a few seconds, the device should change from RTK Float to RTK Fix.

### PointPerfect L-Band

When this service is enabled, the device will attempt to connect to WiFi. If registration has been completed through SparkFun (see above), the device will be issued L-Band specific keys and will obtain corrections over an extra signal broadcast by a geosynchronous satellite. The encrypted SPARTN packets are sent through the PointPerfectLibrary (PPL) and RTCM corrections are produced after ~60 seconds. The device will enter RTK Float, the RTK Fix states. Users report variable fix times with ok accuracy (not as good as RTCM service).

#### Keys

Once registered, to gain access to the PointPerfect L-Band system, the device must be given WiFi. The RTK device will automatically obtain **keys**. These keys allow the decryption of L-Band based corrections. 

PointPerfect L-Band keys are valid for a maximum of 56 days. During that time, the RTK device can operate normally without the need to update keys. However, when the keys are set to expire in 28 days or less, the RTK device will attempt to log in to WiFi at each power on. If WiFi is not available, it will continue normal operation.

On RTK L-Band equipped devices, if the keys fully expire, the device will continue to receive the L-Band signal but will be unable to decrypt the signal. The RTK device will continue to have extraordinary accuracy (we've seen better than 0.15m HPA) but not the centimeter-level accuracy that comes with RTK.

!!! note
	All RTK devices (including those equipped with L-Band) are capable of receiving RTCM corrections over traditional means including NTRIP data over Bluetooth or a serial radio, WiFi (and Ethernet/Cellular on the RTK EVK).

<figure markdown>

![Display showing 14 days until Keys Expire](<./img/Displays/SparkFun_RTK_LBand_DayToExpire.jpg>)
<figcaption markdown>
Display showing 14 days until keys expire
</figcaption>
</figure>

On devices that have a display, the unit will display various prompts to aid the user in obtaining keys as needed.

#### Decryption Icon

<figure markdown>

![Three-pronged satellite dish indicating L-Band reception](<./img/Displays/SparkFun_RTK_LBand_Indicator.jpg>)
<figcaption markdown>
Three-pronged satellite dish indicating L-Band reception
</figcaption>
</figure>

<figure markdown>

![Three-pronged satellite dish indicating L-Band reception on EVK](<./img/Displays/SparkFun_RTK_EVK_LBand_Indicator.png>)
<figcaption markdown>
Three-pronged satellite dish indicating L-Band reception on EVK
</figcaption>
</figure>

On devices that have a display, upon successful reception and decryption of PointPerfect corrections delivered over L-Band, the satellite dish icon will increase to a three-pronged icon. As the unit's fix increases the cross-hair will indicate a basic 3D solution, a double blinking cross-hair will indicate a floating RTK solution, and a solid double cross-hair will indicate a fixed RTK solution.

### PointPerfect Flex MQTT (Deprecated)

This service has been discontinued by PointPerfect and is still available for legacy devices. It is recommended to use PointPerfect Flex RTCM/NTRIP for a better experience. When this service is enabled, the device will attempt to connect to WiFi. If registration has been completed through SparkFun (see above), the device will be issued MQTT specific keys and will obtain corrections over an MQTT client. The encrypted SPARTN packets are sent through the PointPerfectLibrary (PPL) and RTCM corrections are produced after ~60 seconds. The device will enter RTK Float, the RTK Fix states. Users report variable fix times with ok accuracy (not as good as RTCM service). 

#### Localized Corrections

The u-blox PointPerfect Localized correction service via MQTT (IP) offers quick delivery of high precision accuracy by providing your device only the SPARTN corrections applicable to your location. This feature offers several advantages over the traditional continental streams, including significantly reduced bandwidth requirements and seamless transition between regions.

- **Reduced bandwidth requirements:** PointPerfect Localized can reduce bandwidth requirements by up to 80%. This is a significant advantage for applications sensitive to bandwidth constraints, such as those that operate in remote areas or use low-power devices.
- **Retained Privacy:** PointPerfect Localized retains user privacy by not sending the device's precise location to the service to receive correction data. This is done by using a general node-based location system.

PointPerfect Localized works by dividing the coverage area into a grid of tiles. Each tile contains a set of nodes that are relevant to a user located within that tile. Unlike the continental level approach where a device subscribes to the continental level topic, in the localized approach a device subscribes to the localized node topic based on its location. This ensures that the device receives only the correction data that is relevant to its location, greatly reducing the required bandwidth.

<figure markdown>

![Comparison of PointPerfect Localized node density](<./img/PointPerfect/SparkFun RTK Everywhere - PointPerfect Localized Distribution.png>)
<figcaption markdown>
Comparison of PointPerfect Localized node density
</figcaption>
</figure>

The above image shows the Localized tiles for Level 2 (250 x 250km sparse, ~90-100km separation) vs. Level 5 (250 x 250km high density, ~30km separation).

Localized distribution can be enabled via the serial menu (PointPerfect menu - option 'p'), or the web config page. On serial, option '4' will enable or disable localized distribution; option '5' selects the tile level.

<figure markdown>

![PointPerfect Menu](<./img/Terminal/SparkFun RTK Everywhere - PointPerfect Menu.png>)
<figcaption markdown>
Configuring PointPerfect settings over serial
</figcaption>
</figure>

## Coverage

[<figure markdown>

![PointPerfect Coverage map including L-Band and IP delivery methods](<./img/PointPerfect/SparkFun RTK Everywhere - PointPerfect Coverage Map Small.png>)](https://www.u-blox.com/en/pointperfect-service-coverage)
<figcaption markdown>
PointPerfect Coverage map including L-Band and IP delivery methods
</figcaption>
</figure>

	!!! note
		 L-Band coverage is currently limited to North America and u-blox has announced L-Band North America service will be discontinued on December 31st, 2025.

## Registration

All SparkFun RTK products can operate in RTK mode out-of-the-box using corrections from a local base or a RTCM provider (see [Correction Sources](correction_sources.md)). If you wish to use PointPerfect corrections, the device must be registered before it is allowed on the PointPerfect network. To facilitate this, please [obtain your device ID](https://docs.sparkfun.com/SparkFun_RTK_Everywhere_Firmware/menu_pointperfect/#obtaining-the-device-id) through the software interface and visit the registration page associated with your device:

- **RTK EVK:** please visit the [RTK EVK registration page](https://www.sparkfun.com/rtk_evk_registration)
- **RTK Facet mosaic:** please visit the [RTK Facet mosaic registration page](https://www.sparkfun.com/rtk_facet_mosaic_registration)
- **RTK Postcard:** please visit the [RTK Postcard registration page](https://www.sparkfun.com/rtk_postcard_registration)
- **RTK Torch:** please visit the [RTK Torch registration page](https://www.sparkfun.com/rtk_torch_registration)

## Obtaining the Device ID

The device ID is unique to each RTK device and must be entered by SparkFun into the PointPerfect network.

<figure markdown>

![Device ID within the serial menu](<./img/Terminal/SparkFun RTK Everywhere - PointPerfect Menu Device ID.png>)
<figcaption markdown>
Device ID within the serial menu
</figcaption>
</figure>

<figure markdown>

![Device ID within the WiFi Config page](<./img/WiFi Config/SparkFun RTK PointPerfect Config Device ID.png>)
<figcaption markdown>
Device ID within the WiFi Config page
</figcaption>
</figure>

This ID can be obtained by using option **i** from the *PointPerfect* menu or by opening the PointPerfect section within the [WiFi Config](https://docs.sparkfun.com/SparkFun_RTK_Everywhere_Firmware/configure_with_wifi/) interface in the PointPerfect Configuration section.

## PointPerfect Serial Menu

<figure markdown>

![PointPerfect Menu](<./img/Terminal/SparkFun RTK Everywhere - PointPerfect Menu.png>)
<figcaption markdown>
PointPerfect Menu
</figcaption>
</figure>

**Note:** Various options may not be shown depending on the PointPerfect service level.

The *Days until keys expire* inform the user how many days the unit has until it needs to connect to WiFi or Ethernet to obtain new keys.

- **1** - Select the type of PointPerfect service.
- **3** - Enable / disable the automatic attempts at WiFi / Ethernet connections when key expiry is in less than 28 days.
- **4** - Trigger an immediate attempt to connect over WiFi / Ethernet and provision the device (if no keys are available) or update the keys (if provisioning has already been completed). Depending on which RTK product you have and which interfaces are connected, it may be necessary to exit the menus for the provisioning / update to take place.
- **5** - Enable / disable [localized distribution](#localized-corrections)
- **6** - When localized distribution is enabled, option 5 can be used to select the tile level. The default is Level 5 - 250 x 250km tiles, high density.
- **a** - Enable / disable AssistNow. This is a service over PointPerfect MQTT (Deprecated) that can decrease the 3D fix time at startup.
- **c** - Clear the current keys.
- **i** - Display the Device ID. This is needed when a SparkFun RTK device needs to be added to the PointPerfect system. This is needed when first registering the device, or modifying a subscription. [Go here for RTK Torch](https://www.sparkfun.com/rtk_torch_registration) to enable or renew your subscription. [Go here for RTK EVK](https://www.sparkfun.com/rtk_evk_registration) subscriptions.
- **k** - Bring up the Manual Key Entry menu.
- **g** - Set the Geographic Region. The default is US; but EU, Australia, Korea and Japan can also be selected. This is an important setting since it sets both the IP correction distribution topic (PointPerfect MQTT (Deprecated)).

### Manual Key Entry

<figure markdown>

![Manual Key Entry menu](<./img/Terminal/SparkFun RTK Everywhere - PointPerfect Menu Manual Key Entry.png>)
<figcaption markdown>
Manual Key Entry Menu
</figcaption>
</figure>

Because of the length and complexity of the keys, we do not recommend you manually enter them. This menu is most helpful for displaying the current keys.

Option '1' will allow a user to enter their Device Profile Token. This is the token that is used to provision a device on a PointPerfect account. By default, users may use the SparkFun token but must pay SparkFun for the annual service fee. If an organization would like to administer its own devices, the token can be changed here.

## Error Messages

There are various messages that may be reported by the device. Here is a list of explanations and resolutions.

### No SSIDs

	Error: Please enter at least one SSID before getting keys

This message is seen when no WiFi network credentials (SSID and password) have been entered. The device needs WiFi to obtain the keys to decrypt the packets provided by PointPerfect. Enter your home/office/cellular hotspot WiFi SSID and password and try again.

### Not Whitelisted

	This device is not whitelisted. Please contact support@sparkfun.com to get your subscription activated. Please reference device ID: [device ID]

This message is seen whenever the PointPerfect service is not aware of the given device. Please see the [Registration](menu_pointperfect.md#registration) section to get SparkFun your device type, ID, and payment details. Alternatively contact support@sparkfun.com with your device ID as reported in the error or manually [Obtain the Device ID](menu_pointperfect.md#obtaining-the-device-id).

### Device Deactivated

	This device has been deactivated. Please contact support@sparkfun.com to renew the PointPerfect subscription. Please reference device ID: [device ID]

This message is seen whenever the device's subscription has lapsed. Please see the [Registration](menu_pointperfect.md#registration) section to get SparkFun your device type, ID, and payment details. Alternatively contact support@sparkfun.com with your device ID as reported in the error or manually [Obtain the Device ID](menu_pointperfect.md#obtaining-the-device-id).

### Device Registered to a Different Account

	This device is registered on a different profile. Please contact support@sparkfun.com for more assistance. Please reference device ID: [device ID]
	
This message is seen whenever the device was assigned to a different PointPerfect service level, usually when the user has decided to switch service types. This needs to be manually modified. Contact support@sparkfun.com with your device ID as reported in the error or manually [Obtain the Device ID](menu_pointperfect.md#obtaining-the-device-id).

### HTTP response error -11 - Read Timeout

The connection to PointPerfect did not respond. Please try again or try a different WiFi network or access point (AP).
