# Correction Sources

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

To get millimeter accuracy we need to provide the RTK unit with correction values. Corrections, often called RTCM, help the RTK unit refine its position calculations. RTCM (Radio Technical Commission for Maritime Services) can be obtained from a variety of sources but they fall into three buckets: Commercial, Public, and Civilian Reference Stations.

## Commercial Reference Networks

These companies set up a large number of reference stations that cover entire regions and countries, but charge a monthly fee. They are often easy to use but can be expensive.

- [PointPerfect](https://docs.sparkfun.com/SparkFun_RTK_Everywhere_Firmware/quickstart-torch/#pointperfect-corrections) ($15/month) - US, EU, as well as parts of Australia, Brazil, and South Korea. Note: This is an [SSR service](https://docs.sparkfun.com/SparkFun_RTK_Everywhere_Firmware/correction_sources/#osr-vs-ssr).
- [Onocoy](https://console.onocoy.com/explorer) ($25/month) - US, EU, Australia, and many other partial areas
- [PointOneNav](https://app.pointonenav.com/trial?src=sparkfun) ($125/month for "True RTK") - US, UK, EU, KOR, AUS, NZ, and JP
- [Skylark](https://www.swiftnav.com/skylark) ($29 to $69/month) - US, EU, Japan, Australia
- [SensorCloud RTK](https://rtk.sensorcloud.com/pricing/) ($100/month) partial US, EU
- [Premium Positioning](https://www.premium-positioning.com) (~$315/month) partial EU
- [KeyNetGPS](https://www.keypre.com/KeynetGPS) ($375/month) North Eastern US
- [Hexagon/Leica](https://hxgnsmartnet.com/en-US) ($500/month) - partial US, EU

## Public Reference Stations

<figure markdown>
![Wisconsin network of CORS](./img/Corrections/SparkFun NTRIP 7 - Wisconsin Map.png)
<figcaption markdown>
State Wide Network of Continuously Operating Reference Stations (CORS)
</figcaption>
</figure>

Be sure to check if your state or country provides corrections for free. Many do! Currently, there are 21 states in the USA that provide this for free as a department of transportation service. Search ‘Wisconsin CORS’ as an example. Similarly, in France, check out [CentipedeRTK](https://docs.centipede.fr/). There are several public networks across the globe, be sure to google around!

## Civilian Reference Stations

<figure markdown>
![SparkFun Base Station Enclosure](./img/Corrections/Roof_Enclosure.jpg)
<figcaption markdown>
The base station at SparkFun
</figcaption>
</figure>

You can set up your own correction source. This is done with a 2nd GNSS receiver that is stationary, often called a Base Station. There is just the one-time upfront cost of the Base Station hardware. See the [Creating a Permanent Base](https://docs.sparkfun.com/SparkFun_RTK_Everywhere_Firmware/permanent_base/) document for more information.

## OSR vs SSR

Not all companies providing correction services use the same type of corrections. There are two types: OSR and SSR.

<figure markdown>
![Wisconsin network of CORS](./img/Corrections/SparkFun NTRIP 7 - Wisconsin Map.png)
<figcaption markdown>
State Wide Network of Continuously Operating Reference Stations (CORS)
</figcaption>
</figure>

**Observation Space Representation** (OSR) is the classic type of corrections network. This is a collection of base stations located at regular intervals across a geographic area. Corrections coming from this type of network provide the highest RTK accuracy (14mm or less is common when located within 10km of a base station) with the minimum convergence time (the time you have to wait before the GNSS receiver can achieve RTK Fix). Normal convergence time for an OSR is a few seconds. However, because a CORS has to be placed every few 10km, these type of networks are expensive to install and maintain. An OSR network is prone to holes or gaps in the network where a base station is not sufficiently close to maintain RTK Fix. Imagine an autonomous semi-trailer truck driving across hundreds or thousands of miles; an OSR network is extremely difficult to set up that maintains the full coverage needed for highly kinetic applications.

[PointOne Nav](https://app.pointonenav.com/trial?src=sparkfun), and [Skylark Nx RTK](https://www.swiftnav.com/products/skylark) are examples of an OSR.

<figure markdown>
[![PointPerfect Coverage map including L-Band and IP delivery methods](./img/PointPerfect/SparkFun RTK Everywhere - PointPerfect Coverage Map Small.png)](https://www.u-blox.com/en/pointperfect-service-coverage)
<figcaption markdown>
PointPerfect Coverage map including L-Band and IP delivery methods
</figcaption>
</figure>

**State Space Representation** (SSR) covers huge areas, sometimes entire continents. SSR combines the readings from a handful of base stations and creates a model for the region. This model extrapolates the needed corrections for a given area. These corrections are 'good enough' for many applications. Because SSR requires far fewer base stations, they are often a much lower-cost service. The RTK Fix accuracy is lower (20mm is possible but 30-60mm is common), and the convergence time increases considerably. Convergence time for an SSR can be 180 seconds or more.

The [PointPerfect](https://www.u-blox.com/en/pointperfect-service-coverage) and [Skylark Cx](https://www.swiftnav.com/products/skylark) are examples of an SSR.

## Galileo HAS

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
- Postcard: :material-radiobox-indeterminate-variant:{ .support-partial title="Feature Partially Supported" }
- Torch: :material-radiobox-marked:{ .support-full title="Feature Supported" }

</div>

The European Union launched a free correction service called [High Accuracy Service](https://www.gsc-europa.eu/galileo/services/galileo-high-accuracy-service-has) or **HAS** starting in 2023. The service is delivered over the E6 frequency. In general, this service will greatly improve accuracy to receivers but is lower accuracy than an OSR or SSR-based RTK Fix. Additionally, a receiver can take up to 5 minutes to benefit from these corrections (convergence time is larger), as opposed to OSR (seconds) or SSR (~180 seconds) to achieve maximum accuracy. But HAS is free! And available with very little additional configuration.

Various SparkFun RTK products support this new GNSS band (E6):

* The RTK EVK does not support E6 reception.
* The RTK Facet mosaic's mosaic-X5 supports E6 reception. However, at the time of writing, the X5 firmware does not yet support the HAS service. Septentrio are planning to support it in a future firmware release.
* The RTK Postcard's LG290P GNSS receiver has the ability to receive the E6 signals but as of writing, HAS is not yet implemented in the GNSS location engine.
* The RTK Torch fully supports HAS/E6. The UM980 firmware needs to be 11833 or newer. See how to [Update the UM980 Firmware](./firmware_update_um980.md) for instructions. HAS/E6 is enabled by default and can be disabled in the [GNSS Menu](menu_gnss.md#galileo-e6-corrections) if desired.

## L-Band Service

Some companies broadcast an extra signal over geosynchronous satellites in something called the 'L-Band' which is really just a catch-all name for any broadcast between 1GHz and 2GHz. Because GNSS satellites broadcast their signal at ~1.57GHz (L1) and ~1.23GHz (L2), it is beneficial to broadcast an extra signal near these frequencies because the GNSS receiver hardware can be adapted to pick up this extra signal. Depending on the company, this signal adds additional correction data that can allow a GNSS receiver to obtain much higher accuracy than L1/L2/L5 positioning alone. But because broadcasting on satellites is exorbitantly expensive, the signal is often encrypted to force users to pay a subscription fee. The benefit of L-Band corrections is that they can cover an entire country or continent with corrections, without the need for internet connectivity, allowing for remote location to be surveyed, or scientific research conducted.

For many years, u-blox provided a service called PointPerfect L-Band that was compatible with certain SparkFun RTK products (specifically [RTK Facet mosaic L-Band](https://www.sparkfun.com/sparkpnt-rtk-facet-mosaic-l-band.html), and [RTK Facet L-Band](https://www.sparkfun.com/sparkfun-rtk-facet-l-band.html)). Unfortunately, in July of 2025 u-blox announced the L-Band service to North America would be discontinued on December 31st, 2025. These products still function as high precision receivers, but they will need to switch to an internet based correction services to obtain an RTK Fix.

## Remote Corrections

What do you do if you need to get an RTK Fix but there is no L-Band or internet service? So how can users obtain corrections in remote areas? There are a few options.

### Base/Rover Setup

A base can be set on site and surveyed in with high accuracy in about 12 hours. Once that is completed a base can transmit corrections to the rover allowing the rover to obtain RTK Fix with high absolute accuracy. The connection between the rover and base can be accomplished with a variety of options. Please see [Correction Transport](correction_transport.md) for more information.

### Remote Internet

A few innovative users have reported using [Eutelsat KONNECT](https://europe.konnect.com/en-DE) or [Starlink Roam](https://www.starlink.com/us/roam) to gain internet connectivity in remote areas to then connect to a standard NTRIP corrections source.

### Post Processing

If real time kinematics is not needed, many SparkFun PNT devices can log raw GNSS satellite data onto an SD card where it can be post processed into very accurate location data.

