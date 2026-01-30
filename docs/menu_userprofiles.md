# User Profiles Menu

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

![List of user profiles](<./img/Terminal/SparkFun RTK Everywhere - User Profiles Menu.png>)
<figcaption markdown>
User Profiles Menu in the serial interface
</figcaption>
</figure>


<figure markdown>

![List of user profiles](<./img/WiFi Config/SparkFun RTK Profiles Menu.png>)
<figcaption markdown>
User Profiles Menu in the Web Config interface
</figcaption>
</figure>

Profiles are a very powerful feature. A profile is a complete copy of all the settings on the RTK product. Switching profiles changes all the settings in one step. This is handy for creating a complex setup for surveying, and a different setup for an NTRIP-enabled base station. Rather than changing the variety of parameters, a user can simply switch profiles.

Profiles can be selected, renamed, reset to defaults, and completely erased from the **User Profiles** menu.

## User Profile Selection via Display

<figure markdown>

![Multiple Profiles on Menu](<./img/SparkFun_RTK_Facet_Profile.jpg>)
<figcaption markdown>
Multiple Profiles on Menu
</figcaption>
</figure>

On devices that have a display, if more than one profile is defined, the profiles will be displayed and selectable by using the **Power/Setup** button. Only the first 7 characters of a profile's name will be shown on the menu. Once a profile has been selected, the device will reboot using that profile.

## Print Profile

A profile can be printed to the terminal. This dumps the entire settings file and can be helpful for debugging or getting support.

<figure markdown>

![Printing a profile](<./img/Terminal/SparkFun RTK Profiles Print.png>)
<figcaption markdown>
Printing a Profile
</figcaption>
</figure>