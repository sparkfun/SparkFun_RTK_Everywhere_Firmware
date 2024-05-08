# Corrections Priorities

Torch: ![Feature Supported](img/Icons/GreenDot.png)

![RTK Corrections Priorities Menu](<img/Terminal/SparkFun RTK Everywhere - Corrections Priorities Menu.png>)

*RTK Corrections Priorities Menu*

To achieve an RTK Fix, SparkFun RTK products must be provided with a correction source. An RTK device can obtain corrections from a variety of sources. Below is the list of possible sources (not all platforms support all sources) and their default priorities:

* Bluetooth
* IP (PointPerfect/MQTT)
* TCP (NTRIP)
* L-Band
* External Radio
* LoRa Radio
* ESP-Now

The *Corrections Priorities* menu allows a user to specify which correction source should be given priority. For example, if corrections are provided through Bluetooth and L-Band simultaneously, the corrections from L-Band will be discarded because the Bluetooth source has a higher priority. This prevents the RTK engine from receiving potentially mixed correction signals.

![RTK Corrections Priorities Menu](<img/Terminal/SparkFun RTK Everywhere - Corrections Priorities Menu.png>)

In the serial terminal menu, pressing a letter will increase or decrease the position of a priority. For example, in the image above, pressing **D** will raise the `L-Band` priority above `TCP (NTRIP)`.

Please see [Correction Sources](correction_sources.md) for a description of where to obtain corrections.