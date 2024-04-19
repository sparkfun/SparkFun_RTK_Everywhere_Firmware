# System Menu

Torch: ![Feature Supported](img/Icons/GreenDot.png) 

![System Menu accessed over serial](<img/Terminal/SparkFun RTK Everywhere - System Menu.png>)

*System Menu accessed over serial*

The System Menu shows a variety of system information including a full system check to verify what is and what is not online. For example, if an SD card is detected it will be shown as online. Not all RTK devices have all hardware options. For example, the RTK Torch does not have an SD slot so its status and configuration will not be shown.

This menu is helpful when reporting technical issues or requesting support as it displays helpful information about the current GNSS firmware version, and which parts of the unit are online.

## WiFi Interface

Because of the nature of these controls, the WiFi Config page shows different information than the Serial configuration.

![System Config Menu on WiFi Config Page](<img/WiFi Config/SparkFun%20RTK%20WiFi%20Config%20System.png>)

*System Config Menu on WiFi Config Page*

## System Information

![System Menu Header Information](<img/Terminal/SparkFun RTK Everywhere - System Menu Header.png>)

*System Menu Header Information*

The header of the system menu contains various system metrics. 

In order of appearance:

* System Date/time
* Device Mode
* GNSS status including receiver type and firmware version
* Unique ID assigned to the GNSS receiver
* GNSS information including SIV, HPA, Lat/Lon/Alt
* Battery information (if supported)
* Bluetooth MAC (ending) and radio status
* WiFi MAC (full)
* System Uptime
* NTRIP Client/Server uptime (if enabled)
* MQTT Client uptime (if enabled)
* Parser statistics

## Mode Switch

![System Menu Options serial menu](<img/Terminal/SparkFun RTK Everywhere - System Menu Options.png>)

*System Menu Options serial menu*

The device can be in Rover, Base, or WiFi Config mode. The selected mode will be entered once the user exits the menu system.

* **B, R, W, or S** - Change the mode the device is in.

    * Rover - This is the default mode. The device transmits its NMEA and other messages (if enabled) over Bluetooth. It can receive corrections over Bluetooth (or other transport methods such as NTRIP Client) to achieve RTK Fix.
    * Base - The device will reconfigure for base mode. It will begin transmitting corrections over Bluetooth, WiFi (NTRIP Server, TCP, etc), or other (ESP-Now, external radio if compatible, etc).
    * WiFi Config - The device will shut down GNSS operations and begin broadcasting as a WiFi Access Point (or will attempt to connect to local WiFi). Once started, the device can be connected to over WiFi for configuration.
    * Shut Down - If supported, the device will immediately shut down.

## Settings

![System Menu Options serial menu](<img/Terminal/SparkFun RTK Everywhere - System Menu Options.png>)

*System Menu Options serial menu*

* **a** - On devices that support it, a beeper is used to indicate various system states (system power on/off, tilt compensation in use, etc). This can be disabled if desired.

* **b** - Change the Bluetooth protocol. By default, the RTK device begins dual broadcasting over Bluetooth Classic SPP (Serial Port Profile) **and** Bluetooth Low-Energy (BLE). The following options are available: *Dual*, *Classic*, *BLE*, or *Off*. Bluetooth v2.0 SPP (Serial Port Profile) is supported by nearly all data collectors and Android tablets. BLE is used for configuration and to be compatible with Apple iOS-based devices. Additionally, the Bluetooth radio can be turned off.

* **c** - On devices that support it, a device will continue to operate until the battery is exhausted. If desired, a timeout can be entered: If no charging is detected, the device will power off once this amount of time has expired.

* **d** - Enters the [Debug Software menu](menu_debug_software.md) that is for advanced users.

* **e** - Controls the printing of local characters (also known as 'echoing').

* **f** - On devices that support it, show any files on the microSD card (if present).

* **h** - Enters the [Debug Hardware menu](menu_debug_hardware.md) that is for advanced users.

* **n** - Enters the [Debug Network menu](menu_debug_network.md) that is for advanced users.

* **o** - Enters the [Configure RTK operation menu](menu_debug_rtk_operation.md) that is for advanced users.

* **p** - Enters the [Configure periodic print menu](menu_debug_periodic_print.md) that is for advanced users.

* **r** - Reset all settings to default including a factory reset of the GNSS receiver. This can be helpful if the unit has been configured into an unknown or problematic state. See [Factory Reset](menu_system.md#factory-reset).

* **u** - Change between metric and Imperial units. This only modifies the units shown on serial status messages and on the display (if available), it does not change NMEA output.

* **z** - A local timezone in hours, minutes and seconds may be set by pressing 'z'. The timezone values change the RTC clock setting and the file system's timestamps for new files.

* **~** - If desired, the external button(s) can be disabled to prevent accidental mode changes.

**Note:** Bluetooth SPP cannot operate concurrently with ESP-Now radio transmissions. Therefore, if you plan to use the ESP-Now radio system to connect RTK products, the BLE protocol must be used to communicate over Bluetooth to data collectors. Alternatively, ESP-Now works concurrently with WiFi so connecting to a data collector over WiFi can be used.

## Factory Reset

If a device gets into an unknown state it can be returned to default settings using the WiFi or Serial interfaces. 

Note: On devices that support an SD card, a factory reset can also be accomplished by editing the settings files. See [Force a Factory Reset](configure_with_settings_file.md#forcing-a-factory-reset) for more information. 

Note: Log files and any other files on the SD card are *not* removed or modified.

![Issuing a factory reset](<img/Terminal/SparkFun RTK Everywhere - System Menu Factory Reset.png>)

*Issuing and confirming a Factory Reset*

If a device gets into an unknown state it can be returned to default settings. Press 'r' then 'y' to confirm. Factory Default will erase any user settings and reset the internal receiver to stock settings. If SD is supported, any settings file and commonly used coordinate files on the SD card associated with the current profile will be removed.

![Factory Default button](<img/WiFi Config/SparkFun%20RTK%20WiFi%20Factory%20Defaults.png>)

*Enabling and Starting a Factory Reset*

Factory Defaults will erase any user settings and reset the internal receiver to stock settings. To prevent accidental reset the checkbox must first be checked before the button is pressed. Any logs on SD are maintained. Any settings file and commonly used coordinate files on the SD card associated with the current profile will be removed.
