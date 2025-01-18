# Messages Menu

<!--
Compatibility Icons
====================================================================================

:material-radiobox-marked:{ .support-full title="Feature Supported" }
:material-radiobox-indeterminate-variant:{ .support-partial title="Feature Partially Supported" }
:material-radiobox-blank:{ .support-none title="Feature Not Supported" }
-->

<div class="grid cards fill" markdown>

- Torch: :material-radiobox-marked:{ .support-full title="Feature Supported" }
- EVK: :material-radiobox-marked:{ .support-full title="Feature Supported" }

</div>

![Message rate configuration](<img/Terminal/SparkFun RTK Everywhere - Messages Menu.png>)

*The messages configuration menu*

From this menu, a user can control the output of various NMEA, RTCM, and other messages. Any enabled message will be broadcast over Bluetooth *and* recorded to SD (if available).

Because of the large number of configurations possible, we provide a few common settings:

* Reset to Defaults

RTCM can also be enabled in both Rover and Base modes.

## Reset to Defaults

This will turn off all messages and enable the following messages:

* NMEA-GGA, NMEA-GSA, NMEA-GST, NMEA-GSV, NMEA-RMC

These five NMEA sentences are commonly used with SW Maps for general surveying.

## Individual Messages

![Configuring the NMEA messages](<img/Terminal/SparkFun RTK Everywhere - Messages Menu NMEA.png>)

*Configuring the NMEA messages*

There are a large number of messages supported (listed below). Each message sub-menu will present the user with the ability to set the message report rate.

Each message rate input controls which messages are disabled (0) and how often the message is reported (1 = one message reported per 1 fix, 5 = one report every 5 fixes). The message rate range is 0 to 20.

**Note:** The message report rate is the *number of fixes* between message reports. In the image above, with GSV set to 4, the NMEA GSV message will be produced once every 4 fixes. Because the device defaults to a 4Hz fix rate, the GSV message will appear once per second.

The following messages are supported for Bluetooth output and logging (if available):

<table class="table">
	<table>
	<COLGROUP><COL WIDTH=200><COL WIDTH=200><COL WIDTH=200></COLGROUP>
	<tr>
		<td>&#8226; NMEA-DTM</td>
		<td>&#8226; NMEA-GBS</td>
		<td>&#8226; NMEA-GGA</td>
	</tr>
	<tr>
		<td>&#8226; NMEA-GLL</td>
		<td>&#8226; NMEA-GNS</td>
		<td>&#8226; NMEA-GRS</td>
	</tr>
	<tr>
		<td>&#8226; NMEA-GSA</td>
		<td>&#8226; NMEA-GST</td>
		<td>&#8226; NMEA-GSV</td>
	</tr>
	<tr>
		<td>&#8226; NMEA-RMC</td>
		<td>&#8226; NMEA-ROT</td>
		<td>&#8226; NMEA-THS</td>
	</tr>
	<tr>
		<td>&#8226; NMEA-VTG</td>
	<td>&#8226; NMEA-ZDA</td>
	</tr>
	<tr>
	<td>&#8226; RTCM3x-1001</td>
	<td>&#8226; RTCM3x-1002</td>
	<td>&#8226; RTCM3x-1003</td>
	</tr>
	<tr>
	<td>&#8226; RTCM3x-1004</td>
	<td>&#8226; RTCM3x-1005</td>
	<td>&#8226; RTCM3x-1006</td>
	</tr>
	<tr>
	<td>&#8226; RTCM3x-1007</td>
	<td>&#8226; RTCM3x-1009</td>
	<td>&#8226; RTCM3x-1010</td>
	</tr>
	<tr>
	<td>&#8226; RTCM3x-1011</td>
	<td>&#8226; RTCM3x-1012</td>
	<td>&#8226; RTCM3x-1013</td>
	</tr>
	<tr>
	<td>&#8226; RTCM3x-1019</td>
	<td>&#8226; RTCM3x-1020</td>
	<td>&#8226; RTCM3x-1033</td>
	</tr>
	<tr>
	<td>&#8226; RTCM3x-1042</td>
	<td>&#8226; RTCM3x-1044</td>
	<td>&#8226; RTCM3x-1045</td>
	</tr>
	<tr>
	<td>&#8226; RTCM3x-1046</td>
	<td>&#8226; RTCM3x-1071</td>
	<td>&#8226; RTCM3x-1072</td>
	</tr>
	<tr>
	<td>&#8226; RTCM3x-1073</td>
	<td>&#8226; RTCM3x-1074</td>
	<td>&#8226; RTCM3x-1075</td>
	</tr>
	<tr>
	<td>&#8226; RTCM3x-1076</td>
	<td>&#8226; RTCM3x-1077</td>
	<td>&#8226; RTCM3x-1081</td>
	</tr>
	<tr>
	<td>&#8226; RTCM3x-1082</td>
	<td>&#8226; RTCM3x-1083</td>
	<td>&#8226; RTCM3x-1084</td>
	</tr>
	<tr>
	<td>&#8226; RTCM3x-1085</td>
	<td>&#8226; RTCM3x-1086</td>
	<td>&#8226; RTCM3x-1087</td>
	</tr>
	<tr>
	<td>&#8226; RTCM3x-1091</td>
	<td>&#8226; RTCM3x-1092</td>
	<td>&#8226; RTCM3x-1093</td>
	</tr>
	<tr>
	<td>&#8226; RTCM3x-1094</td>
	<td>&#8226; RTCM3x-1095</td>
	<td>&#8226; RTCM3x-1096</td>
	</tr>
	<tr>
	<td>&#8226; RTCM3x-1097</td>
	<td>&#8226; RTCM3x-1104</td>
	<td>&#8226; RTCM3x-1111</td>
	</tr>
	<tr>
	<td>&#8226; RTCM3x-1112</td>
	<td>&#8226; RTCM3x-1113</td>
	<td>&#8226; RTCM3x-1114</td>
	</tr>
	<tr>
	<td>&#8226; RTCM3x-1115</td>
	<td>&#8226; RTCM3x-1116</td>
	<td>&#8226; RTCM3x-1117</td>
	</tr>
	<tr>
	<td>&#8226; RTCM3x-1121</td>
	<td>&#8226; RTCM3x-1122</td>
	<td>&#8226; RTCM3x-1123</td>
	</tr>
	<tr>
	<td>&#8226; RTCM3x-1124</td>
	<td>&#8226; RTCM3x-1125</td>
	<td>&#8226; RTCM3x-1126</td>
	</tr>
	<tr>
	<td>&#8226; RTCM3x-1127</td>
	</tr>

</table></table>
