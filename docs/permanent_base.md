# Creating a Permanent Base

This section show various options to create your own base station. This is ideal if your Rover will stay within ~20km (12 miles) of the base station.

## RTK mosaic

[<figure markdown>
![SparkFun RTK mosaic base station kit](https://cdn.sparkfun.com/r/455-455/assets/parts/2/4/0/7/2/23748-RTK-Mosaic-X5-Kit-All-Feature-1.png)](https://www.sparkfun.com/products/23748)
<figcaption markdown>
SparkFun RTK mosaic base station kit
</figcaption>
</figure>

This is the gold standard for RTK base station kits. The [RTK mosaic](https://www.sparkfun.com/products/23748) is multi-band/multi-signal/multi-constellation capable, includes all the bits you need (minus the antenna mounting hardware) to have a fully fledged CORS (continuously operating reference station). Additionally, the RTK mosaic is capable of transmitting to up to four Casters so you can push correction data simultaneously to your own caster (say RTK2Go) and to casters that may generate some income (ie, [Onocoy](https://www.onocoy.com/) or [Geodnet](https://geodnet.com/)).

[<figure markdown>
![SPK6618H antenna on a magnetic mount](https://docs.sparkfun.com/SparkFun_RTK_mosaic-X5/assets/img/hookup_guide/assembly-gnss-mount_location.jpg)](https://www.sparkfun.com/products/21257)
<figcaption markdown>
Magnetic antenna mount
</figcaption>
</figure>

If you've got a metal roof or parapet, the [magentic mount](https://www.sparkfun.com/products/21257) makes installation even easier.

## RTK Reference Station

[<figure markdown>
![SparkFun RTK mosaic base station kit](https://cdn.sparkfun.com/assets/parts/2/2/5/2/3/SparkFun_GNSS_RTK_Reference_Station_-_12.jpg)](https://www.sparkfun.com/products/22429)
<figcaption markdown>
SparkFun RTK Reference Station kit
</figcaption>
</figure>

The [RTK Reference Station](https://www.sparkfun.com/products/22429) is similar to the RTK mosaic; it has all the parts you need and is a very capable CORS. While being slightly cheaper in price, it is only dual band (L1/L2) capable so the corrections are *quite* as good as the tri-band [RTK mosaic](https://www.sparkfun.com/products/23748). Additionally, the RTK Reference Station is only capable of transmitting to a single Caster so you can't push correction data simultaneously to say [RTK2Go](https://rtk2go.com/) and [Onocoy](https://www.onocoy.com/) at the same time.

[<figure markdown>
![SPK6618H antenna on a magnetic mount](https://docs.sparkfun.com/SparkFun_RTK_mosaic-X5/assets/img/hookup_guide/assembly-gnss-mount_location.jpg)](https://www.sparkfun.com/products/21257)
<figcaption markdown>
Magnetic antenna mount
</figcaption>
</figure>

If you've got a metal roof or parapet, the [magentic mount](https://www.sparkfun.com/products/21257) makes installation even easier.

## DIY Base Station

<figure markdown>
![SparkFun Base Station Enclosure](./img/Corrections/Roof_Enclosure.jpg)
<figcaption markdown>
The base station at SparkFun
</figcaption>
</figure>

If you're looking to build a base station on the cheap, or if you're more of a DIYer, checkout our [How to Build a DIY GNSS Reference Station](https://learn.sparkfun.com/tutorials/how-to-build-a-diy-gnss-reference-station/all). This will go into depth about how to bring various pieces together to build your own continously operating GNSS reference station (CORS).

## Temporary Base

A site survey temporary base is when a RTK device is deployed over a position, commonly a stake in the ground, raw data is logged for a few hours, then submitted to a processing service to remove as many ambiguities and increase the location accuracy as much possible. With a few hours of logging, location accuracies can be better than 10mm. Once the staked location is known, a base can be quickly deployed over that point during future site visits. This is similar to a base+rover setup in that it removes the need to connect to a corrections network. The benefit of this longer, logged site survey over a 'Survey-In' style base is the increase in relative accuracy. Whereas a Survey-In style base can have over 1000mm of inaccuracy in its absolute location that translates to rover inaccuracies, a site surveyed temporary base has absolute location accuracy that can be 10mm or better, which translates to absolute rover accuracy.

### How to Complete a Site Survey Temporary Base

Any of the SparkPNT products can be used to do site survey of a location. For products that have a microSD interface, the raw satellite data is recorded to the log file. For products that don't have a microSD interface, the device must be connected over USB to an external device capable of logging - this is most often a computer.

For the RTK Postcard and RTK EVK, raw logging must be enabled and a microSD card inserted. Once enabled, the logging icon should show a 'P'.

For the RTK Torch, raw logging must be enabled, then the USB port must be connected for GNSS output. Once complete, you will see a stream of NMEA sentences in normal characters, as well as a mix of non-visible binary characters.

For the RTK Facet mosaic, a RINEX output stream must be created and a microSD card inserted.

Logging of data should run for as many hours as is possible. 4 hours is generally considered a minimum, with diminishing returns after 12 hours. Once a site log is obtained, it needs to be processed with RTKCONV. 

Note: RTK Facet mosaic users can skip the RTKCONV step because a RINEX file was directly recorded and can be used with most post processing services.

Next we must convert the RTCM contained in the log files to RINEX data the post processing services understand.
Download RTKLIB. This is a collection of tools commonly used to manipulate raw GNSS data. Open RTKCONV and select the log file. Press the **Options** button and be sure all constellations are selected. 

If you used a SparkFun SPK6618H (antenna code `SFESPK6618H`) or TOP106 antenna (antenna code `SFETOP106`), you can include an antenna code for additional ambiguity resolution. 

If you want to reduce the time window of the log (ie, split out a 1 hour chunk) use the Time Start and Time Stop 
Press convert. When the Start Time window opens, assigned a time and data that starts *before* your log. 