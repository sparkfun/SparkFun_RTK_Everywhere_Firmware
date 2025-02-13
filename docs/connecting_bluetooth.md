# Connecting Bluetooth

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


SparkFun RTK devices transmit full NMEA sentences over Bluetooth serial port profile (SPP) at 2Hz and 115200bps. This means that nearly any GIS application that can receive NMEA data over a serial port (almost all do) can be used with SparkFun RTK devices. As long as your end system can open a serial port over Bluetooth (also known as SPP) your system can retrieve industry-standard NMEA positional data. The following steps show how to connect an external tablet, or cell phone to the RTK device so that any serial port-based GIS application can be used.

!!! note
	BLE is also supported and can be used in place of Bluetooth SPP. See [Bluetooth Protocols](https://docs.sparkfun.com/SparkFun_RTK_Everywhere_Firmware/menu_system/#bluetooth-protocol) for more information.

## Android

<figure markdown>
![List of Bluetooth devices on Android](./img/QuickStart/SparkFun Torch - Available Devices.png)
<figcaption markdown>
Pairing with the 'Torch Rover-DFAE' over Bluetooth
</figcaption>
</figure>

Open Android's system settings and find the 'Bluetooth' or 'Connected devices' options. Scan for devices and pair with the device in the list that matches the Bluetooth MAC address on your RTK device.

When powered on, the RTK product will broadcast itself as either '[Platform] Rover-5556' or '[Platform] Base-5556' depending on which state it is in. [Platform] is Torch, Facet, etc. Discover and pair with this device from your phone or tablet. Once paired, open SW Maps.

<figure markdown>
![Bluetooth MAC address B022 is shown in the upper left corner](./img/Displays/SparkFun%20RTK%20Rover%20Display.png)
<figcaption markdown>
Bluetooth MAC address B022 is shown in the upper left corner
</figcaption>
</figure>

!!! note
	For devices with a built-in display, *B022* is the last four digits of your unit's MAC address and will be unique to the device in front of you. This is helpful in case there are multiple RTK devices within Bluetooth range.

### Enable Mock Location

Most GIS applications will gracefully handle the Bluetooth connection to the RTK device and provide an NTRIP Client for getting the RTCM corrections so this section can be skipped. If, in the rare case, a GIS app does not allow NTRIP corrections, Mock Locations can be enabled under Android. Then a data provider like Lefebure or GNSS Master can be used to act as a middle-man.

Before proceeding, it is recommended to have the mock location provider app already installed. So if you haven't already, consider installing [Lefebure](gis_software_android.md/#lefebure), [GNSS Master](gis_software_android.md/#gnss-master), etc.

To enable **Mock Locations**, *Developer Mode* in Android must be enabled. It is best to google the [most recent procedure for this](https://www.google.com/search?q=how+to+allow+mock+location+on+android) but the following procedure should work:

1. Open Android settings

	<figure markdown>
	![alt text](./img/MockLocation/SparkFun RTK Mock Location - Settings.png)
	<figcaption markdown>
	</figcaption>
	</figure>

2. Open *About phone*

	<figure markdown>
	![Build Number box](./img/MockLocation/SparkFun RTK Mock Location - Build Number.png)
	<figcaption markdown>
	</figcaption>
	</figure>

3. Scroll to the bottom and click on *Build number* five or more times. The device will prompt as more taps are required.

Once Developer Mode is enabled:

1. Open Android settings

	<figure markdown>
	![alt text](./img/MockLocation/SparkFun RTK Mock Location - Settings.png)
	<figcaption markdown>
	</figcaption>
	</figure>

2. Open *System*

	<figure markdown>
	![Develop options menu](./img/MockLocation/SparkFun RTK Mock Location - Developer Options.png)
	<figcaption markdown>
	</figcaption>
	</figure>

3. Open *Developer options*

	<figure markdown>
	![Mock Location button](./img/MockLocation/SparkFun RTK Mock Location - Select Mock Location App.png)
	<figcaption markdown>
	</figcaption>
	</figure>

4. Scroll all the way to the bottom of a very long list of developer options.

5. Select the app to use for Mock Location. This is usually [Lefebure](gis_software_android.md/#lefebure) or [GNSS Master](gis_software_android.md/#gnss-master) but can be tailored as needed.

## Apple iOS

Please see [iOS GIS Software](gis_software_ios.md) for information about how to connect to individual GIS apps. Some require a BLE connection and some require a WiFi hotspot connection.

More information is available on the [System Menu](menu_system.md) for switching between Bluetooth SPP and BLE.

## Windows

Open settings and navigate to Bluetooth. Click **Add device**.

<figure markdown>
![Adding Bluetooth Device](./img/Bluetooth/SparkFun%20RTK%20Software%20-%20Add%20Bluetooth%20Device.jpg)
<figcaption markdown>
Adding Bluetooth Device
</figcaption>
</figure>

Click Bluetooth 'Mice, Keyboards, ...'

<figure markdown>
![Viewing available Bluetooth Devices](./img/Bluetooth/SparkFun%20RTK%20Software%20-%20Add%20Bluetooth%20Device%202.jpg)
<figcaption markdown>
Viewing available Bluetooth Devices
</figcaption>
</figure>

Click on the RTK device. When powered on, the RTK product will broadcast itself as either '[Platform] Rover-5556' or '[Platform] Base-5556' depending on which state it is in. [Platform] is Facet, Express, Surveyor, etc. Discover and pair with this device from your phone or tablet. Once paired, open SW Maps.

<figure markdown>
![Bluetooth MAC address B022 is shown in the upper left corner](./img/Displays/SparkFun%20RTK%20Rover%20Display.png)
<figcaption markdown>
Bluetooth MAC address B022 is shown in the upper left corner
</figcaption>
</figure>

!!! note
	For devices with a built-in display, *B022* is the last four digits of your unit's MAC address and will be unique to the device in front of you. This is helpful in case there are multiple RTK devices within Bluetooth range.

<figure markdown>
![Bluetooth Connection Success](./img/Bluetooth/SparkFun%20RTK%20Software%20-%20Add%20Bluetooth%20Device%203.jpg)
<figcaption markdown>
Bluetooth Connection Success
</figcaption>
</figure>

The device will begin pairing. After a few seconds, Windows should report that you are ready to go.

<figure markdown>
![Bluetooth COM ports](./img/Bluetooth/SparkFun%20RTK%20Software%20-%20Add%20Bluetooth%20Device%204.jpg)
<figcaption markdown>
Bluetooth COM ports
</figcaption>
</figure>

The device is now paired and a series of COM ports will be added under 'Device Manager'.

<figure markdown>
![NMEA received over the Bluetooth COM port](./img/Terminal/SparkFun RTK Everywhere - NMEA Over Bluetooth.jpg)
<figcaption markdown>
NMEA received over the Bluetooth COM port
</figcaption>
</figure>

If necessary, you can open a terminal connection to one of the COM ports. Because the Bluetooth driver creates multiple COM ports, it's impossible to tell which is the serial stream so it's easiest to just try each port until you see a stream of NMEA sentences (shown above). You're all set! Be sure to close out the terminal window so that other software can use that COM port.
