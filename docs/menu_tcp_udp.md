# TCP/UDP Menu

Torch: ![Feature Supported](img/Icons/GreenDot.png) 

NMEA data is generally consumed by a GIS application or Data Collector. These messages can be transmitted over a variety of transport methods. This section focuses on the delivery of NMEA messages via TCP and UDP.

  ![TCP/UDP Menu showing various TCP/UDP Client and Server options](<img/Terminal/SparkFun RTK Everywhere - TCP-UDP Menu.png>)

*TCP/UDP Menu showing various Client and Server options*

## TCP Client and Server

The RTK device supports connection over TCP. The TCP Client sits on top of the network layer and sends position data to one or more computers or cell phones for display. Some Data Collector software (such as [Vespucci](gis_software.md#vespucci)) requires that the SparkFun RTK device connect as a TCP Client. Other software (such as [QGIS](gis_software.md#qgis)) requires that the SparkFun RTK device acts as a TCP Server. Both are supported.

**Note:** Currently TCP is only supported while connected to local WiFi, not AP mode. This means the device will need to be connected to a WiFi network, such as a mobile hotspot, before TCP connections can occur.

![TCP Port Entry](img/WiFi%20Config/SparkFun%20RTK%20Config%20-%20TCP%20Port.png)

If either Client or Server is enabled, a port can be designated. By default, the port is 2947 (registered as [*GPS Daemon request/response*](https://tcp-udp-ports.com/port-2948.htm)) but any port 0 to 65535 is supported.

## UDP Server

PVT can also be sent via UDP rather than TCP. If enabled, the UDP Server will begin broadcasting NMEA data over the specific port (default 10110).
