# Radios Menu

## ESP-Now

Torch: ![Feature Supported](img/Icons/GreenDot.png) 

![Radio menu showing ESP-Now](<img/Terminal/SparkFun RTK Everywhere - Radios Menu.png>)

*Radio menu showing ESP-Now*

Pressing 'r' from the main menu will open the Configure Radios menu. This allows a user to enable or disable the use of the internal ESP-Now radio.

ESP-Now is a 2.4GHz protocol that is built into the internal ESP32 microcontroller; the same microcontroller that provides Bluetooth and WiFi. ESP-Now does not require WiFi or an Access Point. This is most useful for connecting a Base to Rover (or multiple Rovers) without the need for an external radio. Simply turn two SparkFun RTK products on, enable their radios, pair them, and data will be passed between units.

Additionally, ESP-Now supports point-to-multipoint transmissions. This means a Base can transmit to multiple Rovers simultaneously.

ESP-Now is a free radio included in every RTK product and works well, but it has a few limitations: 

1. Limited use with Bluetooth SPP. The ESP32 is capable of [simultaneously transmitting](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/coexist.html) ESP-Now and Bluetooth LE, but not classic Bluetooth SPP. Unfortunately, SPP (Serial Port Profile) is the most common method for moving data between a GNSS receiver and the GIS software. Because of this, using ESP-Now while connecting to the RTK product using Bluetooth SPP is not stable. SparkFun RTK products support Bluetooth LE and ESP-Now works flawlessly with Bluetooth LE. There are a few GIS applications that support Bluetooth LE including SW Maps. Another option is to use ESP-Now for the Base-Rover link and a GIS app such as [Vespucci](gis_software.md#vespucci) or [QGIS](gis_software.md#qgis) that can obtain PVT data over WiFi (TCP) rather than use Bluetooth to gather the NMEA data from the RTK device.

    ![Max transmission range of about 250m](img/Radios/SparkFun%20RTK%20ESP-Now%20Distance%20Testing.png)

2. Limited range. You can expect two RTK devices to be able to communicate approximately 250m (845 ft) line of sight but any trees, buildings, or objects between the Base and Rover will degrade reception. This range is useful for many applications but may not be acceptable for some applications. We recommend using ESP-Now as a quick, free, and easy way to get started with Base/Rover setups. If your application needs longer RF distances consider cellular NTRIP, WiFi NTRIP, or an external serial telemetry radio plugged into the **RADIO** port.

3. ESP-Now can co-exist with WiFi, but both the receiver and transmitter must be on the same WiFi channel. 

## Pairing

![Pairing Menu](img/Displays/SparkFun%20RTK%20Radio%20E-Pair.png)

On devices that have a display, pressing the Power/Setup button will display the various submenus. Pausing on E-Pair will put the unit into ESP-Now pairing mode. If another RTK device is detected nearby in pairing mode, they will exchange MAC addresses and pair with each other. Multiple Rover units can be paired to a Base in the same fashion.

![Radio menu during AP-Config](<img/WiFi Config/SparkFun%20RTK%20Radio%20Config.png>)

*Radio configuration through WiFi*

The radio system can be configured over WiFi. The radio subsystem is disabled by default. Enabling the radio to ESP-Now will expose the above options. The unit's radio MAC can be seen as well as a button to forget all paired radios. This button is disabled until the 'Enable Forget All Radios' checkbox is checked. The 'Broadcast Override' function changes all data transmitted by this radio to be sent to all radios in the vicinity, instead of only the radios it is paired with. This override feature is helpful if using a base that has not been paired: a base can transmit to multiple rovers regardless if they are paired or not.

![Radio menu showing ESP-Now](<img/Terminal/SparkFun RTK Everywhere - Radios Menu.png>)

*Radio menu showing ESP-Now*

A serial menu is also available. This menu allows users to enter pairing mode, change the channel (ie, set of frequencies) used for communication, view the unit's current Radio MAC, the MAC addresses of any paired radios, as well as the ability to remove all paired radios from memory.

## Setting the WiFi Channel

![Radio menu showing ESP-Now](<img/Terminal/SparkFun RTK Everywhere - Radios Menu.png>)

*Radio menu showing channel 11*

All devices must be on the same WiFi channel to communicate over ESP-Now. Option **4 - Current channel** shows the current channel and allows a user to select a new one. Allowable channel numbers are 1 to 14. By default, devices will communicate on Channel 1. A user may select any channel they prefer.

**Note:** ESP-Now can operate at the same time as WiFi but the user should be aware of the channel numbers of the devices. When a device connects to a WiFi network, the ESP-Now channel number may be altered by the WiFi radio so that the RTK device can communicate with the WiFi Access Point.

Using a single device to communicate corrections to multiple devices (no WiFi involved) is the most common use case for ESP-Now.

Using WiFi on one of the devices in an ESP-Now network is possible. Take the example of a Base that needs to communicate corrections over ESP-Now and will also be pushing the corrections to a Caster over NTRIP using WiFi: The Base is started, WiFi is activated, and the channel is overwritten to 9 (for example) when the device connects to the Access Point. All rovers in the area who wish to obtain corrections over ESP-Now also need to have their channels set to 9.

Using multiple devices on *different* WiFi networks, while attempting to use them in an ESP-Now network, is likely impossible because the device's channel numbers will be modified to match the different channels of the Access Points.
