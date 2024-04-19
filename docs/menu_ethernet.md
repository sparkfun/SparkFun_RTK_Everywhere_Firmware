# Ethernet Menu

Torch: ![Feature Not Supported](img/Icons/RedDot.png) 

An Ethernet-equipped RTK device sends and receives NTRIP correction data via Ethernet. It can also send NMEA and RTCM navigation messages to an external TCP Server via Ethernet. It also has a dedicated Configure-Via-Ethernet (*Cfg Eth*) mode which is accessed via the MODE button and OLED display.

By default, the RTK device will use DHCP to request an IP Address from the network Gateway. But you can optionally configure it with a fixed IP Address.

![RTK Device in DHCP mode](img/Terminal/Ethernet_DHCP.png)

*The Reference Station Ethernet menu - with DHCP selected*

![Reference Station in fixed IP address mode](img/Terminal/Ethernet_Fixed_IP.png)

*The Reference Station Ethernet menu - with a fixed IP address selected*

## Ethernet TCP Client

Ethernet-equipped RTK devices can act as an Ethernet TCP Client, sending NMEA and / or UBX data to a remote TCP Server. This is similar to the WiFi TCP Client mode on our other RTK products, but the data can be sent to any server based on its IP Address or URL. E.g. to connect to a local machine via its IP Address, select option "c" and then enter the IP Address using option "h"

![Ethernet TCP Client configuration](img/Terminal/Ethernet_TCP_Client_1.png)

![Ethernet TCP Client connection](img/Terminal/TCP_Client.gif)

The above animation was generated using [TCP_Server.py](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/blob/main/Firmware/Tools/TCP_Server.py).