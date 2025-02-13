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
- Postcard: :material-radiobox-marked:{ .support-full title="Feature Supported" }
- Torch: :material-radiobox-marked:{ .support-full title="Feature Supported" }

</div>

To get millimeter accuracy we need to provide the RTK unit with correction values. Corrections, often called RTCM, help the RTK unit refine its position calculations. RTCM (Radio Technical Commission for Maritime Services) can be obtained from a variety of sources but they fall into three buckets: Commercial, Public, and Civilian Reference Stations.

## Commercial Reference Networks

These companies set up a large number of reference stations that cover entire regions and countries, but charge a monthly fee. They are often easy to use but can be expensive.

- [PointPerfect](https://docs.sparkfun.com/SparkFun_RTK_Everywhere_Firmware/quickstart-torch/#pointperfect-corrections) ($8/month) - US, EU, as well as parts of Australia, Brazil, and South Korea. Note: This is an [SSR service](https://docs.sparkfun.com/SparkFun_RTK_Everywhere_Firmware/correction_sources/#osr-vs-ssr).
- [Onocoy](https://console.onocoy.com/explorer) ($25/month) - US, EU, Australia, and many other partial areas
- [PointOneNav](https://app.pointonenav.com/trial?src=sparkfun) ($50/month) - US, EU, Australia, South Korea
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

- EVK: [:material-radiobox-blank:{ .support-none }]( title ="Feature Not Supported" )
- Postcard: [:material-radiobox-blank:{ .support-none }]( title ="Feature Partially Supported" )
- Torch: :material-radiobox-marked:{ .support-full title="Feature Supported" }

</div>

The European Union launched a free correction service called [High Accuracy Service](https://www.gsc-europa.eu/galileo/services/galileo-high-accuracy-service-has) or **HAS** starting in 2023. The service is delivered over the E6 frequency. In general, this service will greatly improve accuracy to receivers but is lower accuracy than an OSR or SSR-based RTK Fix. Additionally, a receiver can take up to 5 minutes to benefit from these corrections (convergence time is larger), as opposed to OSR (seconds) or SSR (~180 seconds) to achieve maximum accuracy. But HAS is free! And available with very little additional configuration.

Various SparkFun RTK products support this new GNSS band (E6). 
* The RTK EVK does not support E6 reception.
* The RTK Postcard's LG290P GNSS receiver has the ability to receive the E6 signals but as of writing, HAS is not yet implemented in the GNSS location engine.
* The RTK Torch will need UM980 firmware 118333 or newer. See how to [Update the UM980 Firmware](firmware_update.md#updating-um980-firmware) for instructions. HAS/E6 is enabled by default and can be disabled in the [GNSS Menu](menu_gnss.md#galileo-e6-corrections) if desired.
