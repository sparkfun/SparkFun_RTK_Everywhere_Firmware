# Configure with RxTools

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
- Postcard: :material-radiobox-blank:{ .support-none title="Feature Not Supported" }
- Torch: :material-radiobox-blank:{ .support-none title="Feature Not Supported" }

</div>

On the RTK Facet mosaic, it is possible to configure the mosaic-X5 GNSS module using [Septentrio's RxTools: GNSS receiver control and analysis software](https://www.septentrio.com/en/products/gps-gnss-receiver-software/rxtools).

However, because the ESP32 does considerable configuration of the mosaic-X5 and the _Boot_ and _Current_ receiver configurations at power on, you need to use a little care when modifying the settings of the X5 using RxTools. Nothing will break, but your changes could be overwritten at the next power cycle or if you [Factory Reset](./menu_system.md#factory-reset) the settings.

We have only used RxTools on Windows, but it is also Linux compatible.

Download and install RxTools before continuing. This will also install the USB drivers needed to communicate with the mosaic-X5.

There are multiple ways to connect to the mosaic-X5 using RxTools:

- While the RTF Facet mosaic is connected to your computer via USB, two additional serial COM ports will appear in Device Manager: Septentrio Virtual USB COM Port 1 and 2. RxTools can be configured to use either serial port. The X5 firmware names these ports _USB1_ and _USB2_.
- The mosaic-X5 also supports Ethernet-Over-USB and can be configured via TCP/IP. In RxControl / Change Connection, select **TCP/IP Connection** and enter **192.168.3.1** as the Receiver Address.
- The RTK Facet mosaic **DATA** JST connector is connected the mosaic-X5 _COM3_ UART port via a multiplexer. Please see [Ports Menu](./menu_ports.md) for more details, and the section on the [Mux Channel](./menu_ports.md#mux-channel) in particular. Set the multiplexer to **GNSS TX Out/RX In** to connect X5 _COM3_ to the **DATA** port. The baud rate can be changed through the menu; the default baud rate is **230400** (8 data bits, no parity, 1 stop bit). To connect the **DATA** port to your computer, you will need a [Serial Basic Breakout](https://www.sparkfun.com/sparkfun-serial-basic-breakout-ch340c-and-usb-c.html) or similar.

<figure markdown>
![Wires connected to a SparkFun USB C to Serial adapter](./img/SparkFun_RTK_Facet_-_Data_Port_to_USB.jpg)
<figcaption markdown>
</figcaption>
</figure>

If you want to save the X5 configuration after you have changed it, we recommend using either of the _User1_ or _User2_ Receiver Configurations. The RTK Everywhere firmware will modify the _Current_ and _Boot_ Receiver Configurations, but it leaves _User1_ and _User2_ for your use. Copy the _Current_ configuration into (e.g.) _User1_ to save the modified configuration. You can restore it later by copying _User1_ into _Current_. See [Custom Configuration](./configure_with_ethernet_over_usb.md#custom-configuration) for more details.

RxTools is very powerful software. It can read, analyze and visualise log files containing both NMEA and SBF binary data. It can also convert data to other formats. The mosaic-X5 supports [data logging in RINEX format directly](./menu_data_logging.md#rtk-facet-mosaic). But it is also possible to log data in SBF binary format and later convert it to RINEX using RxTools.
