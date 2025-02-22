# Windows

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

There are a variety of 3rd party apps available for GIS and surveying for [Android](https://docs.sparkfun.com/SparkFun_RTK_Everywhere_Firmware/gis_software_android/), [iOS](https://docs.sparkfun.com/SparkFun_RTK_Everywhere_Firmware/gis_software_ios/), and [Windows](https://docs.sparkfun.com/SparkFun_RTK_Everywhere_Firmware/gis_software_windows/). We will cover a few examples below that should give you an idea of how to get the incoming NMEA data into the software of your choice.

## QGIS

QGIS is a free and open-source geographic information system software for desktops. It's available [here](https://qgis.org/).

Once the software is installed open QGIS Desktop.

<figure markdown>
![View Menu](./img/QGIS/SparkFun%20RTK%20QGIS%20-%20View%20Menu.png)
<figcaption markdown>
</figcaption>
</figure>

Open the View Menu, then look for the 'Panels' submenu.

<figure markdown>
![Panels submenu](./img/QGIS/SparkFun%20RTK%20QGIS%20-%20Enable%20GPS%20Info%20Panel.png)
<figcaption markdown>
</figcaption>
</figure>

From the Panels submenu, enable 'GPS Information'. This will show a new panel on the left side.

At this point, you will need to enable *TCP Server* mode on your RTK device from the [WiFi Config menu](menu_wifi.md). Once the RTK device is connected to local WiFi QGIS will be able to connect to the given IP address and TCP port.

<figure markdown>
![Select GPSD](./img/QGIS/SparkFun%20RTK%20QGIS%20-%20GPS%20Panel.png)
<figcaption markdown>
</figcaption>
</figure>

Above: From the subpanel, select 'gpsd'.

<figure markdown>
![Entering gpsd specifics](./img/QGIS/SparkFun%20RTK%20QGIS%20-%20GPS%20Panel%20Entering%20IP%20and%20port.png)
<figcaption markdown>
</figcaption>
</figure>

Enter the IP address of your RTK device. This can be found by opening a serial connection to the device. The IP address will be displayed every few seconds. Enter the TCP port to use. By default an RTK device uses 2947.

Press 'Connect'.

<figure markdown>
![Viewing location in QGIS](./img/QGIS/SparkFun%20RTK%20QGIS%20-%20Location%20on%20Map.png)
<figcaption markdown>
</figcaption>
</figure>

The device location will be shown on the map. To see a map, be sure to enable OpenStreetMap under the XYZ Tiles on the Browser.

<figure markdown>
![Connecting over Serial](./img/QGIS/SparkFun%20RTK%20QGIS%20-%20Direct%20Serial%20Connection.png)
<figcaption markdown>
</figcaption>
</figure>

Alternatively, a direct serial connection to the RTK device can be obtained. Use a USB cable to connect to the RTK device. See [Output GNSS Data over USB](https://docs.sparkfun.com/SparkFun_RTK_Everywhere_Firmware/menu_ports/#output-gnss-data-over-usb) for more information.

## Other GIS Packages

Hopefully, these examples give you an idea of how to connect the RTK product line to most any GIS software. If there is other GIS software that you'd like to see configuration information about, please open an issue on the [RTK Firmware repo](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/issues) and we'll add it.

## What's an NTRIP Caster?

In a nutshell, it's a server that is sending out correction data every second. There are thousands of sites around the globe that calculate the perturbations in the ionosphere and troposphere that decrease the accuracy of GNSS accuracy. Once the inaccuracies are known, correction values are encoded into data packets in the RTCM format. You, the user, don't need to know how to decode or deal with RTCM, you simply need to get RTCM from a source within 10km of your location into the RTK device. The NTRIP client logs into the server (also known as the NTRIP caster) and grabs that data, every second, and sends it over Bluetooth to the RTK device.

## Where do I get RTK Corrections?

Be sure to see [Correction Sources](correction_sources.md).

Don't have access to an NTRIP Caster or other RTCM correction source? There are a few options.

The [SparkFun RTK Facet L-Band](https://www.sparkfun.com/products/20000) gets corrections via an encrypted signal from geosynchronous satellites. This device gets RTK Fix without the need for a WiFi or cellular connection.

Also, you can use a 2nd RTK product operating in Base mode to provide the correction data. Check out [Creating a Permanent Base](permanent_base.md).

If you're the DIY sort, you can create your own low-cost base station using an ESP32 and a ZED-F9P breakout board. Check out [How to Build a DIY GNSS Reference Station](https://learn.sparkfun.com/tutorials/how-to-build-a-diy-gnss-reference-station).

There are services available as well. [Syklark](https://www.swiftnav.com/skylark) provides RTCM coverage for $49 a month (as of writing) and is extremely easy to set up and use. [Point One](https://app.pointonenav.com/trial?utm_source=sparkfun) also offers RTK NTRIP service with a free 14 day trial and easy to use front end.
