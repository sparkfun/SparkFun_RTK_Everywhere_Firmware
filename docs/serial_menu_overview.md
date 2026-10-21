# Serial Menu Overview

Below is an outline of the serial menu system.  The menus are tailored to
each system so not all menus are found on all systems.  The menus will
expand as items are enabled.  As an example enabling NTRIP from the
'Configure GNSS Receiver' menu adds NTRIP specific entries to that menu.

# Main Menu

[`1) Configure GNSS Receiver`](menu_gnss.md)<br>
[`2) Configure GNSS Messages`](menu_messages.md)<br>
[`3) Configure Base`](menu_base.md)<br>
[`4) Configure Ports`](menu_ports.md)<br>
[`6) Configure WiFi`](menu_wifi.md)<br>
[`7) Configure TCP/UDP`](menu_tcp_udp.md)<br>
[`f) Firmware Update`](menu_firmware.md)<br>
[`i) Configure Corrections Priorities`](menu_corrections_priorities.md)<br>
[`p) Configure PointPerfect`](menu_pointperfect.md)<br>
[`r) Configure Radios`](menu_radios.md)<br>
[`s) Configure System`](menu_system.md)<br>
`t) Configure Instrument Setup`<br>
[`u) Configure User Profiles`](menu_userprofiles.md)<br>
`+) Enter Command Line Mode`<br>
`x) Exit`<br>

## Configure GNSS Receiver Menu

`1) Set measurement rate in Hz: 2.00000`<br>
`2) Set measurement rate in seconds between measurements: 0.50000`<br>
`Note: The measurement rate is overridden to 1Hz when in Base mode.`<br>
`3) Set dynamic model: Survey`<br>
`4) Set Constellations`<br>
`5) Minimum elevation for a GNSS satellite to be used in fix (degrees): 10`<br>
`6) Minimum satellite signal level for navigation (dBHz): 10`<br>
`7) Toggle NTRIP Client: Disabled`<br>
`15) Multipath Mitigation: Enabled`<br>
`16) GNSS-Specific Configuration`<br>
`x) Exit`<br>

## Configure GNSS Messages

`1) Set NMEA Messages`<br>
`2) Set Rover RTCM Messages`<br>
`3) Set Base RTCM Messages`<br>
`10) Reset to Defaults`<br>
`11) Reset to PPP Logging (NMEAx5 / RTCMx4 - 30 second decimation)`<br>
`12) Reset to High-rate PPP Logging (NMEAx5 / RTCMx4 - 1Hz)`<br>
`x) Exit`<br>

## Configure Base

`1) Toggle Base Mode: Use Survey-In`<br>
`2) Set minimum observation time: 60 seconds`<br>
`4) Set required initial positional accuracy before Survey-In: 2.00 meters`<br>
`7) Commonly Used Base Coordinates`<br>
`8) Set RTCM Message Rates for Base Mode`<br>
`9) Set RTCM 1033 Antenna Description`<br>
`10) Toggle NTRIP Server: Disabled`<br>
`x) Exit`<br>

## Configure Ports

`1) Output GNSS data to USB serial: Disabled`<br>
`2) Set USB serial baud rate: 115200 bps`<br>
`x) Exit`<br>

## Configure WiFi

`1) SSID 1:`<br>
`2) Password 1:`<br>
`3) SSID 2:`<br>
`4) Password 2:`<br>
`5) SSID 3:`<br>
`6) Password 3:`<br>
`7) SSID 4:`<br>
`8) Password 4:`<br>
`a) Configure device via WiFi Access Point or connect to WiFi: AP`<br>
`c) Captive Portal: Enabled`<br>
`d) Set default WiFi channel: 0`<br>
`t) Set WiFi timeout: 30000 mSec`<br>
`x) Exit`<br>

## Configure TCP/UDP

`1) TCP Client: Disabled`<br>
`4) TCP Server: Disabled`<br>
`6) UDP Server: Disabled`<br>
`m) MDNS: Enabled`<br>
`n) MDNS host name: rtk`<br>
`t) Broadcast TCP/UDP Server packets over local WiFi or act as Access Point: WiFi`<br>
`u) Broadcast UDP Server packets over local WiFi or act as Access Point: WiFi`<br>
`x) Exit`<br>

