# WiFi Menu

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
![WiFi Network Entry](./img/Terminal/SparkFun RTK Everywhere - WiFi Menu.png)
<figcaption markdown>
WiFi Menu containing one network
</figcaption>
</figure>

The WiFi menu allows a user to input credentials of up to four WiFi networks. WiFi is used for a variety of features on the RTK device. When WiFi is needed, the RTK device will attempt to connect to any network on the list of WiFi networks. For example, if you enter your home WiFi, work WiFi, and the WiFi for a mobile hotspot, the RTK device will automatically detect and connect to the network with the strongest signal.

Additionally, the device will continue to try to connect to WiFi if a connection is not made. The connection timeout starts at 15 seconds and increases by 15 seconds with each failed attempt. For example, 15, 30, 45, etc seconds are delayed between each new WiFi connection attempt. Once a successful connection is made, the timeout is reset.

WiFi is used for the following features:

- NTRIP Client or Server
- TCP Client or Server
- Firmware Updates
- Device Configuration (WiFi mode only)
- PointPerfect (Access keys and IP-based corrections)

## Configure device via WiFi Access Point of connect to WiFi

By default, when a user enters the WiFi config mode (either by using the external button or with the [System Mode Switch](https://docs.sparkfun.com/SparkFun_RTK_Everywhere_Firmware/menu_system/#mode-switch)), the device will stop what it is doing and enter WiFi Config mode. If this setting is set to `AP` then the RTK device will broadcast as an access point with the name *RTK Config*. If this setting is set to `WiFi`, then the device will attempt to connect to that WiFi network. The `AP` setting is best for in-field configuration, and the `WiFi` setting is handy for configuration from a laptop or desktop on the same WiFi network.

<figure markdown>
![Configuring RTK device over local WiFi](./img/WiFi%20Config/SparkFun%20RTK%20AP%20Main%20Page%20over%20Local%20WiFi.png)
<figcaption markdown>
</figcaption>
</figure>

Configuring over WiFi allows the device to be configured from any desktop computer that has access to the same WiFi network. This method allows for greater control from a full-size keyboard and mouse.

<figure markdown>
![RTK display showing local IP and SSID](./img/Displays/SparkFun%20RTK%20WiFi%20Config%20IP.png)
<figcaption markdown>
</figcaption>
</figure>

On devices that have a display, when the device enters WiFi config mode it will display the WiFi network it is connected to as well as its assigned IP address.

## Captive Portal

If **Captive Portal** is enabled, when a user connects to the Access Point the user will automatically be directed towards the correct page. This works with most, but not all phones.

## MDNS

<figure markdown>
![Access using rtk.local](./img/WiFi Config/SparkFun RTK WiFi MDNS.png)
<figcaption markdown>
</figcaption>
</figure>

Multicast DNS or MDNS allows the RTK device to be discovered over wireless networks without needing to know the IP. For example, when MDNS is enabled, simply type 'rtk.local' into a browser to connect to the RTK Config page. This feature works both for 'WiFi Access Point' or direct WiFi config.

!!! note
	When using WiFi config, you must be on the same subdomain (in other words, the same WiFi or Ethernet network) as the RTK device.
