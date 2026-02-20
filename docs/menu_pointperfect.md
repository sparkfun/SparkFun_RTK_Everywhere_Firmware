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
- TX2: :material-radiobox-marked:{ .support-full title="Feature Supported" }

</div>

<figure markdown>

![PointPerfect Menu](<./img/Terminal/SparkFun RTK Everywhere - PointPerfect Menu.png>)
<figcaption markdown>
Configuring PointPerfect settings over serial
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

For those users looking for L-Band support, sadly, u-blox suspended the North American L-Band service on December 31st, 2025. A global L-Band service has been announced but with no set availability date at this time.

PointPerfect has the following benefits and challenges:

- SparkFun RTK devices require a monthly paid subscription of $15 a month to access to PointPerfect. Please see the [registration](#registration) section for details. 
- Using PointPerfect, a SparkFun RTK device can obtain an RTK Fix anywhere there is [coverage](https://www.u-blox.com/en/pointperfect-service-coverage). It depends on the type of service, but this generally includes the US contiguous 48 states, the EU, Korea, as well as parts of Australia, Brazil, and Canada.
- You don't need to be near a base station - the PPP-RTK model covers entire continents.
- Because PointPerfect uses a model instead of a dedicated base station, it is cheaper than many alternative correction services. However, the RTK Fix is not as accurate (3-6cm) as compared to getting corrections from a dedicated base station (2cm or better but depends on the baseline distance).

## PointPerfect Service Types

### PointPerfect Flex RTCM/NTRIP

**Note:** Most users will use the NTRIP Client within their GIS app to connect to PointPerfect Flex. See [Using PointPerfect Flex](using_pointperfect_flex.md) for more information. Only use this option if the GIS app is needed and work arounds like [GNSS Master](gis_software_android.md#gnss-master) are not available.

This service is commonly used with cellular hot spots. When this service is enabled, the device will attempt to connect to WiFi. If [registration](#registration) has been completed through SparkFun, the device will be allowed to obtain NTRIP credentials. The NTRIP Client will be automatically turned on, and the device will quickly enter RTK Float, the RTK Fix states. Users report fast fix times with good accuracy.

<figure markdown>

![RTK Fix using WiFi to connect over WiFi](<./img/Displays/SparkFun RTK Wide WiFi NTRIP.png>)
<figcaption markdown>
RTK Fix using WiFi to connect over WiFi
</figcaption>
</figure>

In the above image we see the WiFi symbol indicating a high signal strength. The down arrow indicates the arrival of new data. The double cross hair indicates the device has an RTK Fix with a reported 5mm horizontal accuracy. 39 satellites are in view. The IP address of the device is shown. In the lower right corner the SNIP icon indicates the corrections source is NTRIP, and the page icon updates to indicate active logging. The **A4D2** indicates the device's Bluetooth MAC.

#### Credentials

PointPerfect Flex RTCM/NTRIP service uses a standard NTRIP interface to obtain corrections. Once [registered](#registration), to gain access to the PointPerfect system, the device must be given WiFi. The RTK device will automatically obtain **credentials**. These credentials will be copied into the NTRIP Client, and the NTRIP Client will be turned on. The device will then use WiFi (usually a cellular hot spot) to connect to the PointPerfect NTRIP Caster and begin providing the SparkFun RTK device with corrections. Within a few seconds, the device should change from RTK Float to RTK Fix.

## Coverage

<figure markdown>

[![PointPerfect Coverage map](<./img/PointPerfect/SparkFun RTK Everywhere - PointPerfect Coverage Map Small.png>)](https://www.u-blox.com/en/pointperfect-service-coverage)

<figcaption markdown>
[PointPerfect Coverage map](https://www.u-blox.com/en/pointperfect-service-coverage)
</figcaption>
</figure>

## Registration

All SparkFun RTK products can operate in RTK mode out-of-the-box using corrections from a local base or a RTCM provider (see [Correction Sources](correction_sources.md)). If you wish to use PointPerfect corrections, the cost varies from $15 to $50 per month and varies on the service type. The device must be registered before it is allowed on the PointPerfect network. To facilitate this, please [obtain your device ID](https://docs.sparkfun.com/SparkFun_RTK_Everywhere_Firmware/menu_pointperfect/#obtaining-the-device-id) through the software interface and visit the registration page associated with your device:

- [RTK EVK registration page](https://www.sparkfun.com/rtk_evk_registration)
- [RTK Facet mosaic registration page](https://www.sparkfun.com/rtk_facet_mosaic_registration)
- [RTK Postcard registration page](https://www.sparkfun.com/rtk_postcard_registration)
- [RTK Torch registration page](https://www.sparkfun.com/rtk_torch_registration)
- [RTK TX2 registration page](https://www.sparkfun.com/tx2_registration)

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

- **1** - Select the type of PointPerfect service.
- **2** - Use WiFi to obtain NTRIP credentials. Requires the device ID be put on the RTCM allow list.
- **i** - Display the Device ID. This is needed when a SparkFun RTK device needs to be added to the PointPerfect system. This is needed when first registering the device, or modifying a subscription.
