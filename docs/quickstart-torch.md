# Quick Start - RTK Torch

![RTK Torch](img/SparkFun_RTK_Torch.png)

This quick start guide will get you started in 10 minutes or less. For the full product manual, please proceed to the [**Introduction**](index.md).

Are you using [Android](https://docs.sparkfun.com/SparkFun_RTK_Everywhere_Firmware/intro/#android) or [iOS](https://docs.sparkfun.com/SparkFun_RTK_Everywhere_Firmware/intro/#ios)?

## Android

1. Download [SW Maps](https://play.google.com/store/apps/details?id=np.com.softwel.swmaps). This may not be the GIS software you intend to do your data collection, but SW Maps is free and makes sure everything is working correctly out of the box.

    ![Download SW Maps](<img/SWMaps/SparkFun RTK SW Maps for Android QR Code.png>)

    *Download SW Maps for Android*

2. Mount the hardware:

    * For RTK Torch: Attach the Torch to a 5/8" 11-TPI standard surveying pole or to a [monopole](https://www.amazon.com/AmazonBasics-WT1003-67-Inch-Monopod/dp/B00FAYL1YU) using the included [thread adapter](https://www.sparkfun.com/products/17546) (Figure 1).

    ![RTK devices attached to a monopole](<img/SparkFun Torch Attached to a Pole.png>)

    *Figure 1*

3. Turn on the RTK Torch device by pressing the Power button for 3 to 4 seconds until a beep is heard and the two front LEDs illuminate (Figure 2). 

    [![RTK Torch Power Button Explanation](<img/RTK-Torch_Buttons_Front-Small.png>)](<img/RTK-Torch_Buttons_Front.png>)

    *Figure 2*

5. From your cell phone, open Bluetooth settings and pair it with a new device. You will see a list of available Bluetooth devices. Select the ‘Torch Rover-3AF1’. The '3AF1' is the last four digits of the device's MAC address and will vary depending on the device (Figure 4).

    ![List of Bluetooth devices on Android](<img/QuickStart/SparkFun Torch - Available Devices.png>)

    *Figure 4*

6. Once paired, open SW Maps. Select ‘New Project’ and give your project a name like ‘RTK Project’. 

7. Press the SW Maps icon in the top left corner of the home screen and select **Bluetooth GNSS**. You should see the Torch Rover-3AF1’ in the list. Select it. Confirm that the *Instrument Model* is **SparkFun RTK**, then press the ‘Connect’ button in the bottom left corner (Figure 5). SW Maps will show a warning that the instrument height is 0m. That’s ok. 

    ![SW Map list of Bluetooth devices](<img/QuickStart/SparkFun Torch - SW Maps Bluetooth Small.png>)

    *Figure 5*

8. Once connected, have a look at the Bluetooth LED on the RTK device. You should see the LED turn solid. You’re connected!

9. Now put the device outside with a clear view of the sky. GNSS doesn’t work indoors or near windows. Press the SW Maps icon in the top left corner of the home screen and select **GNSS Status**. Within about 30 seconds you should see 10 or more satellites in view (SIV) (Figure 6). More SIV is better. We regularly see 30 or more SIV. The horizontal positional accuracy (HPA) will start at around 10 meters and begin to decrease. The lower the HPA the more accurate your position. This accuracy is around 2m in normal mode.

    ![RTK GNSS Status Window](<img/QuickStart/SparkFun Torch - SW Maps GNSS Status SIV Small.png>)

    *Figure 6*

You can now use your RTK device to measure points with good (meter) accuracy. If you need extreme accuracy (down to 8mm) continue reading the [RTK Crash Course](https://docs.sparkfun.com/SparkFun_RTK_Everywhere_Firmware/intro/#rtk-crash-course).

## iOS

The software options for Apple iOS are much more limited because Apple products do not support Bluetooth SPP. That's ok! The SparkFun RTK products support Bluetooth Low Energy (BLE) which *does* work with iOS.

1. Download [SW Maps for iOS](https://apps.apple.com/us/app/sw-maps/id6444248083). This may not be the GIS software you intend to do your data collection, but SW Maps is free and makes sure everything is working correctly out of the box.

    ![SW Maps for Apple](<img/SWMaps/SparkFun RTK SW Maps for Apple QR Code.png>)

    *Download SW Maps for iOS*

2. Mount the hardware:

    * For RTK Torch: Attach the Torch to a 5/8" 11-TPI standard surveying pole or to a [monopole](https://www.amazon.com/AmazonBasics-WT1003-67-Inch-Monopod/dp/B00FAYL1YU) using the included [thread adapter](https://www.sparkfun.com/products/17546) (Figure 1).

    ![RTK devices attached to a monopole](<img/SparkFun Torch Attached to a Pole.png>)

    *Figure 1*

3. Turn on the RTK Torch device by pressing the Power button for 3 to 4 seconds until a beep is heard and the two front LEDs illuminate (Figure 2). 

    [![RTK Torch Power Button Explanation](<img/RTK-Torch_Buttons_Front-Small.png>)](<img/RTK-Torch_Buttons_Front.png>)

    *Figure 2*

4. Open SW Maps. Select ‘New Project’ and give your project a name like ‘RTK Project’. 

5. Press the SW Maps icon in the top left corner of the home screen and select Bluetooth GNSS. You will need to agree to allow a Bluetooth connection. Set the *Instrument Model* to **Generic NMEA (Bluetooth LE)**. Press 'Scan' and your RTK device should appear. Select it then press the ‘Connect’ button in the bottom left corner.

6. Once connected, have a look at the Bluetooth LED on the RTK device. You should see the LED turn solid. You’re connected!

7. Now put the device outside with a clear view of the sky. GNSS doesn’t work indoors or near windows.  Press the SW Maps icon in the top left corner of the home screen and select **GNSS Status**. Within about 30 seconds you should see 10 or more satellites in view (SIV) (Figure 3). More SIV is better. We regularly see 30 or more SIV. The horizontal positional accuracy (HPA) will start at around 10 meters and begin to decrease. The lower the HPA the more accurate your position. This accuracy is around 2m in normal mode.

    ![RTK GNSS Status Window](<img/QuickStart/SparkFun Torch - SW Maps GNSS Status SIV Small.png>)

    *Figure 3*

You can now use your RTK device to measure points with good (meter) accuracy. If you need extreme accuracy (down to 8mm) continue reading the [RTK Crash Course](https://docs.sparkfun.com/SparkFun_RTK_Everywhere_Firmware/intro/#rtk-crash-course).

## RTK Crash Course

To get millimeter accuracy we need to provide the RTK unit with correction values. Corrections, often called RTCM, help the RTK unit refine its position calculations. RTCM (Radio Technical Commission for Maritime Services) can be obtained from a variety of sources but they fall into three buckets: Commercial, Public, and Civilian Reference Stations.

See [Corrections Sources](correction_sources.md) for a breakdown of the options and the pros and cons of each. For this quickstart, we'll be showing two examples: using PointPerfect for $8 a month (a little less accurate but nation-wide coverage) and PointOne Nav for $50 a month (maximum accuracy, gaps in the coverage area).

## PointPerfect Corrections

One of the great features of the RTK Torch is that it has the ability to get corrections from PointPerfect over WiFi. No need for NTRIP credentials! [Contact SparkFun](https://www.sparkfun.com/rtk_torch_registration) with your device ID, pay a small monthly fee of $8 per month (as of this writing) and your device will obtain credentials and start receiving corrections anywhere there is coverage.

[![PointPerfect Coverage map including L-Band and IP delivery methods](<img/PointPerfect/SparkFun RTK Everywhere - PointPerfect Coverage Map Small.png>)](https://www.u-blox.com/en/pointperfect-service-coverage)

*PointPerfect Coverage map including L-Band and IP delivery methods*

The PointPerfect IP service is available for various areas of the globe including the contiguous US, EU, South Korea, as well as parts of Brazil, Australia, and Canada. See the [coverage map](https://www.u-blox.com/en/pointperfect-service-coverage) for specifics; the RTK Torch is compatible with any area that has *IP Coverage* (it is not compatible with L-Band coverage).

Steps to use PointPerfect:

1. [Register](https://www.sparkfun.com/rtk_torch_registration) the device with SparkFun by entering the device ID (this is the ID seen on the [printed stickers](https://docs.sparkfun.com/SparkFun_RTK_Everywhere_Firmware/menu_pointperfect/#registration) included in the kit). It can take up to two business days for registration to complete.
    
2. Power on the RTK Torch by pressing and holding the power button for around 4 seconds. The device will emit a short beep and illuminate the LEDs.
3. Put the RTK Torch into WiFi config mode by double-tapping the power button. You will hear two beeps indicating it is ready to connect to.

    ![SparkFun RTK WiFi Configuration Interface](<img/WiFi Config/SparkFun RTK WiFi Config - Header Block.png>)

    *SparkFun RTK WiFi Configuration Interface*

4. From your phone, connect to the WiFi network *RTK Config*. You should be redirected to the WiFi Config page. If you are not, open a browser (Chrome is preferred) and type **rtk.local** into the address bar.

    ![WiFi Menu containing one network](<img/WiFi Config/SparkFun RTK WiFi Config - WiFi Menu.png>)

    *WiFi Menu containing one network*

6. Under the *WiFi Configuration* menu, give the device WiFi credentials for your local WiFi. This can be the cellphone hotspot if local WiFi is not available.

    ![PointPerfect Configuration Menu](<img/WiFi Config/SparkFun RTK WiFi Config - PointPerfect Menu.png>)

    *PointPerfect Configuration Menu*

7. Under the *PointPerfect Configuration* menu, **Enable PointPefect Corrections**.

    ![Saving and All Saved notifications](<img/WiFi Config/SparkFun RTK WiFi Config - Save Steps.png>)

    *Saving... then All Saved*

8. Click **Save Configuration**. The device will record all settings in a few seconds. Then press **Exit and Reset**. The unit will now reboot.

![SW Maps showing accuracy](<img/SWMaps/SparkFun Torch - SW Maps PointPerfect Fix Accuracy.png>)

*SW Maps showing positional accuracy*

After the reboot, the device will connect to WiFi, obtain keys, and begin applying corrections. Assuming you are outside, after a few minutes of receiving PointPerfect corrections to the device, connect to the RTK Torch over SW Maps (or other) and the device will enter RTK Float, then RTK Fix (usually under 3 minutes). You can now take positional readings with millimeter accuracy!

## NTRIP Example

If you decide to use a service that provides NTRIP (as opposed to PointPerfect) we need to feed that data into your SparkFun RTK device. In this example, we will use PointOneNav and SW Maps.

1. Create an account on [PointOneNav](https://app.pointonenav.com/trial?src=sparkfun). **Note:** This service costs $50 per month at the time of writing.

2. Open SW Maps and connect to the RTK device over Bluetooth.

3. Once connected, open the SW Maps menu again (top left corner) and you will see a new option; click on ‘NTRIP Client'.

4. Enter the credentials provided by PointOneNav and click Connect (Figure 1). Verify that *Send NMEA GGA* is checked.

    ![NTRIP credentials in SW Maps](<img/SWMaps/SparkFun RTK SW Maps - NTRIP Credentials.png>)

    *Figure 1*

5. Corrections will be downloaded every second from PointOneNav using your phone’s cellular connection and then sent down to the RTK device over Bluetooth. You don't need a very fast internet connection or a lot of data; it's only about 530 bytes per second.

Assuming you are outside, as soon as corrections are sent to the device, the bubble in SW Maps will turn Orange (RTK Float). Once RTK Fix is achieved (usually under 30 seconds) the bubble will turn Green and the HPA will be below 20mm (Figure 2). You can now take positional readings with millimeter accuracy!

![Double crosshair indicating RTK Fix](<img/SWMaps/SparkFun Torch - SW Maps GNSS Status RTK Fix HPA Small.png>)

*Figure 2*

In SW Maps, the position bubble will turn from Blue (regular GNSS fix), then to Orange (RTK Float), then to Green (RTK Fix) (Figure 3).

![Green bubble indicating RTK Fix](<img/SWMaps/SparkFun RTK SW Maps - Green Bubble-1.png>)

*Figure 3*

RTK Fix will be maintained as long as there is a clear view of the sky and corrections are delivered to the device every few seconds.

## Common Gotchas

* High-precision GNSS works best with a clear view of the sky; it does not work indoors or near a window. GNSS performance is generally *not* affected by clouds or storms. Trees and buildings *can* degrade performance but usually only in very thick canopies or very near tall building walls. GNSS reception is very possible in dense urban centers with skyscrapers but high-precision RTK may be impossible.

* The location reported by the RTK device is the location of the antenna element; it's *not* the location of the pointy end of the stick. Lat and Long are fairly easy to obtain but if you're capturing altitude be sure to do additional reading on ARPs (antenna reference points) and how to account for the antenna height in your data collection software. The Torch ARP is [here](https://docs.sparkfun.com/SparkFun_RTK_Torch/hardware_overview/#antenna-reference-point). Note: This rule does not apply when tilt compensation is activated. See the [Tilt Compensation Menu](menu_tilt.md) for more information.

* An internet connection is required for most types of RTK. RTCM corrections can be transmitted over other types of connections (such as serial telemetry radios). See [Correction Transport](correction_transport.md) for more details.
