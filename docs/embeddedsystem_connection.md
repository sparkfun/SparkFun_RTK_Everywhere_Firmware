# Output to an Embedded System

Many applications using the RTK products will use a 3rd party GIS application or mobile app like SW Maps and receive the data over Bluetooth. For RTK devices that have an external connection, a user can obtain the NMEA data over serial directly.

## USB

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

Most SparkFun RTK devices have a USB port for programming and status reports. This USB port can also be configured to output full NMEA/RTCM output that can be used to create a wired connection to an embedded system. See [Output GNSS Data over USB](menu_ports.md#output-gnss-data-over-usb) for more information.

## DATA Port

<!--
Compatibility Icons
====================================================================================

:material-radiobox-marked:{ .support-full title="Feature Supported" }
:material-radiobox-indeterminate-variant:{ .support-partial title="Feature Partially Supported" }
:material-radiobox-blank:{ .support-none title="Feature Not Supported" }
-->

<div class="grid cards fill" markdown>

- EVK: :material-radiobox-blank:{ .support-none title="Feature Not Supported" }
- Facet mosaic: :material-radiobox-marked:{ .support-full title="Feature Supported" }
- Postcard: [:material-radiobox-blank:{ .support-full }]( title ="Feature Supported" )
- Torch: :material-radiobox-blank:{ .support-none title="Feature Not Supported" }

</div>

For this example, we will connect the output from the **DATA** port of an RTK Facet to a [USB to Serial adapter](https://www.sparkfun.com/products/15096) so that we can view the serial data over a terminal connection.

The **DATA** port on the RTK Facet can be configured to output a variety of different signals including NMEA Serial data. Be sure to check out the [Ports Menu](menu_ports.md) section to be sure your device is configured to output NMEA.

Connect the included [4-pin JST to breadboard cable](https://www.sparkfun.com/products/17240) to the **DATA** port. The cable has the following pinout:

- **Red** - 3.3V
- **Green** - TX (output from the RTK device)
- **Orange** - RX (input into the RTK device)
- **Black** - GND

<figure markdown>
![Wires connected to a SparkFun USB C to Serial adapter](./img/SparkFun_RTK_Facet_-_Data_Port_to_USB.jpg)
<figcaption markdown>
</figcaption>
</figure>

[Open a terminal](https://learn.sparkfun.com/tutorials/terminal-basics) at 115200bps and you should see NMEA sentences:

<figure markdown>
![NMEA output from the RTK Surveyor](./img/Terminal/SparkFun_RTK_Surveyor_-_Data_Output.jpg)
<figcaption markdown>
</figcaption>
</figure>

The Data connector on all RTK products is a 4-pin locking 1.25mm JST SMD connector (part#: SM04B-GHS-TB, mating connector part#: GHR-04V-S). **3.3V** is provided by this connector to power a remote device if needed. While the port is capable of sourcing up to 600mA, we do not recommend more than 300mA. This port should not be connected to a power source, so if your embedded device has its own power do not connect the red wire.

!!! warning
	All data in and out of RTK products is **3.3V**. Exposing these pins to **5V** or higher voltage logic will damage the device.

The parsing of NMEA sentences is straightforward and left to the reader. There are ample NMEA parsing libraries available in C++, Arduino, Python, and many more languages.

## External Terminals

<!--
Compatibility Icons
====================================================================================

:material-radiobox-marked:{ .support-full title="Feature Supported" }
:material-radiobox-indeterminate-variant:{ .support-partial title="Feature Partially Supported" }
:material-radiobox-blank:{ .support-none title="Feature Not Supported" }
-->

<div class="grid cards fill" markdown>

- EVK: :material-radiobox-marked:{ .support-full title="Feature Supported" }
- Facet mosaic: :material-radiobox-blank:{ .support-none title="Feature Not Supported" }
- Postcard: :material-radiobox-blank:{ .support-none title="Feature Not Supported" }
- Torch: :material-radiobox-blank:{ .support-none title="Feature Not Supported" }

</div>

On devices that support it, external screw terminals provide direct access to the GNSS TX and RX signals via the 3.5mm screw terminal I/O header:

<figure markdown>
![Reference Station I/O screw terminals](./img/SparkFun_GNSS_RTK_Reference_Station_IO.jpg)
<figcaption markdown>
</figcaption>
</figure>

- TX2 : u-blox ZED-F9P UART2 transmit: 3.3V OUTPUT
- RX2 : u-blox ZED-F9P UART2 receive: 3.3V INPUT

Please see the [EVK Hookup Guide](https://docs.sparkfun.com/SparkFun_RTK_EVK/hardware_overview/#zed-f9p-secondary-uart-port) for more details.
