# Quick Start - RTK EVK

![RTK EVK](img/SparkFun_RTK_EVK.png)

This quick start guide will get you started in 10 minutes or less. For the full product manual, please proceed to the [**Introduction**](index.md).

Are you using [Android](#android) or [iOS](#ios)?

## Android

1. Download [SW Maps](https://play.google.com/store/apps/details?id=np.com.softwel.swmaps). This may not be the GIS software you intend to do your data collection, but SW Maps is free and makes sure everything is working correctly out of the box.

	![Download SW Maps](<img/SWMaps/SparkFun RTK SW Maps for Android QR Code.png>)

	*Download SW Maps for Android*

2. Connect the antennas.

	* The RTK EVK has its own [hookup guide](http://docs.sparkfun.com/SparkFun_RTK_EVK/) which has a dedicated section on [hardware hookup](https://docs.sparkfun.com/SparkFun_RTK_EVK/hardware_hookup/). As a minimum:
	* Position the L1/L2/L5 GNSS antenna outside, with a clear view of the sky
	* Connect the GNSS antenna to the EVK using the supplied TNC-SMA cable
	* Attach the combined WiFi and Bluetooth® antenna

	![RTK EVK kit](<img/SparkFun_RTK_EVK_Kit.jpg>)

	*Figure 1*

3. Provide power.

	* Connect a power source. The EVK can be powered by: either of the two USB-C ports on the front panel; Power-over-Ethernet; or a DC voltage source (9V-36V) via the VIN screw terminals on the rear.
	* The simplest way is to connect the provided USB power supply to one of the USB-C ports using the provided cable.
	* The EVK will power on and begin to acquire satellites.

	![RTK EVK assembled](<img/SparkFun_RTK_EVK.jpg>)

	*Figure 2*

4. From your cell phone, open Bluetooth settings and pair it with a new device. You will see a list of available Bluetooth devices. Select the ‘EVK Rover-3AF1’. The '3AF1' is the last four digits of the device's MAC address and will vary depending on the device (Figure 3).

	![List of Bluetooth devices on Android](<img/QuickStart/SparkFun Torch - Available Devices.png>)

	*Figure 3*

5. Once paired, open SW Maps. Select ‘New Project’ and give your project a name like ‘RTK Project’.

6. Press the SW Maps icon in the top left corner of the home screen and select **Bluetooth GNSS**. You should see the ‘EVK Rover-3AF1’ in the list. Select it. Confirm that the *Instrument Model* is **SparkFun RTK**, then press the ‘Connect’ button in the bottom right corner (Figure 4). SW Maps will show a warning that the instrument height is 0m. That’s ok.

	![SW Map list of Bluetooth devices](<img/QuickStart/SparkFun Torch - SW Maps Bluetooth Small.png>)

	*Figure 4*

7. Make sure the GNSS antenna is outside with a clear view of the sky. GNSS doesn’t work indoors or near windows. Press the SW Maps icon in the top left corner of the home screen and select **GNSS Status**. Within about 30 seconds you should see 10 or more satellites in view (SIV) (Figure 5). More SIV is better. We regularly see 30 or more SIV. The horizontal positional accuracy (HPA) will decrease as more satellites are acquired. The lower the HPA the more accurate your position.

	![RTK GNSS Status Window](<img/QuickStart/SparkFun Torch - SW Maps GNSS Status SIV Small.png>)

	*Figure 5*

To improve the accuracy (down to 1.4cm), you now need to enable the PointPerfect corrections. Continue reading the [RTK Crash Course](#rtk-crash-course).

## iOS

The software options for Apple iOS are much more limited because Apple products do not support Bluetooth SPP. That's ok! The SparkFun RTK products support Bluetooth Low Energy (BLE) which *does* work with iOS.

1. Download [SW Maps for iOS](https://apps.apple.com/us/app/sw-maps/id6444248083). This may not be the GIS software you intend to do your data collection, but SW Maps is free and makes sure everything is working correctly out of the box.

	![SW Maps for Apple](<img/SWMaps/SparkFun RTK SW Maps for Apple QR Code.png>)

	*Download SW Maps for iOS*

2. Connect the antennas.

	* The RTK EVK has its own [hookup guide](http://docs.sparkfun.com/SparkFun_RTK_EVK/) which has a dedicated section on [hardware hookup](https://docs.sparkfun.com/SparkFun_RTK_EVK/hardware_hookup/). As a minimum:
	* Position the L1/L2/L5 GNSS antenna outside, with a clear view of the sky
	* Connect the GNSS antenna to the EVK using the supplied TNC-SMA cable
	* Attach the combined WiFi and Bluetooth® antenna

	![RTK EVK kit](<img/SparkFun_RTK_EVK_Kit.jpg>)

	*Figure 1*

3. Provide power.

	* Connect a power source. The EVK can be powered by: either of the two USB-C ports on the front panel; Power-over-Ethernet; or a DC voltage source (9V-36V) via the VIN screw terminals on the rear.
	* The simplest way is to connect the provided USB power supply to one of the USB-C ports using the provided cable.
	* The EVK will power on and begin to acquire satellites.

	![RTK EVK assembled](<img/SparkFun_RTK_EVK.jpg>)

	*Figure 2*

4. Open SW Maps. Select ‘New Project’ and give your project a name like ‘RTK Project’.

5. Press the SW Maps icon in the top left corner of the home screen and select *Bluetooth GNSS*. You will need to agree to allow a Bluetooth connection. Set the *Instrument Model* to **Generic NMEA (Bluetooth LE)**. Press 'Scan' and your RTK device should appear.

	![RTK EVK SW Maps (iOS)](<img/SparkFun_EVK_SWMaps_iOS_1.png>)

	*Figure 3*

6. Select it then press the ‘Connect’ button in the bottom right corner.

	![RTK EVK SW Maps (iOS)](<img/SparkFun_EVK_SWMaps_iOS_2.png>)

	*Figure 4*

7. Make sure the GNSS antenna is outside with a clear view of the sky. GNSS doesn’t work indoors or near windows. Press the SW Maps icon in the top left corner of the home screen and select **GNSS Status**. Within about 30 seconds you should see 10 or more satellites in view (SIV) (Figure 5). More SIV is better. We regularly see 30 or more SIV. The horizontal positional accuracy (HPA) will decrease as more satellites are acquired. The lower the HPA the more accurate your position.

	![RTK GNSS Status Window](<img/QuickStart/SparkFun Torch - SW Maps GNSS Status SIV Small.png>)

	*Figure 5*

To improve the accuracy (down to 1.4cm), you now need to enable the PointPerfect corrections. Continue reading the [RTK Crash Course](#rtk-crash-course).

## RTK Crash Course

To get centimeter accuracy we need to provide the RTK unit with correction values. Corrections, often called RTCM, help the RTK unit refine its position calculations. RTCM (Radio Technical Commission for Maritime Services) can be obtained from a variety of sources but they fall into three buckets: Commercial, Public, and Civilian Reference Stations.

See [Corrections Sources](correction_sources.md) for a breakdown of the options and the pros and cons of each. For this quickstart, we'll be showing you how to enable PointPerfect corrections using your one month free subscription to the L-Band + IP service.

## PointPerfect Corrections

One of the great features of the RTK EVK is that it has the ability to get corrections from PointPerfect over Ethernet or WiFi. No need for NTRIP credentials! [Contact SparkFun](https://www.sparkfun.com/rtk_evk_registration) with your device ID, pay a monthly fee of $60 per month (as of this writing) and your device will obtain credentials and start receiving corrections anywhere there is coverage. $60 per month sounds like a lot, but this is a subscription to the premium PointPerfect L-Band + IP service. The subscription allows you to use IP-based corrections over Ethernet or WiFi, and L-Band corrections using the built-in NEO-D9S L-Band receiver. We really like u-blox's new Localized Distribution service where IP corrections are generated for your exact location, improving performance and minimising your network traffic.

[![PointPerfect Coverage map including L-Band and IP delivery methods](<img/PointPerfect/SparkFun RTK Everywhere - PointPerfect Coverage Map Small.png>)](https://www.u-blox.com/en/pointperfect-service-coverage)

*PointPerfect Coverage map including L-Band and IP delivery methods*

The PointPerfect IP service is available for various areas of the globe including the contiguous US, EU, South Korea, as well as parts of Brazil, Australia, and Canada. See the [coverage map](https://www.u-blox.com/en/pointperfect-service-coverage) for specifics; the RTK EVK is compatible with all areas as it supports both L-Band and IP coverage.

Steps to use PointPerfect:

1. [Register](https://www.sparkfun.com/rtk_evk_registration) the device with SparkFun by entering the device ID (this is the ID seen on the [printed stickers](https://docs.sparkfun.com/SparkFun_RTK_Everywhere_Firmware/menu_pointperfect/#registration) included in the kit). It can take up to two business days for registration to complete.

2. Put the RTK EVK into WiFi config mode by clicking the Mode button on the front panel. The first click opens the mode menu, successive clicks select the next menu option. Keep clicking until **Cfg WiFi** is highlighted, then do a quick double-click to select it.

	![SparkFun RTK EVK Mode Menu](<img/Displays/24342-RTK-EVK-Action-Screen_GIF_750ms.gif>)

	*SparkFun RTK EVK Mode Menu*

3. From your phone, connect to the WiFi network *RTK Config*. You should be redirected to the WiFi Config page. If you are not, open a browser (Chrome is preferred) and type **rtk.local** into the address bar. The IP Address will be **192.168.4.1**.

	![SparkFun RTK WiFi Configuration Interface](<img/WiFi Config/SparkFun RTK WiFi Config - Header Block.png>)

	*SparkFun RTK WiFi Configuration Interface*

4. Under the *WiFi Configuration* menu, give the device WiFi credentials for your local WiFi. This can be the cellphone hotspot if local WiFi is not available. If you will be using Ethernet, you can skip this step.

	![WiFi Menu containing one network](<img/WiFi Config/SparkFun RTK WiFi Config - WiFi Menu.png>)

	*WiFi Menu containing one network*

5. Under the [*PointPerfect Configuration* menu](menu_pointperfect.md), **Enable PointPefect Corrections** and select your **Geographic Region**. If desired, enable **Localized Corrections** and **AssistNow**.

	![PointPerfect Configuration Menu](<img/WiFi Config/SparkFun RTK PointPerfect Config.png>)

	*PointPerfect Configuration Menu*

	!!! note
		It is important that you set your Geographic Region correctly, via the menu or web config page, as this determines both the IP correction distribution topic and the L-Band frequency (on L-Band-capable products).

6. Click **Save Configuration**. The device will record all settings in a few seconds. Then press **Exit and Reset**. The unit will now reboot.

	![Saving and All Saved notifications](<img/WiFi Config/SparkFun RTK WiFi Config - Save Steps.png>)

	*Saving... then All Saved*

After the reboot, the device will connect to WiFi using your credentials. If you are using Ethernet instead, ensure the Ethernet cable is connected. The RTK will connect to PointPerfect, obtain keys, and begin applying corrections. Assuming your antenna is outside, after a few minutes of receiving PointPerfect corrections the device will enter RTK Float, then RTK Fix (usually under 3 minutes). Connect to the RTK EVK over SW Maps (or other) and view your position with millimeter accuracy!

![SW Maps showing accuracy](<img/SWMaps/SparkFun Torch - SW Maps PointPerfect Fix Accuracy.png>)

*SW Maps showing positional accuracy*

## Common Gotchas

* High-precision GNSS works best with a clear view of the sky; it does not work indoors or near a window. GNSS performance is generally *not* affected by clouds or storms. Trees and buildings *can* degrade performance but usually only in very thick canopies or very near tall building walls. GNSS reception is very possible in dense urban centers with skyscrapers but high-precision RTK may be impossible.

* The location reported by the RTK device is the location of the antenna element. Lat and Long are fairly easy to obtain but if you're capturing altitude be sure to do additional reading on ARPs (antenna reference points) and how to account for the antenna height in your data collection software.

* An internet connection is required for most types of RTK. RTCM corrections can be transmitted over other types of connections (such as serial telemetry radios). The RTK EVK also supports PointPerfect L-Band geostationary satellite corrections through the built-in NEO-D9S corrections receiver. The L-Band corrections are encrypted and keys are required but, once your unit has them, corrections will be available for up to eight weeks. See [Correction Transport](correction_transport.md) for more details.
