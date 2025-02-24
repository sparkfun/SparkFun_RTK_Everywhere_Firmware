# Network Time Protocol Menu

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

</div>

Ethernet-equipped RTK devices can act as an Ethernet Network Time Protocol (NTP) server.

Network Time Protocol has been around since 1985. It is a simple way for computers to synchronize their clocks with each other, allowing the network latency (delay) to be subtracted:

- A client sends an NTP request (packet) to the chosen or designated server
	- The request contains the client's current clock time - for identification
- The server logs the time the client's request arrived and then sends a reply containing:
	- The client's clock time - for identification
	- The server's clock time - when the request arrived at the server
	- The server's clock time - when the reply is sent
	- The time the server's clock was last synchronized - providing the age of the synchronization
- The client logs the time the reply is received - using its own clock

When the client receives the reply, it can deduce the total round-trip delay which is the sum of:

- How long the request took to reach the server
- How long the server took to construct the reply
- How long the reply took to reach the client

This exchange is repeated typically five times, before the client synchronizes its clock to the server's clock, subtracting the latency (delay) introduced by the network.

Having your own NTP server on your network allows tighter clock synchronization as the network latency is minimized.

Ethernet-equipped RTK devices can be placed into dedicated NTP mode, by clicking the **MODE** button until NTP is highlighted in the display and double-clicking there.

<figure markdown>
![Animation of selecting NTP mode](./img/Displays/SparkFun RTK - NTP Select.gif)
<figcaption markdown>
Selecting NTP mode
</figcaption>
</figure>

An Ethernet-equipped RTK device will first synchronize its Real Time Clock (RTC) using the very accurate time provided by the u-blox GNSS module. The module's Time Pulse (Pulse-Per-Second) signal is connected to the ESP32 as an interrupt. The ESP32's RTC is synchronized to Universal Time Coordinate (UTC) on the rising edge of the TP signal using the time contained in the UBX-TIM-TP message.

When an Ethernet-equipped RTK device is in Network Time Protocol (NTP) mode, the display also shows a clock symbol - as shown above. The value next to the clock symbol is the Time Accuracy Estimate (tAcc) from the UBX-NAV-PVT message.

!!! note
	tAcc is the time accuracy estimate for the navigation position solution. The timing accuracy of the TP pulse is significantly better than this. We show the tAcc as we believe it is more meaningful than the TIM-TP time pulse quantization error (qErr) - which is generally zero.

The RTK device will respond to each NTP request within a few 10s of milliseconds.

If desired, you can log all NTP requests to a file on the microSD card, and/or print them as diagnostic messages. The log and messages contain the NTP timing information and the IP Address and port of the Client.

[<figure markdown>
![The system debug menu showing how to enable the NTP diagnostics](./img/NTP/NTP_Diagnostics.png)](./img/NTP/NTP_Diagnostics.png)
<figcaption markdown>
System Debug Menu - NTP Diagnostics (Click for a closer look)
</figcaption>
</figure>

[<figure markdown>
![The logging menu showing how to log the NTP requests](./img/NTP/NTP_Logging.png)](./img/NTP/NTP_Logging.png)
<figcaption markdown>
Logging Menu - Log NTP Requests
</figcaption>
</figure>

### Logged NTP Requests

<figure markdown>
![NTP requests log](./img/NTP/NTP_Log.png)
<figcaption markdown>
</figcaption>
</figure>

NTP uses its own epoch - midnight January 1st, 1900. This is different than the standard Unix epoch - midnight January 1st, 1970 - and the GPS epoch - midnight January 6th, 1980. The times shown in the log and diagnostic messages use the NTP epoch. You can use online calculators to convert between the different epochs:

- [https://weirdo.cloud/](https://weirdo.cloud/)
- [https://www.unixtimestamp.com/](https://www.unixtimestamp.com/)
- [https://www.labsat.co.uk/index.php/en/gps-time-calculator](https://www.labsat.co.uk/index.php/en/gps-time-calculator)

### NTP on Windows

If you want to synchronize your Windows PC to a RTK device running as an NTP Server, here's how to do it:

- Install [Meinberg NTP](https://www.meinbergglobal.com/english/sw/ntp.htm) - this replaces the Windows built-in Time Service

	<figure markdown>
	![Meinberg NTP initial configuration](./img/NTP/NTP_Install_1.png)
	<figcaption markdown>
	</figcaption>
	</figure>

- During the installation, select "Create an initial configuration file" and select the NTP Pool server for your location
- Select "Use fast initial sync mode" for faster first synchronization

	<figure markdown>
	![Meinberg NTP service settings](./img/NTP/NTP_Install_2.png)
	<figcaption markdown>
	</figcaption>
	</figure>

- The next step is to edit the NTP Configuration File
	- Editing the file requires Administrator privileges
	- Open the *Start* menu, navigate to *Meinberg*, right-click on *Edit NTP Configuration* and select *Run as administrator*

		[<figure markdown>
		![Meinberg NTP configuration](./img/NTP/NTP_Config_1_small.png)](./img/NTP/NTP_Config_1.png)
		<figcaption markdown>
		</figcaption>
		</figure>

- Comment the lines in *ntp.conf* which name the pool.ntp servers
- Add an extra *server* line and include the IP Address for your RTK device. It helps to give your RTK device a fixed IP Address first - see [Menu Ethernet](menu_ethernet.md)
- Save the file

	[<figure markdown>
	![Meinberg NTP configuration](./img/NTP/NTP_Config_2_small.png)](./img/NTP/NTP_Config_2.png)
	<figcaption markdown>
	</figcaption>
	</figure>

- Finally, restart the NTP Service
	- Again this needs to be performed with Administrator privileges
	- Open the *Start* menu, navigate to *Meinberg*, right-click on *Restart NTP Service* and select *Open file loctaion*

		[<figure markdown>
		![Meinberg NTP configuration](./img/NTP/NTP_Config_3_small.png)](./img/NTP/NTP_Config_3.png)
		<figcaption markdown>
		</figcaption>
		</figure>

- Right-click on the *Restart NTP Service* and select *Run as administrator*

	<figure markdown>
	![Meinberg NTP configuration](./img/NTP/NTP_Config_4.png)
	<figcaption markdown>
	</figcaption>
	</figure>

- You can check if your PC clock synchronized successfully by opening a *Command Prompt (cmd)* and running *ntpq -pd*

	<figure markdown>
	![Meinberg NTP configuration](./img/NTP/NTP_Config_5.png)
	<figcaption markdown>
	</figcaption>
	</figure>

If enabled, your Windows PC NTP requests will be printed and logged by the RTK device. See [above](#logged-ntp-requests).
