# Configure with Ethernet

<!--
Compatibility Icons
====================================================================================

:material-radiobox-marked:{ .support-full title="Feature Supported" }
:material-radiobox-indeterminate-variant:{ .support-partial title="Feature Partially Supported" }
:material-radiobox-blank:{ .support-none title="Feature Not Supported" }
-->

<div class="grid cards fill" markdown>

- Torch: [:material-radiobox-blank:{ .support-none }]( title ="Feature Not Supported" )
- EVK: :material-radiobox-marked:{ .support-full title="Feature Supported" }

</div>

During Ethernet configuration, the RTK device will present a webpage that is viewable from a desktop/laptop connected to the local network.

![SparkFun RTK Ethernet Configuration Interface](<img/WiFi Config/SparkFun RTK Ethernet Config - Crop.png>)

*SparkFun RTK Ethernet Configuration Interface*

## RTK EVK

To get into Ethernet configuration follow these steps:

1. Power on the RTK EVK and connect it to your Ethernet network using the supplied cable
2. Once the device has started, put the RTK EVK into Ethernet config mode by clicking the Mode button on the front panel. The first click opens the mode menu, successive clicks select the next menu option. Keep clicking until **Cfg Eth** is highlighted, then do a quick double-click to select it. Note that it is only possible to put the EVK into this mode via the Mode button, requiring physical access to the EVK. Remote configuration is only possible after putting the EVK into this mode.

    ![SparkFun RTK EVK Mode Menu](<img/Displays/24342-RTK-EVK-Action-Screen_GIF_750ms.gif>)

    *SparkFun RTK EVK Mode Menu*

3. The RTK EVK will reboot into a dedicated Configure-Via-Ethernet mode
4. By default, the RTK EVK uses DHCP, obtaining its IP address from your DHCP server. The IP address is displayed on the OLED display. You can use a fixed IP address if desired. See [Ethernet Menu](menu_ethernet.md) for more details.

    ![SparkFun RTK EVK Config Ethernet](<img/Displays/SparkFun RTK EVK Ethernet Config.png>)

    *SparkFun RTK EVK Config Ethernet*

5. Open a browser (Chrome is preferred) on your computer and type the EVK's IP address into the address bar. The web config page will open.

![SparkFun RTK Ethernet Configuration Interface](<img/WiFi Config/SparkFun RTK Ethernet Config - Main Interface.png>)

*SparkFun RTK Ethernet Configuration Interface*

## File Manager

![List of files in file manager](<img/WiFi Config/SparkFun%20RTK%20WiFi%20Config%20File%20Manager.png>)

On devices that support an external SD card, a file manager is shown if an SD card is detected. This is a handy way to download files to a local device (cell phone or laptop) as well as delete any unneeded files. The SD size and free space are shown. Files may be uploaded to the SD card if needed.

Additionally, clicking on the top checkbox will select all files for easy removal of a large number of files.

## Saving and Exit

![Save and Exit buttons](<img/WiFi Config/SparkFun RTK WiFi Config - Save Steps.png>)

Once settings are input, please press ‘Save Configuration’. This will validate any settings, show any errors that need adjustment, and send the settings to the unit. The page will remain active until the user presses ‘Exit and Reset’ at which point the unit will exit Ethernet configuration and return to whichever mode was selected in the **System Configuration** tab **System Initial State** drop-down (Base, Rover or NTP).

It is also possible to exit Configure-Via-Ethernet mode by: clicking the Mode button on the front panel; or by opening the serial menu and selecting 'r'.
