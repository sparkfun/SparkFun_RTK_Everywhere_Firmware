# Creating a Local Base

This section show various options to create your own base station. This is ideal if your Rover will stay within ~20km (12 miles) of the base station.

If setting up a local base setup is too inconvenient, consider using a paid service such as [PointPerfect Flex](using_pointperfect_flex.md)

## Temporary Bases

Temporary Bases are generally setup for a short amount of time (a day or less) and require less up-front work than a permanent base, but generally have lower accuracy.

### Survey-In Base

A Survey-In Base is the default mode for a device when it enters Base mode. The GNSS receiver will monitor its location for approximately 60 seconds. After 60 seconds, generally speaking, the readings are averaged together and that location result is used as the 'fixed' location. All rovers receiving the corrections from this base will be relatively very accurate, but the absolutely accuracy will be low. Said differently, if the Survey-In result is incorrectly 0.7 meters to the west, all rover locations will be 0.7m +/-10mm to the west.

### Augmented Survey-In Base

An Augmented Survey-In Base is where a temporary base is set up to Survey-In its location but is simultaneously provided RTCM corrections so that its Survey-In is done with very precise readings. An augmented base running a Survey-In removes much of the relative inaccuracies from a Rover-Base system. We've found an Augmented Base varies as little as 50mm RMS between intra-day tests, with accuracy within 65mm of a PPP of the same location, same day.

To set up an augmented base the RTK device should be located in a good reception area and provided with RTCM corrections. Let it obtain RTK Fix from a fixed position (on a tripod, for example) in Rover mode. Once an RTK fix is achieved, change the device to temporary Base mode (also called Survey-In). The device will take 60 seconds of positional readings, at which point the fixed position of the base will be set using RTK augmented coordinates. At this point, corrections provided to the base can be discontinued. The Base will begin outputting very accurate RTCM corrections that can be relayed to a rover that is in a less optimal reception setting.

Similarly, a PPP service such as Galileo HAS or BeiDou B2b can be used to increase the accuracy during the Survey-In. The RTK device should be located in a good reception area and allowed to reach PPP convergence (10+ minutes). Once converge, change the device to temporary Base mode (also called Survey-In). The device will take 60 seconds of positional readings, at which point the fixed position of the base will be set using PPP augmented coordinates. The Base will begin outputting very accurate RTCM corrections that can be relayed to a rover that is in a less optimal reception setting.

### Site Located Base

A site located temporary base is when a RTK device is deployed over a position, commonly a stake in the ground, each day that work needs to get done, then the equipment is removed to prevent theft or abuse. A site located base has nearly the accuracy of a permanent base, but requires a one-time investment of a few hours of work to configure.

The first step is to log raw GNSS data for a few hours, then submit it to a processing service to remove as many ambiguities and increase the location accuracy as much possible. With a few hours of logging, location accuracies can be better than 10mm. Once the staked location is known, a base can be quickly redeployed over that known spot during future site visits. The benefit of this longer, logged site survey over a 'Survey-In' style base is the increase in absolute accuracy.

Each day that work needs to be done, the RTK device should be carefully located over the stake or monument with the known coordinates. The RTK device is configured to operate in 'Fixed Base' mode with the coordinates reported by the PPP service, and the base will immediately begin transmitting corrections over whatever transport methods are enable: for example, NTRIP Server, ESP-NOW, LoRa, WiFi AP, etc. Any rover receiving corrections from this type of base will be both relatively and absolutely accurate with the accuracy reported in PPP service report.

#### How to Complete a Site Located Temporary Base

Any of the SparkPNT products can be used to do a site located base. For products that have a microSD interface, the raw satellite data is recorded to the log file. For products that don't have a microSD interface, the device must be connected over USB to an external device capable of logging - this is most often a laptop or tablet.

![Enabling raw logging](<img/Base/SparkFun RTK Base Raw Signal Logging on Postcard.png>)

Above, the Messages Menu, shown on the RTK Postcard, allow PPP logging to be enabled.

![Logging icon shown](<img/Base/SparkFun RTK Base Logging Icon.png>)

Above, the RTK Postcard displays the 'P' logging icon indicating raw signals are being logged.

For the RTK Postcard and RTK EVK, raw logging must be enabled and a microSD card inserted. Once enabled, the logging icon should show a 'P'.

For the RTK Torch, raw logging must be enabled, then the USB port must be configured for GNSS output. Once complete, you will see a stream of NMEA sentences in readable characters, as well as a mix of non-readable binary characters.

For the RTK Facet mosaic, a RINEX output stream must be created and a microSD card inserted.

Logging of data should run for as many hours as is possible. 4 hours is generally considered a minimum, with diminishing returns after 12 hours. The PPP service report will include an estimate of the inaccuracy so if greater accuracy is needed, a longer survey can be re-run.

Once a log of raw GNSS data is obtained, it needs to be processed with RTKCONV. 

**Note:** RTK Facet mosaic users can skip the following RTKCONV step because a RINEX file was directly recorded and can be used with most post processing services.

![Settings in RTKCONV](<img/Base/SparkFun RTK Base RTKCONV.png>)

