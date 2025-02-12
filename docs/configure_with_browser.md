# Configure with Browser

<!--
Compatibility Icons
====================================================================================

:material-radiobox-marked:{ .support-full title="Feature Supported" }
:material-radiobox-indeterminate-variant:{ .support-partial title="Feature Partially Supported" }
:material-radiobox-blank:{ .support-none title="Feature Not Supported" }
-->

<div class="grid cards fill" markdown>

- EVK: :material-radiobox-marked:{ .support-full title="Feature Supported" }
- Postcard: :material-radiobox-marked:{ .support-full title="Feature Supported" }
- Torch: :material-radiobox-marked:{ .support-full title="Feature Supported" }

</div>

<figure markdown>
![SparkFun RTK WiFi Configuration Interface](./img/WiFi Config/SparkFun RTK WiFi Config - Main Interface.png)
<figcaption markdown>
SparkFun RTK WiFi Configuration Interface
</figcaption>
</figure>

During browser configuration, the RTK device will present a webpage that is viewable from a desktop, laptop, or cell phone. This is available over WiFi for all devices and ethernet for some (RTK EVK only). For advanced configurations, a desktop is recommended. For quick in-field changes, a cell phone works great.

<figure markdown>
![Desktop vs Phone display size configuration](./img/WiFi Config/SparkFun_RTK_Facet_-_Desktop_vs_Phone_Config.jpg)
<figcaption markdown>
Desktop vs Phone display size configuration
</figcaption>
</figure>

## RTK EVK

To get into browser configuration follow these steps:

1. Ensure that an ethernet cable is plugged in and/or the WiFi / Bluetooth antenna is attached, then connect the RTK EVK to a power source.

	<figure markdown>
	![SparkFun RTK EVK Mode Menu](./img/Displays/SparkFun_RTK_Postcard_Config.png)
	<figcaption markdown>
	SparkFun RTK EVK Mode Menu
	</figcaption>
	</figure>

2. Once the device has started, put the RTK EVK into WiFi config mode by clicking the Mode button on the front panel. The first click opens the mode menu, successive clicks select the next menu option. Keep clicking until **Config** is highlighted, then do a quick double-click to select it.

3. The display will change, showing that the EVK is in configuration mode.

4. If connecting over ethernet, open a browser and connect to the IP address shown on the display.

	<figure markdown>
	![Browser with rtk.local address](./img/WiFi Config/SparkFun RTK WiFi Config - Browser rtk local.png)
	<figcaption markdown>
	Browser with rtk.local
	</figcaption>
	</figure>
	
5. If connecting over WiFi, then connect to WiFi network named ‘RTK Config’. You should be automatically re-directed to the config page but if you are not, open a browser (Chrome is preferred) and type **rtk.local** into the address bar.

!!! note
	Upon connecting, your phone may warn you that this WiFi network has no internet. That's ok. Stay connected to the network and open a browser. If you still have problems turn off Mobile Data so that the phone does not default to cellular for internet connectivity and instead connects to the RTK Device.

## RTK Postcard

To get into browser configuration follow these steps:

1. Connect the RTK Postcard to a power source.

	<figure markdown>
	![SparkFun RTK EVK Mode Menu](./img/Displays/SparkFun_RTK_Postcard_Config.png)
	<figcaption markdown>
	SparkFun RTK EVK Mode Menu
	</figcaption>
	</figure>

2. Once the device has started, put the RTK Postcard into browser config mode by clicking down on the directional pad. The first click opens the mode menu, successive clicks select the next menu option. Keep clicking until **Config** is highlighted, then press the center of the directional pad to select it.

3. The display will change, showing that the device is in configuration mode.

4. Connect to the WiFi network named ‘RTK Config’.

!!! note
	Upon connecting, your phone may warn you that this WiFi network has no internet. That's ok. Stay connected to the network and open a browser. If you still have problems turn off Mobile Data so that the phone does not default to cellular for internet connectivity and instead connects to the RTK Device.


	<figure markdown>
	![Browser with rtk.local address](./img/WiFi Config/SparkFun RTK WiFi Config - Browser rtk local.png)
	<figcaption markdown>
	Browser with rtk.local
	</figcaption>
	</figure>

5. You should be automatically re-directed to the config page but if you are not, open a browser (Chrome is preferred) and type **rtk.local** into the address bar.

## RTK Torch

To get into browser configuration follow these steps:

1. Power on the RTK Torch
2. Once the device has started press the Power Button twice within 1 second (double tap).
3. The device will beep twice indicating it is waiting for incoming connections.

4. Connect to WiFi network named ‘RTK Config’.

!!! note
	Upon connecting, your phone may warn you that this WiFi network has no internet. That's ok. Stay connected to the network and open a browser. If you still have problems turn off Mobile Data so that the phone does not default to cellular for internet connectivity and instead connects to the RTK Device.

<figure markdown>
![Browser with rtk.local address](./img/WiFi Config/SparkFun RTK WiFi Config - Browser rtk local.png)
<figcaption markdown>
Browser with rtk.local
</figcaption>
</figure>

5. You should be automatically re-directed to the config page but if you are not, open a browser (Chrome is preferred) and type **rtk.local** into the address bar.

Continue with [Connecting to WiFi network](#connecting-to-wifi-network).

## Connecting to WiFi Network

<figure markdown>
![Discovered WiFi networks](./img/WiFi Config/RTK_Surveyor_-_WiFi_Config_-_Networks.jpg)
<figcaption markdown>
The WiFi network RTK Config as seen from a cellphone
</figcaption>
</figure>

!!! note
	Upon connecting, your phone may warn you that this WiFi network has no internet. That's ok. Stay connected to the network and open a browser. If you still have problems turn off Mobile Data so that the phone does not default to cellular for internet connectivity and instead connects to the RTK Device.

<figure markdown>
![Webpage showing the RTK Configuration options](./img/WiFi Config/SparkFun RTK WiFi Config - Main Interface.png)
<figcaption markdown>
Connected to the RTK WiFi Setup Page
</figcaption>
</figure>

Clicking on the category tab will open or close that section. Clicking on an ‘i’ will give you a brief description of the options within that section.

<figure markdown>
![Firmware highlighted](./img/WiFi Config/SparkFun RTK WiFi Config - Header Firmware Version.png)
<figcaption markdown>
This unit has firmware version 1.0 and a UM980 GNSS receiver
</figcaption>
</figure>

The firmware for the RTK device and the firmware for the GNSS receiver is shown at the top of the page. This can be helpful when troubleshooting or requesting new features.

## File Manager

<figure markdown>
![List of files in file manager](./img/WiFi Config/SparkFun%20RTK%20WiFi%20Config%20File%20Manager.png)
<figcaption markdown>
</figcaption>
</figure>

On devices that support an external SD card, a file manager is shown if an SD card is detected. This is a handy way to download files to a local device (cell phone or laptop) as well as delete any unneeded files. The SD size and free space are shown. Files may be uploaded to the SD card if needed.

Additionally, clicking on the top checkbox will select all files for easy removal of a large number of files.

## Saving and Exit

<figure markdown>
![Save and Exit buttons](./img/WiFi Config/SparkFun RTK WiFi Config - Save Steps.png)
<figcaption markdown>
</figcaption>
</figure>

Once settings are input, please press ‘Save Configuration’. This will validate any settings, show any errors that need adjustment, and send the settings to the unit. The page will remain active until the user presses ‘Exit and Reset’ at which point the unit will exit WiFi configuration and return to whichever mode was selected in the **System Configuration** tab **System Initial State** drop-down (Base, Rover or NTP).
