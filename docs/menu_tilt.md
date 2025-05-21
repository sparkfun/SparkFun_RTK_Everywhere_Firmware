# Tilt Compensation Menu

<!--
Compatibility Icons
====================================================================================

:material-radiobox-marked:{ .support-full title="Feature Supported" }
:material-radiobox-indeterminate-variant:{ .support-partial title="Feature Partially Supported" }
:material-radiobox-blank:{ .support-none title="Feature Not Supported" }
-->

<div class="grid cards fill" markdown>

- EVK: :material-radiobox-blank:{ .support-none title="Feature Not Supported" }
- Facet mosaic: :material-radiobox-blank:{ .support-none title="Feature Not Supported" }
- Postcard: :material-radiobox-blank:{ .support-none title="Feature Not Supported" }
- Torch: :material-radiobox-marked:{ .support-full title="Feature Supported" }

</div>

<figure markdown>
![Tilt Compensation menu](./img/Terminal/SparkFun RTK Everywhere - Tilt Menu.png)
<figcaption markdown>
Tilt Compensation menu
</figcaption>
</figure>

The Tilt Compensation menu controls how the tilt sensor is configured.

- **1** - By default, tilt compensation is enabled but can be disabled if desired.
- **2** - The pole length must be set accurately to enter tilt compensation mode. The default is 1.8 meters.

## Entering Tilt Compensation Mode

To use Tilt Compensation, the user must indicate to the IMU to begin calibration by shaking the device. Then, once the IMU has calculated its position on the end of the pole, Tilt Compensation will be active.

During Tilt Compensation, all outgoing NMEA messages are modified to output the location *of the tip of the pole*. The Data Collector software will not be aware that the position of the GNSS receiver position is being modified. Moving the tip of the pole is allowed. Tilt up to 30 degrees will introduce less than 10mm of inaccuracy. Tilt up to 60 degrees will introduce less than 20mm of inaccuracy.

If the audible beeper is enabled, a long beep will be heard when the IMU starts calibration (by shaking). A short beep will be heard when the IMU completes calibration and Tilt Compensation is active. A short beep will continue every 10 seconds to let the user know Tilt Compensation is being applied.

!!! info "Tilt Affects Point Altitude"
    During tilt compensation the reported location is that *of the tip of the pole*. If Tilt mode is exited for whatever reason, the reported location and altitude of the device will change significantly. It is recommended the user use, listen for, and confirm the audible beep before recording a point in order to avoid recording an inaccurate location and altitude.

Tilt Compensation mode will be exited when the user short presses the power button, and a long beep will be heard. Additionally, Tilt Compensation mode will be exited if RTK Fix is lost. When this happens, the IMU will attempt to re-enter Tilt Compensation mode if RTK Fix is re-achieved.

Tilt compensation mode can be entered using the following steps:

1. The device must be in Rover mode.
2. The device must achieve an RTK Fix.
3. The pole length must be accurately configured. By default, this is 1.8 meters.
4. Once the above requirements are met, the device must be shaken. This is normally a strong up/down vertical motion. However, if it is more comfortable, the device can be positioned horizontally over the shoulder and shaken with a strong forward/backward motion.
5. On devices that support it, the device will emit a chirp once Tilt Mode is started.
6. Place the tip of the device on the ground. Move the head of the device back and forth up to ~30 degrees of tilt. Repeat on the opposite axis.
7. On devices that support it, the device will emit a long chirp once Tilt Compensation Mode is active. The device is now outputting the *location of the tip* of the pole.
8. Exit Tilt Compensation Mode by short pressing the power button.

## Height of Instrument Calculation

During tilt compensation, the length of the pole, and the APC (antenna phase center) must be accurately configured. It is recommended to leave the default APC (116.5mm for the RTK Torch) and use a 1.8m length pole. Once tilt is activated, the IMU will automatically adjust the reported location *from* the APC *to* the pole tip.

The pole length is the distance measured from the pole tip (usually on the ground) to where the bottom of the RTK device interfaces with the pole.

The Antenna Phase Center is the theoretical point where the GNSS signals are effectively received by the antenna. It is not necessarily (and is rarely) the physical surface of the antenna element. The APC is the measured distance between this known point in space and the bottom surface of the RTK device (also known as the Antenna Reference Point or ARP).

How do we find this 'known point in space'? Antenna APCs are estimated using various mathematical models then measured and calibrated using services provided by the [NGS](https://geodesy.noaa.gov/ANTCAL/) or other agencies.