We must convert the RTCM contained in the log files to RINEX data the post processing services understand. Download [RTKLIB](https://github.com/tomojitakasu/RTKLIB/releases). As of writing, this was v2.4.3. RTKLIB is a collection of tools commonly used to manipulate raw GNSS data. Navigate to the bin folder and open *RTKCONV*. Select the log file that needs to be converted to RINEX. In the above example, the text file is RTCM 3 format, and we enabled 30 second intervals. This decimation process is important: it selects a reading every 30 seconds, removing any extra readings in between. Most PPP services automatically decimate to 30 seconds so this will not result in accuracy loss but will *significantly* reduce your file sizes allowing much faster processing.

![RTKCONV Options](<img/Base/SparkFun RTK Base RTKCONV options.png>)

Above, press the **Options** button and be sure all GNSS signals are enabled and the GPS, GLO, and GAL constellations are selected. Not all PPP services can use this extra data but it's better to have recorded them. If you used a SparkFun SPK6618H (antenna code `SFESPK6618H`) or TOP106 antenna (antenna code `SFETOP106`), you can include an antenna code for additional ambiguity resolution. 

Press convert. 

![Start time window](<img/Base/SparkFun RTK Base RTKCONV Start Time.png>)

When the Start Time window opens, press the File Time button and then OK.

![OBS output file](<img/Base/SparkFun RTK Base RTKCONV OBS File.png>)

Once complete, an OBS file should be output. Zip the OBS file with the ZIP tool of your choice.

![CSRS Login](<img/Base/SparkFun RTK Base RTKCONV CSRS Login.png>)

Log in to [CSRS](https://webapp.csrs-scrs.nrcan-rncan.gc.ca/geod/tools-outils/ppp.php).

![Settings in the CSRS](<img/Base/SparkFun RTK Base RTKCONV CSRS Settings.png>)

Enter your email address and select ITRF as the Processing Mode. Upload the ZIP file of the OBS. Processing can take a few minutes up to a few hours.

![Output from CSRS](<img/Base/SparkFun RTK Base RTKCONV CSRS Output.png>)

Above, we see the location as well as the accuracy estimate of 4mm, 3mm, and 14mm of the lat/lon/alt. When work needs to be done at the site, the RTK device is set on the known location, then configured as a 'Fixed Base' with the PPP location coordinates.

## Permanent Bases

Permanent Bases are the gold-standard for accuracy. They require extra work to set up, but can used for years with proper maintenance. 

### RTK mosaic

[<figure markdown>
![SparkFun RTK mosaic base station kit](https://cdn.sparkfun.com/r/455-455/assets/parts/2/4/0/7/2/23748-RTK-Mosaic-X5-Kit-All-Feature-1.png)](https://www.sparkfun.com/products/23748)
<figcaption markdown>
SparkFun RTK mosaic base station kit
</figcaption>
</figure>

This is the premium RTK base station. The [RTK mosaic](https://www.sparkfun.com/products/23748) is multi-band/multi-signal/multi-constellation capable, includes all the bits you need (minus the antenna mounting hardware) to have a fully fledged CORS (continuously operating reference station). Additionally, the RTK mosaic is capable of transmitting to up to four Casters so you can push correction data simultaneously to your own caster (say RTK2Go) and to casters that may generate some income (ie, [Onocoy](https://www.onocoy.com/) or [Geodnet](https://geodnet.com/)).

[<figure markdown>
![SPK6618H antenna on a magnetic mount](https://docs.sparkfun.com/SparkFun_RTK_mosaic-X5/assets/img/hookup_guide/assembly-gnss-mount_location.jpg)](https://www.sparkfun.com/products/21257)
<figcaption markdown>
Magnetic antenna mount
</figcaption>
</figure>

If you've got a metal roof or parapet, the [magentic mount](https://www.sparkfun.com/products/21257) makes installation even easier.

### RTK Reference Station

[<figure markdown>
![SparkFun RTK mosaic base station kit](https://cdn.sparkfun.com/assets/parts/2/2/5/2/3/SparkFun_GNSS_RTK_Reference_Station_-_12.jpg)](https://www.sparkfun.com/products/22429)
<figcaption markdown>
SparkFun RTK Reference Station kit
</figcaption>
</figure>

The [RTK Reference Station](https://www.sparkfun.com/products/22429) is similar to the RTK mosaic; it has all the parts you need and is a very capable CORS. While being slightly cheaper in price, it is only dual band (L1/L2) capable so the corrections are not *quite* as good as the tri-band [RTK mosaic](https://www.sparkfun.com/products/23748). Additionally, the RTK Reference Station is only capable of transmitting to a single Caster so you can't push correction data simultaneously to say [RTK2Go](https://rtk2go.com/) and [Onocoy](https://www.onocoy.com/) at the same time.

[<figure markdown>
![SPK6618H antenna on a magnetic mount](https://docs.sparkfun.com/SparkFun_RTK_mosaic-X5/assets/img/hookup_guide/assembly-gnss-mount_location.jpg)](https://www.sparkfun.com/products/21257)
<figcaption markdown>
Magnetic antenna mount
</figcaption>
</figure>

If you've got a metal roof or parapet, the [magnetic mount](https://www.sparkfun.com/products/21257) makes installation even easier.

### DIY Base Station

<figure markdown>
![SparkFun Base Station Enclosure](./img/Corrections/Roof_Enclosure.jpg)
<figcaption markdown>
The base station at SparkFun
</figcaption>
</figure>

If you're looking to build a base station on the cheap, or if you're more of a DIYer, checkout our [How to Build a DIY GNSS Reference Station](https://learn.sparkfun.com/tutorials/how-to-build-a-diy-gnss-reference-station/all). This will go into depth about how to bring various pieces together to build your own continuously operating GNSS reference station (CORS).