# Base Menu

Torch: ![Feature Supported](img/Icons/GreenDot.png)

In addition to providing accurate local location fixes, SparkFun RTK devices can also serve as a correction source, also called a *Base*. The Base doesn't move and 'knows' where it is so it can calculate the discrepancies between the signals it is receiving and what it should be receiving. Said differently, the 'Base' is told where it is, and that it's not moving. If the GPS signals say otherwise, the Base knows there was a disturbance in the ~~Force~~ ionosphere. These differences are the correction values passed to the Rover so that the Rover can have millimeter-level accuracy.

There are two types of bases: *Surveyed* and *Fixed*. A surveyed base is often a temporary base set up in the field. Called a 'Survey-In', this is less accurate but requires only 60 seconds to complete. The 'Fixed' base is much more accurate but the precise location at which the antenna is located must be known. A fixed base is often a structure with an antenna bolted to the side. Raw satellite signals are gathered for a few hours and then processed using Precision Point Position. We have a variety of tutorials that go into depth on these subjects but all you need to know is that the RTK Facet supports both Survey-In and Fixed Base techniques.

Please see the following tutorials for more information:

<table class="table table-hover table-striped table-bordered">
  <tr align="center">
   <td><a href="https://learn.sparkfun.com/tutorials/what-is-gps-rtk"><img src="https://cdn.sparkfun.com/c/178-100/assets/learn_tutorials/8/1/3/Location-Wandering-GPS-combined.jpg"></a></td>
   <td><a href="https://learn.sparkfun.com/tutorials/getting-started-with-u-center-for-u-blox"><img src="https://cdn.sparkfun.com/c/178-100/assets/learn_tutorials/8/1/5/u-center.jpg"></a></td>
   <td><a href="https://learn.sparkfun.com/tutorials/setting-up-a-rover-base-rtk-system"><img src="https://cdn.sparkfun.com/c/178-100/assets/learn_tutorials/1/3/6/2/GNSS_RTK_DIY_Surveying_Tutorial.jpg"></a></td>
   <td><a href="https://learn.sparkfun.com/tutorials/how-to-build-a-diy-gnss-reference-station"><img src="https://cdn.sparkfun.com/c/178-100/assets/learn_tutorials/1/3/6/3/Roof_Enclosure.jpg"></a></td>
  </tr>
  <tr align="center">
    <td><a href="https://learn.sparkfun.com/tutorials/what-is-gps-rtk">What is GPS RTK?</a></td>
    <td><a href="https://learn.sparkfun.com/tutorials/getting-started-with-u-center-for-u-blox">Getting Started with u-center for u-blox</a></td>
    <td><a href="https://learn.sparkfun.com/tutorials/setting-up-a-rover-base-rtk-system">Setting up a Rover Base RTK System</a></td>
    <td><a href="https://learn.sparkfun.com/tutorials/how-to-build-a-diy-gnss-reference-station">How to build a DIY GNSS reference station</a></td>
  </tr>
</table>

## Mode

The Base Menu allows the user to select between Survey-In or Fixed Base setups.

![Serial menu showing Base options](<img/Terminal/SparkFun RTK Everywhere - Base Menu Survey-In.png>)

*Base Menu showing Survey-In Mode*

In **Survey-In** mode, the minimum observation time can be set. The default is 60 seconds. The device will wait for the position accuracy to be better than 1 meter before a Survey-In is started. Don't be fooled; setting the observation time to 4 hours is not going to significantly improve the accuracy of the survey - use [PPP](https://learn.sparkfun.com/tutorials/how-to-build-a-diy-gnss-reference-station#gather-raw-gnss-data) instead.

![Fixed Base Coordinate input](<img/Terminal/SparkFun RTK Everywhere - Base Menu Fixed ECEF.png>)

*Base Menu showing Fixed Base Mode with ECEF Coordinates*

In **Fixed** mode, the coordinates of the antenna need to be set. These can be entered in ECEF or Geographic coordinates. 

Once the device has been configured, a user enters Base mode by changing the mode in the [System Menu](menu_system.md).

If the device is configured for *Survey-In* base mode, the survey will begin. The mean standard deviation will be printed as well as the time elapsed. For most Survey-In setups, the survey will complete in around 60 seconds.

In *Fixed Base* mode the GNSS receiver will go into Base mode with the defined coordinates and immediately begin outputting RTCM correction data.

## NTRIP Server

**NTRIP** is where the real fun begins. The Base needs a method for getting the correction data to the Rover. This can be done using radios but that's limited to a few kilometers at best. If you've got WiFi reception, use the internet!

Enabling NTRIP will present a handful of new options seen below:

![NTRIP Server Settings](<img/Terminal/SparkFun RTK Everywhere - Base Menu Fixed Geodetic NTRIP Server.png>)

*Settings for the NTRIP Servers*

