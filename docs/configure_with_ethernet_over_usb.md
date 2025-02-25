# Configure with Ethernet Over USB

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

On the RTK Facet mosaic, it is possible to configure the mosaic-X5 GNSS module using Ethernet-over-USB and the X5's internal web page.

However, because the ESP32 does considerable configuration of the mosaic-X5 and the _Boot_ and _Current_ receiver configurations at power on, you need to use a little care when modifying the settings of the X5 using the internal web page. Nothing will break, but your changes could be overwritten at the next power cycle or if you [Factory Reset](./menu_system.md#factory-reset) the settings.

Download and install [Septentrio's RxTools](https://www.septentrio.com/en/products/gps-gnss-receiver-software/rxtools) before continuing. This will also install the USB drivers needed to communicate with the mosaic-X5 using Ethernet-over-USB.

Connect the RTK Facet mosaic to your computer using a USB-C cable. Power on the RTK and wait a few seconds for the mosaic-X5 to boot up. The OLED display will show the Satellites In View count and Horizontal Accuracy once the mosaic-X5 is running. Open a web browser and navigate to **192.168.3.1**. You should see the mosaic-X5's internal web page.

<figure markdown>
![The mosaic-X5 internal web page](./img/mosaic-X5/mosaic-X5_web_config_1.png)
<figcaption markdown>
The mosaic-X5 internal web page
</figcaption>
</figure>

## RTK Facet mosaic Essentials

You can do **SO** much with the mosaic-X5's internal web page that it is difficult to know where to begin. In this section we summarise the essentials: how the mosaic-X5 is interfaced; how it is configured by the ESP32 processor and the RTK Everywhere firmware; and how it is possible to change and save that configuration using the internal web page.

The mosaic-X5's internal web page is (probably) the easiest way to view and change the X5's configuration. But it is far from the only way. The configuration can also be changed using [Sepentrio's RxTools](./configure_with_rxtools.md). Or, if you study the [mosaic-X5 Firmware Reference Guide](https://docs.sparkfun.com/SparkFun_RTK_Facet_mosaic/assets/component_documentation/mosaic-X5%20Firmware%20v4.14.0%20Reference%20Guide.pdf), you can also do all of the following manually by entering Commands over any COM (Serial) port.

### mosaic-X5 Interfaces

The following is a summary of the mosaic-X5's interfaces and how they can be accessed on the RTK Facet mosaic:

- **Ethernet-over-USB:**
    - If you have followed the instructions above, the X5's internal web page can be viewed at **192.168.3.1**. This address is fixed and cannot be changed. If any of your computers other interfaces (WiFi, Ethernet) also the nnn.nnn.3.nnn subnet, you will run into conflicts. You will need to change the configuration of the other device to use a different subnet.
- **USB COM (Serial) Ports:**
    - When connected via USB, two additional COM ports will appear. Internally to the X5, these are named _USB1_ and _USB2_. RxTools can connect directly to these ports. Or, if you wish, you can open a serial console / terminal emulator and enter commands directly.
- **RADIO Port**:
    - The RTK Facet mosaic RADIO port is connected directly to the mosaic-X5 **COM2** serial / UART port. Normally this port carries NMEA and/or RTCM data to/from an external radio module. But, if you wish, you can enter commands direct to this port too.
- **DATA Port:**
    - The RTK Facet mosaic DATA port is connected to the mosaic-X5 **COM3** serial / UART port via a hardware multiplexer. Please see [Ports Menu](./menu_ports.md) for more details, and the section on the [Mux Channel](./menu_ports.md#mux-channel) in particular. Set the multiplexer to **NMEA** to connect X5 COM3 to the **DATA** port. The baud rate can be changed through the menu; the default baud rate is **230400** (8 data bits, no parity, 1 stop bit). Commands can be entered over this port too.
- **ESP32 and RTK Everywhere Firmware:**
    - The ESP32 processor is connected to the mosaic-X5 **COM1** and **COM4** serial / UART ports. Configuration is performed over COM4. COM1 is used to: provide RTCM correction data _to_ the X5 in Rover mode; carry NMEA and / or RTCM data _from_ the X5 in Base mode. If you have a mobile phone connected to the RTK Facet mosaic over Bluetooth, corrections received over Bluetooth are pushed to the X5 on COM1. If the ESP32 is connected directly to WiFi, TCP (NTRIP) corrections received over WiFi are also pushed over COM1. COM1 also carries the raw L-Band correction stream from the X5, when the X5 is configured to use the u-blox PointPerfect corrections (only available in the contiguous USA).

### mosaic-X5 microSD

If you are familiar with the other SparkFun RTK products, you will know that most provide data storage on microSD card. On the RTK Facet mosaic, the microSD card is connected directly to the mosaic-X5. It is not connected to the ESP32 processor. This provides many advantages and some disadvantages.

The mosaic-X5 can be configured to log NMEA messages direct to SD card. It also supports data logging in RINEX format, opening a new log file every 15 mins / 1 hour / 6 hours / 24 hours if desired.

Because the SD card is connected directly to the mosaic-X5 using SDIO, the X5 is able to log data at much higher rates than our other RTK products. The data does not need to be passed to the ESP32 via UART for logging via SPI.

The mosaic-X5 SD card must be mounted for data logging. But it can also be unmounted, causing it to appear as a mass-storage device ("thumb drive"). Files can be downloaded or uploaded very quickly over the USB interface. The RTK Everywhere Firmware does not currently support mounting / unmounting, but it is [something we will add](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/issues/559) in the near future.

The RTK Everywhere Firmware [Data Logging Menu](./menu_data_logging.md) can be used to enable logging to microSD. The messages which are logged are the NMEA messages defined in the [Configure GNSS Messages](./menu_messages.md) menu.

### Receiver Configuration

The mosaic-X5 supports five Receiver Configurations:

- _RxDefault_
- _Boot_
- _Current_
- _User1_
- _User2_

_RxDefault_ is read-only. During a [Factory Reset](./menu_system.md#factory-reset), _RxDefault_ is copied into both _Current_ and _Boot_.

_Boot_ contains the configuration which is loaded when the mosaic-X5 boots up. The RTK Everywhere Firmware modifies the _Boot_ configuration to (e.g.): change the COM port settings; enable a stream of NMEA and RTCM messages.

_Current_ is the current configuration. Commands can be used to alter the current configuration. _Current_ can be copied to _Boot_ if desired. During a [Factory Reset](./menu_system.md#factory-reset) the RTK Everywhere Firmware: copies _RxDefault_ into both _Current_ and _Boot_; makes changes to _Current_; and then copies _Current_ to _Boot_.

If you want to save the X5 configuration after you have changed it, we recommend using either of the _User1_ or _User2_ Receiver Configurations. The RTK Everywhere firmware will modify the _Current_ and _Boot_ Receiver Configurations, but it leaves _User1_ and _User2_ for your use. Copy the _Current_ configuration into (e.g.) _User1_ to save the modified configuration. You can restore it later by copying _User1_ into _Current_.

The _Admin \ Configurations_ tab can be used to view and copy the receiver configurations:

<figure markdown>
![The mosaic-X5 configurations web page](./img/mosaic-X5/mosaic-X5_web_config_2.png)
<figcaption markdown>
The mosaic-X5 configurations web page
</figcaption>
</figure>

Clicking on the small **+** icon next to **Current** will show how the current configuration differs from the RxDefault.

### Expert Control

The _Admin \ Expert Control \ Expert Console_ option can be very useful when modifying the mosaic-X5 configuration. It provides full access to configuration and makes it possible to use script files.

_Expert Control_ is so powerful, you may wish to use the _Admin \ User Administration_ settings to limit access to the X5's configuration. But be careful that you do not prevent the firmware on the ESP32 from accessing the COM ports.