## Firmware Update

`a) Automatic firmware updates: Disabled`<br>
`c) Check for product firmware updates: Not requested`<br>
`d) Enable developer options`<br>
`P) Update to latest product specific firmware`<br>
`q) Cancel check and update requests`<br>
`u) Run system update: Not Requested`<br>
`x) Exit`<br>

## Configure Corrections Priorities

`1) Correction source lifetime in seconds: 30`<br>
<br>
`These are the correction sources in order of decreasing priority`<br>
`Enter the uppercase letter to increase the correction priority`<br>
`Enter the lowercase letter to decrease the correction priority`<br>
<br>
`       Priority   Status     Last Seen     Source`<br>
`       --------  --------  -------------   ------`<br>
`A / a)     0     inactive                  External Radio`<br>
`B / b)     1     inactive                  ESP-NOW`<br>
`C / c)     2     inactive                  LoRa Radio`<br>
`D / d)     3     inactive                  Bluetooth`<br>
`E / e)     4     inactive                  USB Serial`<br>
`F / f)     5     inactive                  TCP (NTRIP)`<br>
`G / g)     6     inactive                  PPP HAS/B2b`<br>
`H / h)     7     inactive                  L-Band`<br>
`I / i)     8     inactive                  IP (PointPerfect/MQTT)`<br>
<br>
`x) Exit`<br>

## Configure PointPerfect

`1) Select PointPerfect Service: Disabled`<br>
`i) Show device ID`<br>
`x) Exit`<br>

## Configure Radios

`1) ESP-NOW Radio: Disabled`<br>
`10) LoRa Radio: Disabled`<br>
`20) Set default WiFi channel: 0`<br>
`b) Set Bluetooth Mode: Dual`<br>
`a) Accessory time offset: -1.000s`<br>
`c) Clear BT pairings: No`<br>
`x) Exit`<br>

## Configure System

-----  Mode Switch  -----`<br>
Mode: Rover`<br>
`A) Switch to Base mode using Base Assist`<br>
`B) Switch to Base mode`<br>
`C) Switch to Base Caster mode`<br>
`R) Switch to Rover mode`<br>
`W) Switch to Web Config mode`<br>
`-----  Settings  -----`<br>
`a) Automatic device reboot in minutes: Disabled`<br>
`b) Set Bluetooth Mode: Dual`<br>
`c) Shutdown if not charging: Disabled`<br>
[`d) Debug software`](menu_debug_software.md)<br>
`e) Echo User Input: On`<br>
`g) Enable Beeper: Enabled`<br>
[`h) Debug hardware`](menu_debug_hardware.md)<br>
`l) Debug LFS and SD card files`<br>
[`n) Debug network`](menu_debug_network.md)<br>
[`o) Configure operation`](menu_debug_rtk_operation.md)<br>
[`p) Configure periodic print messages`](menu_debug_periodic_print.md)<br>
`r) Reset all settings to default`<br>
`u) Printed measurement units: meters`<br>
`z) Set time zone offset: 00:00:00`<br>
`~) Setup button: Enabled`<br>
`x) Exit`<br>

### Debug Software

`1) Heap Reporting: Disabled`<br>
`2) Set level to use PSRAM (bytes): 40`<br>
`3) WiFi Connect Timeout (ms): 30000`<br>
`4) Debug malloc/free and new/delete: Disabled`<br>
`10) Print ring buffer offsets: Disabled`<br>
`11) Print ring buffer overruns: Disabled`<br>
`12) Validate incoming RTCM before sending the NTRIP Server: Disabled`<br>
`20) Print Rover accuracy messages: Enabled`<br>
`30) Print states: Enabled`<br>
`31) Print duplicate states: Disabled`<br>
`33) Print boot times: Disabled`<br>
`34) Print partition table: Disabled`<br>
`40) Print LittleFS and settings management: Disabled`<br>
`41) Halt on ESP_RST_PANIC: Disabled`<br>
`50) Task Highwater Reporting: Disabled`<br>
`51) Print task start/stop: Disabled`<br>
`60) Print firmware update states: Disabled`<br>
`70) Point Perfect certificate management: Disabled`<br>
`e) Erase LittleFS`<br>
`r) Force system reset`<br>
`t) Display task list`<br>
`x) Exit`<br>

