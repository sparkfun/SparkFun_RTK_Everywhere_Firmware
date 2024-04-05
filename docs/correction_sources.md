# Correction Sources

Torch: ![Feature Supported](img/Icons/GreenDot.png)

To achieve an RTK Fix, SparkFun RTK products must be provided with a correction source. This correction data, sometimes called RTCM (see [What is RTCM?](https://learn.sparkfun.com/tutorials/what-is-gps-rtk/all#what-is-rtcm)), can be produced from a variety of sources.

* [Paid Services](correction_sources.md#paid-services)
* [Government Provided Corrections](correction_sources.md#government-provided-corrections)
* [Permanent Base](correction_sources.md#permanent-base)

## Paid Services

These services cover entire countries and regions but charge a monthly fee. Easy to use, but the most expensive.

* [PointOneNav](https://app.pointonenav.com/trial?src=sparkfun) ($50/month) - US, EU
* [Skylark](https://www.swiftnav.com/skylark) ($29 to $69/month) - US, EU, Japan, Australia
* [Vector RTK](https://vectorrtk.com/) ($115/month) - UK
* [SensorCloud RTK](https://rtk.sensorcloud.com/pricing/) ($100/month) partial US, EU
* [KeyNetGPS](https://www.keypre.com/KeynetGPS) ($375/month) North Eastern US
* [Hexagon/Leica](https://hxgnsmartnet.com/en-US) ($500/month) - partial US, EU

Using PointOneNav is discussed in the [Quick Start guide](https://docs.sparkfun.com/SparkFun_RTK_Firmware/intro/#ntrip-example). We'll discuss using Skylark below. All services have the same basic interface: as long as the service has NTRIP, the SparkFun RTK product can use it.

**Skylark**

![Skylark coverage area](img/Corrections/Skylark-Coverage.png)

*Skylark Coverage Area*

A company called SwiftNav offers a service called [Skylark](https://www.swiftnav.com/skylark). As of writing, for $29 to $69 per month, you will get corrections covering North America, Europe, and the Asia Pacific. This is a very simple method for obtaining NTRIP corrections.

![Skylark website showing credentials](img/Corrections/SparkFun%20NTRIP%20Skylark%201%20-%20Credentials.png)

Upon creating an account, you'll be issued NTRIP credentials that can immediately be used with Lefebure, SW Maps, or any GIS app that supports NTRIP. 

![Entering credentials into SW maps](img/SWMaps/SparkFun%20NTRIP%20Skylark%202%20-%20SW%20Maps%20Credentials.png)

*Entering credentials into SW maps*

The most difficult part of using Skylark for corrections is entering the auto-generated NTRIP Password. While we understand security is important, it's not trivial manually entering these types of credentials into a GIS application.

![GNSS Status showing positional accuracy](img/SWMaps/SparkFun%20NTRIP%20Skylark%202%20-%20SW%20Maps%20HPA.png)

*SW Maps showing Positional Accuracy*

One downside is that with a 'regional' provider such as Skylark the distance to the correction station may be larger than 10km. While we've always gotten an RTK fix, we often see horizontal positional accuracy of ~30mm instead of the 14mm when using our fixed GNSS reference station. Your mileage may vary.

**PointPerfect**


[![PointPerfect Coverage map including L-Band and IP delivery methods](<img/PointPerfect/SparkFun RTK Everywhere - PointPerfect Coverage Map Small.png>)](https://www.u-blox.com/en/pointperfect-service-coverage)

*PointPerfect Coverage map including L-Band and IP delivery methods*

SparkFun RTK Everywhere devices are equipped to get corrections from a service called PointPerfect. Please see the [PointPerfect Menu](menu_pointperfect.md) page for more information.

## Government Provided Corrections

![Wisconsin network of CORS](<img/Corrections/SparkFun NTRIP 7 - Wisconsin Map.png>) 

*State Wide Network of Continuously Operating Reference Stations (CORS)*

Be sure to check if your state or country provides corrections for free. Many do! Currently, there are 21 states in the USA that provide this for free as a department of transportation service. Search ‘Wisconsin CORS’ as an example. Similarly, in France, check out [CentipedeRTK](https://docs.centipede.fr/). 

[![UNAVO map](img/Corrections/SparkFun%20NTRIP%204%20-%20UNAVCO%20Map.png)](https://www.unavco.org/instrumentation/networks/status/all/realtime)

[UNAVCO](https://www.unavco.org/) is a US-based governmental organization that runs a [network of publicly available NTRIP sources](https://www.unavco.org/instrumentation/networks/status/all/realtime). If you're lucky there's a station within 10km (6 miles) of you.

![Map of European stations](img/Corrections/SparkFun%20NTRIP%206%20-%20EUREF%20Map.png)

[EUREF](http://www.epncb.oma.be/_networkdata/data_access/real_time/map.php) is a permanent GNSS network in the EU.

There are several public networks across the globe, be sure to google around!

## Permanent Base

![SparkFun Base Station Enclosure](img/Corrections/Roof_Enclosure.jpg)

*The base station at SparkFun*

A permanent base is a user-owned and operated base station. See [Creating a Permanent Base](permanent_base.md) for more information. A permanent base has the benefit of being the most accurate, with relatively low cost, but requires at least 24 hours of initial logging and some mechanical setup time (attaching the antenna, connecting a device to the internet, etc).

## Temporary Base

[![Temporary RTK Express Base setup](<img/Corrections/SparkFun_RTK_Express_-_Base_Radio.jpg>)](<img/Corrections/SparkFun_RTK_Express_-_Base_Radio - Big.jpg>)

*Temporary RTK Express Base setup with serial radio*

A temporary or mobile base setup is handy when you are in the field too far away from a correction source, or if your measurements do not require absolute accuracy. 

To set up a temporary base, an RTK device is mounted to a tripod and is configured to complete a survey-in (aka, locate itself). It will then begin broadcasting RTCM correction data. This data (~1000 bytes a second) is sent over a data link to one or multiple rovers that can then obtain RTK Fix. 

Any tripod with a ¼” camera thread will work. The [Amazon Basics tripod](https://www.amazon.com/AmazonBasics-Lightweight-Camera-Mount-Tripod/dp/B00XI87KV8) works well enough but is a bit lightweight and rickety. 

Any of the RTK Everywhere products can be set up to operate in **Base** mode. Once the base has been set up with a clear view of the sky, turn on the RTK device.

![RTK Torch in Survey-In Mode](img/SparkFun_RTK_Torch_On_Tripod.jpg)
![](img/Displays/SparkFun_RTK_Express_-_Display_-_Survey-In.jpg)

*RTK device in Survey-In Mode*

On the RTK Torch, base mode is entered using a [Mode Switch](https://docs.sparkfun.com/SparkFun_RTK_Everywhere_Firmware/menu_system/#mode-switch) from the System Menu.

Once the survey is complete the device will begin producing RTCM correction data. If you are using the internal 2.4GHz ESP-Now radios, data will start to broadcast. If you have an NTRIP Server enabled, corrections will be uploaded to the NTRIP Caster of your choosing. 

Note: A mobile base station works well for quick trips to the field. However, the survey-in method is not recommended for the highest accuracy measurements because the positional accuracy of the base will directly translate to the accuracy of the rover. Said differently, if your base's calculated position is off by 100cm, so will every reading your rover makes. For many applications, such as surveying, this is acceptable since the Surveyor may only be concerned with measuring the lengths of property lines or features. If you’re looking for maximum accuracy consider installing a [permanent static base with a fixed antenna](permanent_base.md). We were able to pinpoint the antenna on the top of SparkFun with an incredible accuracy [+/-2mm of accuracy](img/Corrections/SparkFun_PPP_Results.png) using PPP!

## Other Sources

There are a large number of networks run throughout the world. Be sure to dig a bit to find a local correction source near you.

[![Map of RTK2Go Stations](img/Corrections/SparkFun%20NTRIP%205%20-%20RTK2Go%20Map.png)](http://monitor.use-snip.com/?hostUrl=rtk2go.com&port=2101)

* RTK2go offers a [list](http://monitor.use-snip.com/?hostUrl=rtk2go.com&port=2101) and map (click 'View all' from the list) of stations using their public casting service. While none of these stations have been verified as accurate, it can be a decent starting point to do a 'quick' test of your system.