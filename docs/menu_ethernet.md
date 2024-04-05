# Ethernet Menu

## TCP Client and Server

The RTK device supports connection over TCP. Some Data Collector software (such as [Vespucci](gis_software.md#vespucci)) requires that the SparkFun RTK device connect as a TCP Client. Other software (such as [QGIS](gis_software.md#qgis)) requires that the SparkFun RTK device acts as a TCP Server. Both are supported.

**Note:** Currently TCP is only supported while connected to local WiFi, not AP mode. This means the device will need to be connected to a WiFi network, such as a mobile hotspot, before TCP connections can occur.

![TCP Port Entry](img/WiFi%20Config/SparkFun%20RTK%20Config%20-%20TCP%20Port.png)

If either Client or Server is enabled, a port can be designated. By default, the port is 2947 (registered as [*GPS Daemon request/response*](https://en.wikipedia.org/wiki/Gpsd)) but any port 0 to 65535 is supported.



Surveyor: ![Feature Not Supported](img/Icons/RedDot.png) / Express: ![Feature Not Supported](img/Icons/RedDot.png) / Express Plus: ![Feature Not Supported](img/Icons/RedDot.png) / Facet: ![Feature Not Supported](img/Icons/RedDot.png) / Facet L-Band: ![Feature Not Supported](img/Icons/RedDot.png) / Reference Station: ![Feature Supported](img/Icons/GreenDot.png)

The Reference Station sends and receives NTRIP correction data via Ethernet. It can also send NMEA and RTCM navigation messages to an external TCP Server via Ethernet.
It also has a dedicated Configure-Via-Ethernet (*Cfg Eth*) mode which is accessed via the MODE button and OLED display.

By default, the Reference Station will use DHCP to request an IP Address from the network Gateway. But you can optionally configure it with a fixed IP Address.

![Reference Station in DHCP mode](img/Terminal/Ethernet_DHCP.png)

*The Reference Station Ethernet menu - with DHCP selected*

![Reference Station in fixed IP address mode](img/Terminal/Ethernet_Fixed_IP.png)

*The Reference Station Ethernet menu - with a fixed IP address selected*

### Ethernet TCP Client

The Reference Station can act as an Ethernet TCP Client, sending NMEA and / or UBX data to a remote TCP Server.

This is similar to the WiFi TCP Client mode on our other RTK products, but the data can be sent to any server based on its IP Address or URL.

E.g. to connect to a local machine via its IP Address, select option "c" and then enter the IP Address using option "h"

![Ethernet TCP Client configuration](img/Terminal/Ethernet_TCP_Client_1.png)

![Ethernet TCP Client connection](img/Terminal/TCP_Client.gif)

The above animation was generated using [TCP_Server.py](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/blob/main/Firmware/Tools/TCP_Server.py).