### Debug hardware

`1) Print battery status messages: Enabled`<br>
`3) Print RTC resyncs: Disabled`<br>
`4) Print log file messages: Disabled`<br>
`5) Print log file status: Enabled`<br>
`7) Print SD and UART buffer sizes: Disabled`<br>
`9) Print GNSS Debugging: Disabled`<br>
`10) Print Correction Debugging: Disabled`<br>
`11) Print Tilt/IMU Debugging: Disabled`<br>
`12) Print Tilt/IMU Compensation Debugging: Disabled`<br>
`13) UM980 direct connect for firmware upgrade`<br>
`14) PSRAM (online): Enabled`<br>
`15) Print ESP-NOW Debugging: Disabled`<br>
`16) Print LoRa Debugging: Disabled`<br>
`17) STM32 direct connect for LoRa firmware upgrade`<br>
`18) Display littleFS stats`<br>
`19) Print CLI Debugging: Disabled`<br>
`20) Delay between CLI LIST prints over BLE: 50ms`<br>
`21) Double-Tap Interval: 250ms`<br>
`22) Print GNSS Config Debugging: Disabled`<br>
`23) Reset GNSS Config`<br>
`24) EA Protocol name: com.sparkfun.rtk`<br>
`25) RTCM buffer debugging: Disabled`<br>
`26) STM32 direct connect for LoRa RX testing`<br>
`27) STM32 dedicated LoRa TX testing`<br>
`28) IM19 direct connect for firmware upgrade`<br>
`c) Display configuration`<br>
`e) Erase LittleFS`<br>
`p) Display product resistor table`<br>
`r) Force system reset`<br>
`x) Exit`<br>

### Debug LFS and SD card files

`0) Select NVM/SFE_Torch_Settings_0.txt <- Current`<br>
`8) Dump NVM/profileNumber.txt`<br>
`d) Dump file: NVM:/SFE_Torch_Settings_0.txt`<br>
`l) List NVM files`<br>
`v) Verify NVM:/SFE_Torch_Settings_0.txt CRC`<br>
`x) Exit`<br>

### Debug network

`1) Print Ethernet diagnostics: Disabled`<br>
`3) Debug WiFi state: Disabled`<br>
`4) Debug Web Server: Disabled`<br>
`10) Debug network layer: Disabled`<br>
`11) Print network layer status: Enabled`<br>
`12) NetworkClient write timeout: 250ms`<br>
`13) Debug AppleAccessory: Disabled`<br>
`20) Debug NTP: Disabled`<br>
`21) Debug NTRIP client state: Disabled`<br>
`22) Debug NTRIP client --> caster GGA messages: Disabled`<br>
`23) Debug NTRIP server state: Disabled`<br>
`24) Debug caster --> NTRIP server GNSS messages: Disabled`<br>
`25) Debug TCP client: Disabled`<br>
`26) Debug TCP server: Disabled`<br>
`27) Debug UDP server: Disabled`<br>
`28) Debug MQTT client data: Disabled`<br>
`29) Debug MQTT client state: Disabled`<br>
`30) Debug HTTP client data: Disabled`<br>
`31) Debug HTTP client state: Disabled`<br>
`r) Force system reset`<br>
`x) Exit`<br>

### Configure operation

