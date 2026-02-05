# Updating u-blox Firmware

<!--
Compatibility Icons
====================================================================================

:material-radiobox-marked:{ .support-full title="Feature Supported" }
:material-radiobox-indeterminate-variant:{ .support-partial title="Feature Partially Supported" }
:material-radiobox-blank:{ .support-none title="Feature Not Supported" }
-->

<div class="grid cards fill" markdown>

- EVK: :material-radiobox-marked:{ .support-full title="Feature Supported" }
- Facet mosaic: :material-radiobox-blank:{ .support-none title="Feature Not Supported" }
- Postcard: :material-radiobox-blank:{ .support-none title="Feature Not Supported" }
- Torch: :material-radiobox-blank:{ .support-none title="Feature Not Supported" }
- TX2: :material-radiobox-blank:{ .support-none title="Feature Not Supported" }

</div>

The u-blox ZED-F9P is the GNSS receiver inside the RTK EVK. The RTK EVK also contains the u-blox NEO-D9S for receiving L-Band corrections. The following describes how to update the firmware on the ZED-F9P and NEO-D9S modules.

The firmware loaded onto the ZED-F9P and NEO-D9S receivers is written by u-blox and can vary depending on the manufacture date. The RTK Firmware (that runs on the ESP32) is designed to flexibly work with any u-blox firmware. Upgrading the ZED-F9x/NEO-D9S is a good thing to consider but is not crucial to the use of RTK products.

Not sure what firmware is loaded onto your RTK product? Open the [System Menu](menu_system.md) to display the module's current firmware version.

The firmware on u-blox devices can be updated using a [Windows-based GUI](#update-using-windows-gui) or [u-center](#update-using-u-center). A CLI method is also possible using the `ubxfwupdate.exe` tool provided with u-center. Additionally, u-blox offers the source for the ubxfwupdate tool that is written in C. It is currently released only under an NDA so contact your local u-blox Field Applications Engineer if you need a different method.

### Update Using Windows GUI

<figure markdown>
![SparkFun u-blox firmware update tool](./img/SparkFun%20RTK%20Facet%20L-Band%20u-blox%20Firmware%20Update%20GUI.png)
<figcaption markdown>
SparkFun RTK u-blox Firmware Update Tool
</figcaption>
</figure>

The [SparkFun RTK u-blox Firmware Update Tool](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/tree/main/u-blox_Update_GUI) is a simple Windows GUI and python script that runs the ubxfwupdate.exe tool. This allows users to directly update module firmware without the need for u-center. Additionally, this tool queries the module to verify that the firmware type matches the module. Because the RTK Facet L-Band contains two u-blox modules that both appear as identical serial ports, it can be difficult and perilous to know which port to load firmware. This tool prevents ZED-F9P firmware from being accidentally loaded onto a NEO-D9S receiver and vice versa.

The SparkFun RTK u-blox Firmware Update Tool will only run on Windows as it relies upon u-blox's `ubxfwupdate.exe`. The full, integrated executable for Windows is available [here](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/raw/main/u-blox_Update_GUI/Windows_exe/RTK_u-blox_Update_GUI.exe).

- Attach the RTK device's USB port to your computer using a USB cable
- Turn the RTK device on
- Open Device Manager to confirm which COM port the device is operating on

	<figure markdown>
	![Device Manager showing USB Serial port on COM14](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/raw/main/u-blox_Update_GUI/SparkFun_RTK_u-blox_Updater_COM_Port.jpg)
	<figcaption markdown>
	Device Manager showing USB Serial port on COM14
	</figcaption>
	</figure>

- Get the latest binary firmware file from the [ZED Firmware](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/tree/main/ZED%20Firmware), [NEO Firmware](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/tree/main/NEO%20Firmware) folder, or the [u-blox](https://www.u-blox.com/) website
- Run *RTK_u-blox_Update_GUI.exe* (it takes a few seconds to start)
- Click the Firmware File *Browse* and select the binary file for the update
- Select the COM port previously seen in the Device Manager
- Click *Update Firmware*

Once complete, the u-blox module will restart.

### Update Using u-center

If you're familiar with u-center a tutorial with step-by-step instructions for locating the firmware version as well as changing the firmware can be found in [How to Upgrade Firmware of a u-blox Receiver](https://learn.sparkfun.com/tutorials/how-to-upgrade-firmware-of-a-u-blox-gnss-receiver/all).

### ZED-F9P Firmware Changes

This module is used in the RTK EVK. It is capable of both Rover *and* base modes.

Most of these binaries can be found in the [ZED Firmware/ZED-F9P](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/tree/main/ZED%20Firmware/ZED-F9P) folder.

All field testing and device-specific performance parameters were obtained with ZED-F9P v1.30.

- v1.32 has a few SPARTN protocol-specific improvements. See [release notes](https://www.u-blox.com/sites/default/files/documents/ZED-F9P-FW100-HPG132_RN_UBX-22004887.pdf). This firmware is required for use with the NEO-D9S and the decryption of PMP messages.
- v1.30 has a few RTK and receiver performance improvements, I<sup>2</sup>C communication improvements, and most importantly support for SPARTN PMP packets. See [release notes](https://www.u-blox.com/sites/default/files/ZED-F9P-FW100-HPG130_RN_UBX-21047459.pdf).
- v1.13 has a few RTK and receiver performance improvements but introduces a bug that causes the RTK Status LED to fail when SBAS is enabled. See [release notes](https://content.u-blox.com/sites/default/files/ZED-F9P-FW100-HPG113_RN_%28UBX-20019211%29.pdf).
- v1.12 has the benefit of working with SBAS and an operational RTK status signal (the LED illuminates correctly). See [release notes](https://content.u-blox.com/sites/default/files/ZED-F9P-FW100-HPG112_RN_%28UBX-19026698%29.pdf).

### NEO-D9S Firmware Changes

This module is used in the Facet L-Band to receive encrypted PMP messages over ~1.55GHz broadcast via a geosynchronous Inmarsat.

This binary file can be found in the [NEO Firmware](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/tree/main/NEO%20Firmware) folder.

- v1.04 Initial release.

As of writing, no additional releases of the NEO-D9S firmware have been made.
