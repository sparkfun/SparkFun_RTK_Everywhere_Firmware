# Configure with Bluetooth

<!--
Compatibility Icons
====================================================================================

:material-radiobox-marked:{ .support-full title="Feature Supported" }
:material-radiobox-indeterminate-variant:{ .support-partial title="Feature Partially Supported" }
:material-radiobox-blank:{ .support-none title="Feature Not Supported" }
-->

<div class="grid cards fill" markdown>

- EVK: :material-radiobox-marked:{ .support-full title="Feature Supported" }
- Facet mosaic: :material-radiobox-marked:{ .support-full title="Feature Supported" }
- Postcard: :material-radiobox-marked:{ .support-full title="Feature Supported" }
- Torch: :material-radiobox-marked:{ .support-full title="Feature Supported" }
- TX2: :material-radiobox-marked:{ .support-full title="Feature Supported" }

</div>

<figure markdown>

![Configuration menu open over Bluetooth](<./img/Bluetooth/SparkFun%20RTK%20BEM%20-%20Config%20Menu.png>)
<figcaption markdown>
Configuration menu via Bluetooth
</figcaption>
</figure>

Bluetooth-based configuration provides a quick and easy way to navigate the serial menus as if you were connected over a USB cable. For regular users, this is often the preferred configuration method.

By default, the RTK device will broadcast over BLE and Bluetooth SPP simultaneously. Because BLE does not require pairing, it is what is recommended to connect to out of simplicity. For information about Bluetooth pairing, please see [Connecting Bluetooth](connecting_bluetooth.md).

## Terminal Apps

<figure markdown>

![BLESerial nRF for iOS](<./img/Bluetooth/SparkFun RTK BLE Config on iOS.png>)
<figcaption markdown>

[BLESerial nRF](https://apps.apple.com/us/app/bleserial-nrf/id1632235163) for iOS and [Bluetooth Serial Terminal](https://play.google.com/store/apps/details?id=de.kai_morich.serial_bluetooth_terminal&hl=en_US) for Android
</figcaption>
</figure>

For configuration of an RTK device over Bluetooth we recommend [Bluetooth Serial Terminal](https://play.google.com/store/apps/details?id=de.kai_morich.serial_bluetooth_terminal&hl=en_US) for Android (free as of writing) and [BLESerial nRF](https://apps.apple.com/us/app/bleserial-nrf/id1632235163) for iOS (free as of writing). BLESerial nRF can only be used over BLE.

## Entering Bluetooth Echo Mode

<figure markdown>

![Entering the BEM escape characters](<./img/Bluetooth/SparkFun%20RTK%20BEM%20-%20EscapeCharacters.png>)
<figcaption markdown>
Send the `+++` characters to enter Bluetooth Echo Mode
</figcaption>
</figure>

Once connected, the RTK device will report a large amount of NMEA data over the link. To enter Bluetooth Echo Mode send the characters `+++`.

!!! note
	There must be a 2 second gap between any serial sent from a phone to the RTK device, and any escape characters. In almost all cases this is not a problem. Just be sure it's been 2 seconds since an NTRIP source has been turned off and attempting to enter Bluetooth Echo Mode.

<figure markdown>

![The GNSS message menu over BEM](<./img/Bluetooth/SparkFun%20RTK%20BEM%20-%20Config%20Menu.png>)
<figcaption markdown>
The GNSS Messages menu shown over Bluetooth Echo Mode
</figcaption>
</figure>

Once in Bluetooth Echo Mode, any character sent from the RTK unit will be shown in the Bluetooth app, and any character sent from the connected device (cell phone, laptop, etc) will be received by the RTK device. This allows the opening of the config menu as well as the viewing of all regular system output.

For more information about the Serial Config menu please see [Configure with Serial](configure_with_serial.md).

<figure markdown>

![System output over Bluetooth](<./img/Bluetooth/SparkFun%20RTK%20BEM%20-%20System%20Output.png>)
<figcaption markdown>
Exit from the Serial Config Menu
</figcaption>
</figure>

Bluetooth can also be used to view system status and output. Simply exit the config menu using option 'x' and the system output can be seen.

## Exit Bluetooth Echo Mode

To exit Bluetooth Echo Mode simply disconnect Bluetooth. In the Bluetooth Serial Terminal app, this is done by pressing the 'two plugs' icon in the upper right corner.

<figure markdown>

![Exiting Bluetooth Echo Mode](<./img/Bluetooth/SparkFun%20RTK%20BEM%20-%20Exit%20BEM.png>)
<figcaption markdown>
Menu option 'b' for exiting Bluetooth Echo Mode
</figcaption>
</figure>

Alternatively, if you wish to stay connected over Bluetooth but need to exit Bluetooth Echo Mode, use the 'b' menu option from the main menu.

## Serial Bluetooth Terminal Settings

Here we provide some settings recommendations to make the terminal navigation of the RTK device a bit easier.

<figure markdown>

![Disable Time stamps](<./img/Bluetooth/SparkFun%20RTK%20BEM%20-%20Settings%20Terminal.png>)
<figcaption markdown>
Terminal Settings with Timestamps disabled
</figcaption>
</figure>

Disable timestamps to make the window a bit wider, allowing the display of longer menu items without wrapping.

<figure markdown>

![Clear on send](<./img/Bluetooth/SparkFun%20RTK%20BEM%20-%20Settings.png>)
<figcaption markdown>
Clear on send and echo off
</figcaption>
</figure>

Clearing the input box when sending is very handy as well as turning off local echo.
