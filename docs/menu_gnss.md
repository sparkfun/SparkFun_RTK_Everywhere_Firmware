# GNSS Menu

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

The SparkFun RTK product line is immensely configurable. The RTK device will, by default, put the GNSS receiver into the most common configuration for rover/base RTK for use with *SW Maps* and other GIS applications.

The GNSS Configuration menu allows a user to change the report rate, dynamic model, and select which constellations should be used for fix calculations.

<figure markdown>

![GNSS menu showing measurement rates and dynamic model](<./img/Terminal/SparkFun RTK Everywhere - GNSS Receiver.png>)
<figcaption markdown>
GNSS menu showing measurement rates and dynamic model
</figcaption>
</figure>

## Measurement Frequency

Measurement Frequency can be set by either Hz or by seconds between measurements. Some users need many measurements per second; RTK devices support up to 20Hz with RTK enabled. Some users are doing very long static surveys that require many seconds between measurements; the GNSS receiver supports up to 65 seconds between readings.

!!! note
	When in **Base** mode, the measurement frequency is set to 1Hz. This is because RTK transmission does not benefit from faster updates, nor does logging of RAWX for PPP.

## Dynamic Model

The Dynamic Model can be changed but it is recommended to leave it as *Survey*. For more information, please see the list of [reference documents](reference_documents.md) for your platform.

## Min SV Elevation and C/N0

<figure markdown>

![Elevation and C/N0](<./img/Terminal/SparkFun RTK Everywhere - GNSS Receiver.png>)
<figcaption markdown>
GNSS menu showing Minimum SV Elevation and C/N0
</figcaption>
</figure>

A minimum elevation is set in degrees. If a satellite is detected that is below this elevation, it will be *excluded* from any GNSS position calculation.

A minimum C/N0 is set in dB. If a satellite is detected that is below this signal strength, it will be *excluded* from any GNSS position calculation.

## Constellations Menu

<figure markdown>

![Enable or disable the constellations used for fixes](<./img/Terminal/SparkFun RTK Everywhere - GNSS Menu Constellations.png>)
<figcaption markdown>
Enable or disable the constellations used for fixes
</figcaption>
</figure>

The GNSS receiver is capable of tracking multiple channels across four constellations, each producing their own GNSS signals (ie, L1C/A, L1C, L2P, L2C, L5, E1, E5a, E5b, E6, B1I, B2I, B3I, B1C, B2a, B2b, etc). The supported constellations include GPS (USA), Galileo (EU), BeiDou (China), and GLONASS (Russia). SBAS (satellite-based augmentation system) is also supported. By default, all constellations are used. Some users may want to study, log, or monitor a subset. Disabling a constellation will cause the GNSS receiver to ignore those signals when calculating a location fix.

### Galileo E6 Corrections

If supported by hardware, Galileo E6 corrections are enabled by default to support High Accuracy Service. They can be disabled if desired. For detailed information see [High Accuracy Service corrections](correction_sources.md#galileo-has).

## NTRIP Client

<figure markdown>

![NTRIP Client enabled showing settings](<./img/Terminal/SparkFun RTK Everywhere - GNSS Receiver.png>)
<figcaption markdown>
NTRIP Client enabled showing settings
</figcaption>
</figure>

The SparkFun RTK Everywhere devices can obtain their correction data over a few different methods.

- Bluetooth - This is the most common. An app running on a tablet or phone has an NTRIP client built into it. Once the phone is connected over Bluetooth SPP, the RTCM is pushed from the phone to the RTK device. No NTRIP Client needs to be setup on the RTK device.
- WiFi - The rover uses WiFi to be an NTRIP Client and connect to an NTRIP Caster. WiFi and Bluetooth can run simultaneously. This is helpful in situations where a GIS software does not have an NTRIP Client; a cellular hotspot can be used to provide WiFi to the RTK device setup to use NTRIP Client an obtain RTK Fix, while Bluetooth is used to connect to the GIS software for data mapping and collection.

Once the NTRIP Client is enabled you will need a handful of credentials:

- Local WiFi SSID and password (WPA2)
- A casting service and port such as [RTK2Go](http://rtk2go.com/) or [Emlid](https://emlid.com/ntrip-caster/) (the port is almost always 2101)
- A mount point (required) and password (optional)

With these credentials set, the RTK device will attempt to connect to WiFi, then connect to your caster of choice, and then begin downloading the RTCM data over WiFi. We tried to make it as easy as possible. Every second a few hundred bytes, up to ~2k, will be downloaded from the mount point you've entered. Remember, the rover must be in WiFi range to connect in this mode.

Once the device connects to WiFi, it will attempt to connect to the user's chosen NTRIP Caster. If WiFi or the NTRIP connection fails, the rover will return to normal operation.

## Multipath Mitigation

<!--
Compatibility Icons
====================================================================================

:material-radiobox-marked:{ .support-full title="Feature Supported" }
:material-radiobox-indeterminate-variant:{ .support-partial title="Feature Partially Supported" }
:material-radiobox-blank:{ .support-none title="Feature Not Supported" }
-->

<div class="grid cards fill" markdown>

- EVK: :material-radiobox-blank:{ .support-none title="Feature Not Supported" }
- Facet mosaic: :material-radiobox-indeterminate-variant:{ .support-partial title="Feature Partially Supported" }
- Torch: :material-radiobox-marked:{ .support-full title="Feature Supported" }

</div>

<figure markdown>

![Menu for controlling Multipath Mitigation](<./img/Terminal/SparkFun RTK Everywhere - GNSS Multipath Mitigation.png>)
<figcaption markdown>
Menu for controlling Multipath Mitigation
</figcaption>
</figure>

On devices that support it, *Multipath Mitigation* can be enabled (default) or disabled. Multipath Mitigation allows the GNSS receiver to filter signals more rigorously, which aids accuracy in urban or high multipath environments, but may increase processing times in an open environment.

On the RTK Facet mosaic, the mosaic-X5 supports multipath mitigation. However, this cannot yet be configured through the RTK Everywhere. Instead it will be necessary to configure the X5 through its own internal web page and save the configuration to its internal memory.