This is a powerful feature of the RTK line of products. The RTK device can be configured to transmit its RTCM directly over WiFi to up to **4 Casters**. This eliminates the need for a radio link between one Base and one Rover. Providing more than one caster is a unique RTK Everywhere feature that allows a single base installation to push corrections to a public Caster (such as RTK2Go) as well as payment-generating casters (such as [Onocoy](https://www.onocoy.com/) or [Geodnet](https://geodnet.com/)).

Once the NTRIP server is enabled you will need a handful of credentials:

* Local WiFi SSID and password
* A casting service such as [RTK2Go](http://www.rtk2go.com) or [Emlid](http://caster.emlid.com) (the port is almost always 2101)
* A mount point (required) and password (required)

If the NTRIP server is enabled the device will first attempt to connect to  WiFi. Once WiFi connects the device will attempt to connect to the NTRIP mount point. Once connected, every second a few hundred bytes, up to ~2k, will be transmitted to your mount point.

The RTK device will monitor each NTRIP Server connection and automatically attempt to restart it if WiFi or if the Caster is disconnected.

## Commonly Use Coordinates

![List of common coordinates](<img/WiFi Config/SparkFun%20RTK%20Base%20Configure%20-%20Commonly%20Used%20Points%20Menu.png>)

*A list of common coordinates*

For users who return to the same base position or monument, the coordinates can be saved to a 'Commonly Used Coordinates' list. A nickname and the X/Y/Z positions are saved to the list. Any record on the list can be loaded from the list into the X/Y/Z fields allowing quick switching without the need to hand record or re-enter coordinates from day-to-day repositioning of the base.

## RTCM Message Rates

![The RTCM Menu under Base](<img/WiFi Config/SparkFun%20RTK%20Base%20Survey%20In.png>)

When the device is in Base mode, the fix rate is set to 1Hz. This will override any Rover setting. 

![The list of supported RTCM messages](<img/WiFi Config/SparkFun%20RTK%20-%20Base%20RTCM%20Rates%20Menu.png>)

Additionally, RTCM messages are generated at a rate of 1Hz. If lower RTCM rates are desired the RTCM Rates menu can be used to modify the rates of any supported RTCM message. This can be helpful when using longer-range radios that have lower bandwidth.

## Supported Lat/Long Coordinate Formats

![Entering coordinates in alternate formats](<img/WiFi Config/SparkFun%20RTK%20-%20Alternate%20Coordinate%20Types%20for%20Fixed%20Base.png>)

When entering coordinates for a fixed Base in Geodetic format, the following formats are supported:

* DD.ddddddddd (ie -105.184774720, 40.090335429)
* DDMM.mmmmmmm (ie -10511.0864832)
* DD MM.mmmmmmm (ie 40 05.42013)
* DD-MM.mmmmmmm (40-05.42013)
* DDMMSS.ssssss (-1051105.188992)
* DD MM SS.ssssss (-105 11 05.188992)
* DD-MM-SS.ssssss (40-05-25.2075)

![Coordinate formats in the Base serial menu](<img/Terminal/SparkFun RTK Everywhere - Base Menu Alternate Coordinate Format.png>)

*Coordinates shown in DD MM SS.ssssss format*

These coordinate formats are automatically detected and converted as needed. The coordinates are displayed in the format they are entered. If a different format is desired, the coordinate display format can be changed via the serial Base menu.

For more information about coordinate formats, check out this [online converter](https://www.earthpoint.us/convert.aspx).

## Assisted Base

An Assisted Base is where a temporary base is set up to Survey-In its location but is simultaneously provided RTCM corrections so that its Survey-In is done with very precise readings. An assisted base running a Survey-In removes much of the relative inaccuracies from a Rover-Base system. We've found an Assisted Base varies as little as 50mm RMS between intra-day tests, with accuracy within 65mm of a PPP of the same location, same day.

To set up an assisted base the RTK device should be located in a good reception area and provided with RTCM corrections. Let it obtain RTK Fix from a fixed position (on a tripod, for example) in *Rover* mode. Once an RTK fix is achieved, change the device to temporary *Base* mode (also called Survey-In). The device will take 60 seconds of positional readings, at which point the fixed position of the base will be set using RTK augmented coordinates. At this point, corrections provided to the base can be discontinued. The Base will begin outputting very accurate RTCM corrections that can be relayed to a rover that is in a less optimal reception setting.

Similarly, the RTK Facet L-Band can be set up as a relay: the L-Band device can be located in a good reception area, and then transmit very accurate corrections to a rover via Radio or internet link. Because the RTK Facet L-Band can generate its own corrections, you do not need to provide them during Survey-In. To set up an assisted base, set up an RTK Facet L-Band unit with a clear view of the sky, and let it obtain RTK Fix from a fixed position in *Rover* mode. Once an RTK fix is achieved, change the device to temporary *Base* mode. The device will take 60 seconds of positional readings, at which point the fixed position will be set using RTK fixed coordinates. The RTK Facet L-Band will then output very accurate RTCM corrections that can be relayed to a rover that is in a less optimal reception setting.
