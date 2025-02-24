# Correction Transport

Once a [correction source](correction_sources.md) is chosen, the correction data must be transported from the base to the rover. The RTCM serial data is approximately 530 bytes per second but varies depending on the GNSS receiver and its settings. This section describes the various methods to move correction data from a base to one or more rovers.

RTK calculations require RTCM data to be delivered approximately once per second. If RTCM data is lost or not received by a rover, RTK Fix can still be maintained for around 30 seconds before the device will enter RTK Float mode. If a transport method experiences congestion (ie, cellular latency, Serial Radios dropping packets, etc) the rover(s) can continue in RTK Fix mode even if correction data is not available for multiple seconds.

## Cellular - Built-In

<!--
Compatibility Icons
====================================================================================

:material-radiobox-marked:{ .support-full title="Feature Supported" }
:material-radiobox-indeterminate-variant:{ .support-partial title="Feature Partially Supported" }
:material-radiobox-blank:{ .support-none title="Feature Not Supported" }
-->

<div class="grid cards fill" markdown>

- EVK: :material-radiobox-blank:{ .support-none title="Feature Not Supported" }
- Facet mosaic: :material-radiobox-blank:{ .support-none title="Feature Not Supported" }
- Postcard: :material-radiobox-blank:{ .support-none title="Feature Not Supported" }
- Torch: :material-radiobox-blank:{ .support-none title="Feature Not Supported" }

</div>

The RTK EVK has built-in cellular via a u-blox LARA-R6001D. However, the RTK Everywhere firmware does not yet support cellular. Adding it is on our roadmap. Stay tuned for updates! Meanwhile, we do have a stand-alone EVK code example which will connect to PointPerfect localized distribution via cellular:

- [EVK example 8_5_PointPerfect_MQTT](https://github.com/sparkfun/SparkFun_RTK_EVK/tree/main/Example_Sketches/8_5_PointPerfect_MQTT)
- [EVK example 8_6_PointPerfect_MQTT_WiFi_ETH_Cellular](https://github.com/sparkfun/SparkFun_RTK_EVK/tree/main/Example_Sketches/8_6_PointPerfect_MQTT_WiFi_ETH_Cellular)

## Cellular - Via Cellphone

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

<figure markdown>
![SW Maps NTRIP Client](./img/SWMaps/SW_Maps_-_NTRIP_Client.jpg)
<figcaption markdown>
</figcaption>
</figure>

Using a cell phone is the most common way of transporting correction data from the internet to a rover. This method uses the cell phone's built-in internet connection to obtain data from an NTRIP Caster and then pass those corrections over Bluetooth to the RTK device.

Shown above are SW Map's NTRIP Client Settings. Nearly all GIS applications have an NTRIP Client built in so we recommend leveraging the device you already own to save money. Additionally, a cell phone gives your rover incredible range: a rover can obtain RTCM corrections anywhere there is cellular coverage.

Cellular can even be used in Base mode. We have seen some very inventive users use an old cell phone as a WiFi access point. The base unit is configured as an NTRIP Server with the cellphone's WiFi AP credentials. The base performs a survey-in, connects to the WiFi, and the RTCM data is pushed over WiFi, over cellular, to an NTRIP Caster.

## Ethernet

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

Ethernet-equipped RTK devices send and receive correction data via Ethernet.

Please see [Ethernet Menu](menu_ethernet.md) for more details.

## ESP-NOW

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
- Torch: [:material-radiobox-marked:{ .support-full }]( title ="Feature Supported" )

</div>

<figure markdown>
![Max transmission range of about 250m](./img/Radios/SparkFun%20RTK%20ESP-Now%20Distance%20Testing.png)
<figcaption markdown>
</figcaption>
</figure>

All RTK devices have a built-in radio capable of transmitting RTCM from a single base to multiple rovers. The range is not great, but it's free! See [ESP-NOW Configuration](menu_radios.md#esp-now) for more information.


## L-Band

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
- Postcard: :material-radiobox-blank:{ .support-none title="Feature Not Supported" }
- Torch: :material-radiobox-blank:{ .support-none title="Feature Not Supported" }

</div>

What if you are in the field, far away from WiFi, cellular, radio, or any other data connection? Look to the sky!

A variety of companies provide GNSS RTK corrections broadcast from satellites over a spectrum called L-Band. [L-Band](https://en.wikipedia.org/wiki/L_band) is any frequency from 1 to 2 GHz. These frequencies have the ability to penetrate clouds, fog, and other natural weather phenomena making them particularly useful for location applications.

These corrections are not as accurate as a fixed base station, and the corrections can require a monthly subscription fee, but you cannot beat the ease of use!

L-Band reception requires specialized RF receivers capable of demodulating the satellite transmissions. The RTK EVK has a built-in NEO-D9S corrections receiver. The RTK Everywhere firmware supports this and will tune the NEO-D9S to the correct frequency if you are in the US or EU. The PointPerfect L-Band corrections are encrypted and require a subscription and valid keys in order to work. The EVK comes with a one month free subscription to PointPerfect L-Band + IP, providing built-in support for L-Band corrections and IP corrections via Ethernet or WiFi.

## LoRa

<!--
Compatibility Icons
====================================================================================

:material-radiobox-marked:{ .support-full title="Feature Supported" }
:material-radiobox-indeterminate-variant:{ .support-partial title="Feature Partially Supported" }
:material-radiobox-blank:{ .support-none title="Feature Not Supported" }
-->

<div class="grid cards fill" markdown>

- EVK: :material-radiobox-blank:{ .support-none title="Feature Not Supported" }
- Facet mosaic: :material-radiobox-blank:{ .support-none title="Feature Not Supported" }
- Postcard: :material-radiobox-blank:{ .support-none title="Feature Not Supported" }
- Torch: :material-radiobox-marked:{ .support-full title="Feature Supported" }

</div>

<figure markdown>
![Antenna covered removed from the RTK Torch](./img/Repair/GPS-24672-RTK-Torch-Internal2.jpg)
<figcaption markdown>
Antenna covered removed from the RTK Torch
</figcaption>
</figure>


The RTK Torch contains a 1 watt Long Range (LoRa) radio capable of transmitting RTCM from a single base to multiple rovers. See [LoRa Radio Configuration](menu_radios.md#configuration) for more information.

## Serial Radios

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
- Torch: :material-radiobox-blank:{ .support-none title="Feature Not Supported" }

</div>

<figure markdown>
![Two serial radios](./img/Corrections/19032-SiK_Telemetry_Radio_V3_-_915MHz__100mW-01.jpg)
<figcaption markdown>
</figcaption>
</figure>

Serial radios, sometimes called telemetry or packet radios, provide what is essentially a serial cable between the base and rover devices. Transmission distance, frequency, maximum data rate, configurability, and price vary widely, but all behave functionally the same. SparkFun recommends the [HolyBro 100mW](https://www.sparkfun.com/products/19032) and the [SparkFun LoRaSerial 1W](https://www.sparkfun.com/products/19311) radios for RTK use.

<figure markdown>
![Serial radio cable](./img/Corrections/17239-GHR-04V-S_to_GHR-06V-S_Cable_-_150mm-01.jpg)
<figcaption markdown>
</figcaption>
</figure>

On SparkFun RTK products that have an external radio port, a [4-pin to 6-pin cable](https://www.sparkfun.com/products/17239) is included that will allow you to connect the HolyBro branded radio or the SparkFun LoRaSerial radios to a base and rover RTK device.

<figure markdown>
![Radio attached to RTK device](./img/Corrections/SparkFun_RTK_Surveyor_-_Radio.jpg)
<figcaption markdown>
</figcaption>
</figure>

The RTK EVK has screw cage terminals providing access to the ZED-F9P UART2 TX2 and RX2 pins. 3.3V power is provided too, but not 5V. For 5V radios, you may need an additional power source.

These radios attach nicely to the back or bottom of an RTK device.

The benefit of a serial telemetry radio link is that you do not need to configure anything; simply plug two radios onto two RTK devices and turn them on.

The downside to serial telemetry radios is that they generally have a much shorter range (often slightly more than a 1-kilometer functional range) than a cellular link can provide.


## WiFi

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

<figure markdown>
![NTRIP Server setup](./img/WiFi Config/RTK_Surveyor_-_WiFi_Config_-_Base_Config2.jpg)
<figcaption markdown>
</figcaption>
</figure>

Any SparkFun RTK device can be set up as an [NTRIP Server](menu_base.md#ntrip-server). This means the device will connect to local WiFi and broadcast its correction data to the internet. The data is delivered to something called an NTRIP Caster. Any number of rovers can then access this data using something called an NTRIP Client. Nearly *every* GIS application has an NTRIP Client built into it so this makes it very handy.

WiFi broadcasting is the most common transport method of getting RTCM correction data to the internet and to rovers via NTRIP Clients.

<figure markdown>
![RTK product in NTRIP Client mode](./img/Displays/SparkFun_RTK_Rover_NTRIP_Client_Connection.png)
<figcaption markdown>
RTK product showing corrections being downloaded over WiFi in NTRIP Client mode
</figcaption>
</figure>

Similarly, any SparkFun RTK device can be set up as an [NTRIP Client](menu_gnss.md#ntrip-client). The RTK device will connect to the local WiFi and begin downloading the RTCM data from the given NTRIP Caster and RTK Fix will be achieved. This is useful only if the Rover remains in RF range of a WiFi access point. Because of the limited range, we recommend using a cell phone's hotspot feature rather than a stationary WiFi access point for NTRIP Clients.
