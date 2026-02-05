# TCP/UDP Menu

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

NMEA data is generally consumed by a GIS application or Data Collector. These messages can be transmitted over a variety of transport methods. This section focuses on the delivery of NMEA messages via TCP and UDP.

<figure markdown>

![TCP/UDP Menu showing various Client and Server options](<./img/Terminal/SparkFun RTK Everywhere - TCP-UDP Menu.png>)
<figcaption markdown>
TCP/UDP Menu showing various Client and Server options
</figcaption>
</figure>

<figure markdown>

![TCP/UDP settings using Web Config](<./img/WiFi Config/SparkFun RTK Config - TCP Port.png>)
<figcaption markdown>
TCP/UDP settings using Web Config
</figcaption>
</figure>

## TCP Client and Server

The RTK device supports connection over TCP. The TCP Client sits on top of the network layer (WiFi in general, and Ethernet/Cellular on the EVK) and sends position data to one or more computers or cell phones for display. Some Data Collector software (such as [Vespucci](gis_software_android.md#vespucci)) requires that the SparkFun RTK device connect as a TCP Client. Other software (such as [QGIS](gis_software_windows.md#qgis)) requires that the SparkFun RTK device acts as a TCP Server. Both are supported.

If either Client or Server is enabled, a port can be designated. By default, the port is 2948 (registered as [*GPS Daemon request/response*](https://tcp-udp-ports.com/port-2948.htm)) but any port 0 to 65535 is supported.

<figure markdown>

![Ethernet TCP Client connection](<./img/Terminal/TCP_Client.gif>)
<figcaption markdown>
Ethernet TCP Client connection
</figcaption>
</figure>


The above animation was generated using [TCP_Server.py](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/blob/main/Firmware/Tools/TCP_Server.py) and demonstrates connecting to an RTK device over TCP from a desktop.

## UDP Server

NMEA messages can also be broadcast via UDP rather than TCP. If enabled, the UDP Server will begin broadcasting NMEA data over the specific port (default 10110).

## NTRIP Caster

If enabled, incoming TCP connections will be answered as if the device is an NTRIP Caster and return a mount table with only one mount point. In this mode NTRIP Clients can connect directly to the RTK device without the need for a 3rd party NTRIP Caster such as RTK2Go or Emlid. This is most often used when a base RTK device is in AP or WiFi Station mode, and a rover is connected to a cell phone that is on the same WiFi network. A GIS app can then use its built in NTRIP Client to connect to the base's NTRIP Caster.

## MDNS

Multicast DNS allows other devices on the same network to discover this device using 'rtk.local' as its address. When MDNS is enabled, and the RTK device is acting in WiFi AP mode for configuration, typing 'rtk.local' into a browser will connect that browser to the RTK device without having to know or type in the full IP address.

## Broadcast TCP/UDP over WiFi or AP

TCP Server and UDP Server can connect over a local WiFi network, or by becoming an Access Point (hosting its own WiFi network) that client devices can connect to. Use this option to select between the type of WiFi to use for TCP/UDP Server broadcasting.
