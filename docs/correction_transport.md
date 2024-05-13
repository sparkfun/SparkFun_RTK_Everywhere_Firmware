# Correction Transport

Once a [correction source](correction_sources.md) is chosen, the correction data must be transported from the base to the rover. The RTCM serial data is approximately 530 bytes per second. This section describes the various methods to move correction data from a base to one or more rovers.

RTK calculations require RTCM data to be delivered approximately once per second. If RTCM data is lost or not received by a rover, RTK Fix can still be maintained for many seconds before the device will enter RTK Float mode. If a transport method experiences congestion (ie, cellular latency, Serial Radios dropping packets, etc) the rover(s) can continue in RTK Fix mode even if correction data is not available for multiple seconds.

## WiFi

Torch: ![Feature Supported](img/Icons/GreenDot.png) 

![NTRIP Server setup](<img/WiFi Config/RTK_Surveyor_-_WiFi_Config_-_Base_Config2.jpg>)

Any SparkFun RTK device can be set up as an [NTRIP Server](menu_base.md#ntrip-server). This means the device will connect to local WiFi and broadcast its correction data to the internet. The data is delivered to something called an NTRIP Caster. Any number of rovers can then access this data using something called an NTRIP Client. Nearly *every* GIS application has an NTRIP Client built into it so this makes it very handy.

WiFi broadcasting is the most common transport method of getting RTCM correction data to the internet and to rovers via NTRIP Clients.

![RTK product in NTRIP Client mode](img/Displays/SparkFun_RTK_Rover_NTRIP_Client_Connection.png)

*RTK product showing corrections being downloaded over WiFi in NTRIP Client mode*

Similarly, any SparkFun RTK device can be set up as an [NTRIP Client](menu_gnss.md#ntrip-client). The RTK device will connect to the local WiFi and begin downloading the RTCM data from the given NTRIP Caster and RTK Fix will be achieved. This is useful only if the Rover remains in RF range of a WiFi access point. Because of the limited range, we recommend using a cell phone's hotspot feature rather than a stationary WiFi access point for NTRIP Clients.

## Cellular

Torch: ![Feature Supported](img/Icons/GreenDot.png) 

![SW Maps NTRIP Client](img/SWMaps/SW_Maps_-_NTRIP_Client.jpg)

Using a cell phone is the most common way of transporting correction data from the internet to a rover. This method uses the cell phone's built-in internet connection to obtain data from an NTRIP Caster and then pass those corrections over Bluetooth to the RTK device.

Shown above are SW Map's NTRIP Client Settings. Nearly all GIS applications have an NTRIP Client built in so we recommend leveraging the device you already own to save money. Additionally, a cell phone gives your rover incredible range: a rover can obtain RTCM corrections anywhere there is cellular coverage.

Cellular can even be used in Base mode. We have seen some very inventive users use an old cell phone as a WiFi access point. The base unit is configured as an NTRIP Server with the cellphone's WiFi AP credentials. The base performs a survey-in, connects to the WiFi, and the RTCM data is pushed over WiFi, over cellular, to an NTRIP Caster.

## L-Band

Torch: ![Feature Not Supported](img/Icons/RedDot.png) 

What if you are in the field, far away from WiFi, cellular, radio, or any other data connection? Look to the sky! 

A variety of companies provide GNSS RTK corrections broadcast from satellites over a spectrum called L-Band. [L-Band](https://en.wikipedia.org/wiki/L_band) is any frequency from 1 to 2 GHz. These frequencies have the ability to penetrate clouds, fog, and other natural weather phenomena making them particularly useful for location applications.

These corrections are not as accurate as a fixed base station, and the corrections can require a monthly subscription fee, but you cannot beat the ease of use!

L-Band reception requires specialized RF receivers capable of demodulating the satellite transmissions. Currently, the [RTK Facet L-Band](https://www.sparkfun.com/products/20000) is the only product that supports L-Band correction reception.

## Serial Radios

Torch: ![Feature Not Supported](img/Icons/RedDot.png) 

![Two serial radios](img/Corrections/19032-SiK_Telemetry_Radio_V3_-_915MHz__100mW-01.jpg)

Serial radios, sometimes called telemetry radios, provide what is essentially a serial cable between the base and rover devices. Transmission distance, frequency, maximum data rate, configurability, and price vary widely, but all behave functionally the same. SparkFun recommends the [HolyBro 100mW](https://www.sparkfun.com/products/19032) and the [SparkFun LoRaSerial 1W](https://www.sparkfun.com/products/19311) radios for RTK use.

![Serial radio cable](img/Corrections/17239-GHR-04V-S_to_GHR-06V-S_Cable_-_150mm-01.jpg)

On SparkFun RTK products that have an external radio port, a [4-pin to 6-pin cable](https://www.sparkfun.com/products/17239) is included that will allow you to connect the HolyBro branded radio or the SparkFun LoRaSerial radios to a base and rover RTK device.

![Radio attached to RTK device](img/Corrections/SparkFun_RTK_Surveyor_-_Radio.jpg)

These radios attach nicely to the back or bottom of an RTK device.

The benefit of a serial telemetry radio link is that you do not need to configure anything; simply plug two radios onto two RTK devices and turn them on. 

The downside to serial telemetry radios is that they generally have a much shorter range (often slightly more than a 1-kilometer functional range) than a cellular link can provide.

## Ethernet

Torch: ![Feature Not Supported](img/Icons/RedDot.png) 

Ethernet-equipped RTK devices send and receive correction data via Ethernet.

Please see [Ethernet Menu](menu_ethernet.md) for more details.