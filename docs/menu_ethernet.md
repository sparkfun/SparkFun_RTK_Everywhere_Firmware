# Ethernet Menu

Torch: ![Feature Not Supported](img/Icons/RedDot.png) / EVK: ![Feature Supported](img/Icons/GreenDot.png)

An Ethernet-equipped RTK device sends and receives NTRIP correction data via Ethernet. It can also send NMEA and RTCM navigation messages to an external TCP Server via Ethernet. It also has a dedicated Configure-Via-Ethernet (*Cfg Eth*) mode which is accessed via the MODE button and OLED display.

By default, the RTK device will use DHCP to request an IP Address from the network Gateway. But you can optionally configure it with a fixed IP Address.

![RTK Device in DHCP mode](img/Terminal/Ethernet_DHCP.png)

*The Reference Station Ethernet menu - with DHCP selected*

![Reference Station in fixed IP address mode](img/Terminal/Ethernet_Fixed_IP.png)

*The Reference Station Ethernet menu - with a fixed IP address selected*

When enabled, the "Ethernet / WiFi Failover" option allows the firmware to automatically switch from Ethernet to WiFi should Ethernet become unavailable. Should WiFi become unavailable, the firmware will reconnect to Ethernet if it is now available.