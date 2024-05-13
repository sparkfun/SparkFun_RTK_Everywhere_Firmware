# Correction Sources

Torch: ![Feature Supported](img/Icons/GreenDot.png)

To get millimeter accuracy we need to provide the RTK unit with correction values. Corrections, often called RTCM, help the RTK unit refine its position calculations. RTCM (Radio Technical Commission for Maritime Services) can be obtained from a variety of sources but they fall into three buckets: Commercial, Public, and Civilian Reference Stations.

## Commercial Reference Networks

These companies set up a large number of reference stations that cover entire regions and countries, but charge a monthly fee. They are often easy to use but can be expensive.

* [PointPerfect](https://docs.sparkfun.com/SparkFun_RTK_Everywhere_Firmware/quickstart-torch/#pointperfect-corrections) ($8/month) - US, EU, as well as parts of Australia, Brazil, and South Korea. Note: This is an SSR service.
* [PointOneNav](https://app.pointonenav.com/trial?src=sparkfun) ($50/month) - US, EU, Australia, South Korea
* [Skylark](https://www.swiftnav.com/skylark) ($29 to $69/month) - US, EU, Japan, Australia
* [SensorCloud RTK](https://rtk.sensorcloud.com/pricing/) ($100/month) partial US, EU
* [Premium Positioning](https://www.premium-positioning.com) (~$315/month) partial EU
* [KeyNetGPS](https://www.keypre.com/KeynetGPS) ($375/month) North Eastern US
* [Hexagon/Leica](https://hxgnsmartnet.com/en-US) ($500/month) - partial US, EU

## Public Reference Stations

![Wisconsin network of CORS](<img/Corrections/SparkFun NTRIP 7 - Wisconsin Map.png>) 

*State Wide Network of Continuously Operating Reference Stations (CORS)*

Be sure to check if your state or country provides corrections for free. Many do! Currently, there are 21 states in the USA that provide this for free as a department of transportation service. Search ‘Wisconsin CORS’ as an example. Similarly, in France, check out [CentipedeRTK](https://docs.centipede.fr/). There are several public networks across the globe, be sure to google around!

## Civilian Reference Stations

![SparkFun Base Station Enclosure](img/Corrections/Roof_Enclosure.jpg)

*The base station at SparkFun*

You can set up your own correction source. This is done with a 2nd GNSS receiver that is stationary, often called a Base Station. There is just the one-time upfront cost of the Base Station hardware. See the [Creating a Permanent Base](https://docs.sparkfun.com/SparkFun_RTK_Everywhere_Firmware/permanent_base/) document for more information.

## OSR vs SSR

Not all companies providing correction services use the same type of corrections. There are two types: OSR and SSR.

![Wisconsin network of CORS](<img/Corrections/SparkFun NTRIP 7 - Wisconsin Map.png>) 

*State Wide Network of Continuously Operating Reference Stations (CORS)*

**Observation Space Representation** (OSR) is the classic type of corrections network. This is a collection of base stations located at regular intervals across a geographic area. Corrections coming from this type of network provide the highest RTK accuracy (14mm or less is common when located within 10km of a base station) with the minimum convergence time (the time you have to wait before the GNSS receiver can achieve RTK Fix). Normal convergence time for an OSR is a few seconds. However, because a CORS has to be placed every few 10km, these type of networks are expensive to install and maintain. An OSR network is prone to holes or gaps in the network where a base station is not sufficiently close to maintain RTK Fix. Imagine an autonomous semi-trailer truck driving across hundreds or thousands of miles; an OSR network is extremely difficult to set up that maintains the full coverage needed for highly kinetic applications.

[PointOne Nav](https://app.pointonenav.com/trial?src=sparkfun), and [Skylark Nx RTK](https://www.swiftnav.com/products/skylark) are examples of an OSR.

[![PointPerfect Coverage map including L-Band and IP delivery methods](<img/PointPerfect/SparkFun RTK Everywhere - PointPerfect Coverage Map Small.png>)](https://www.u-blox.com/en/pointperfect-service-coverage)

*PointPerfect Coverage map including L-Band and IP delivery methods*

**State Space Representation** (SSR) covers huge areas, sometimes entire continents. SSR combines the readings from a handful of base stations and creates a model for the region. This model extrapolates the needed corrections for a given area. These corrections are 'good enough' for many applications. Because SSR requires far fewer base stations, they are often a much lower-cost service. The RTK Fix accuracy is lower (20mm is possible but 30-60mm is common), and the convergence time increases considerably. Convergence time for an SSR can be 180 seconds or more.

The [PointPerfect](https://www.u-blox.com/en/pointperfect-service-coverage) and [Skylark Cx](https://www.swiftnav.com/products/skylark) are examples of an SSR. 