`1) Display Reset Counter: 0 - Disabled`<br>
`2) GNSS Serial Timeout: 1`<br>
`3) GNSS Handler Buffer Size: 4096`<br>
`4) GNSS Serial RX Full Threshold: 50`<br>
`6) SPI/SD Interface Frequency: 16 MHz`<br>
`7) SPP RX Buffer Size: 2048`<br>
`8) SPP TX Buffer Size: 32`<br>
`9) UART Receive Buffer Size: 2048`<br>
`10) Force Tilt detect`<br>
`11) Set PPL RTK Fix Timeout (seconds): 180`<br>
`----  Interrupts  ----`<br>
`30) Bluetooth Interrupts Core: 1`<br>
`31) GNSS UART Interrupts Core: 1`<br>
`32) I2C Interrupts Core: 1`<br>
`-------  Tasks  ------`<br>
`50) BT Read Task Core: 1`<br>
`51) BT Read Task Priority: 1`<br>
`52) GNSS Data Handler Core: 1`<br>
`53) GNSS Data Handler Task Priority: 1`<br>
`54) GNSS Read Task Core: 1`<br>
`55) GNSS Read Task Priority: 1`<br>
`x) Exit`<br>

### Configure periodic print messages

`-----  Hardware  -----`<br>
`1) Bluetooth RX: Disabled`<br>
`2) Bluetooth TX: Disabled`<br>
`3) Ethernet IP address: Disabled`<br>
`4) Ethernet state: Disabled`<br>
`5) SD log write data: Disabled`<br>
`7) WiFi state: Disabled`<br>
`8) GNSS RX data: Disabled`<br>
`9) GNSS TX data: Disabled`<br>
`10) GNSS RX byte count: Disabled`<br>
`-----  Software  -----`<br>
`20) Periodic print: 0 (0x0000000000000000)`<br>
`21) Interval (seconds): 15`<br>
`22) CPU idle time: Disabled`<br>
`23) Network state: Disabled`<br>
`24) Ring buffer consumer times: Disabled`<br>
`25) RTK position: Disabled`<br>
`26) RTK state: Enabled`<br>
`27) RTK correction source: Disabled`<br>
`28) Firmware mode: Disabled`<br>
`------  Clients  -----`<br>
`40) NTP server data: Disabled`<br>
`41) NTP server state: Disabled`<br>
`42) NTRIP client data: Disabled`<br>
`43) NTRIP client GGA writes: Disabled`<br>
`44) NTRIP client state: Disabled`<br>
`45) NTRIP server data: Disabled`<br>
`46) NTRIP server state: Disabled`<br>
`47) TCP client data: Disabled`<br>
`48) TCP client state: Disabled`<br>
`49) TCP server client data: Disabled`<br>
`50) TCP server data: Disabled`<br>
`51) TCP server state: Disabled`<br>
`52) MQTT client data: Disabled`<br>
`53) MQTT client state: Disabled`<br>
`54) HTTP client state: Disabled`<br>
`55) Provisioning state: Disabled`<br>
`56) UDP server state: Disabled`<br>
`57) UDP server data: Disabled`<br>
`58) UDP server broadcast data: Disabled`<br>
`59) WebServer state: Disabled`<br>
`60) OTA firmware update state: Disabled`<br>
`-------  Tasks  ------`<br>
`70) btReadTask state: Disabled`<br>
`71) ButtonCheckTask state: Disabled`<br>
`72) gnssReadTask state: Disabled`<br>
`73) handleGnssDataTask state: Disabled`<br>
`74) sdSizeCheckTask state: Disabled`<br>
`75) WebServerTask state: Disabled`<br>
`x) Exit`<br>

## Configure Instrument Setup

`1) Set Antenna Height (a.k.a. Pole Length): 1.800m`<br>
`2) Set Antenna Phase Center: 129.0mm`<br>
`3) Report Tip Altitude: Disabled`<br>
[`4) Tilt Compensation: Enabled`](menu_tilt.md)<br>
`x) Exit`<br>

## Configure User Profiles

`1) Select Profile1 <- Current`<br>
`2) Select (Empty)`<br>
`3) Select (Empty)`<br>
`4) Select (Empty)`<br>
`5) Select (Empty)`<br>
`6) Select (Empty)`<br>
`7) Select (Empty)`<br>
`8) Select (Empty)`<br>
`c) Copy current profile to next empty slot`<br>
`d) Delete profile 'Profile1'`<br>
`n) Edit profile name: Profile1v
`p) Print profile`<br>
`r) Set profile 'Profile1' to factory defaults`<br>
`x) Exit`<br>
