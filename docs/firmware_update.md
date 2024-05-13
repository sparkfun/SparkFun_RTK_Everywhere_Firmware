# Update Firmware

Torch: ![Feature Supported](img/Icons/GreenDot.png) 

The device has two primary firmwares:

* Firmware on the ESP32 microcontroller
* Firmware on the GNSS receiver. [See below](firmware_update.md#updating-u-blox-firmware).

The device firmware is displayed in a variety of places:

* Power On (if a display is supported)
* Serial Config Menu
* WiFi Config

![RTK Express with firmware v3.0](<img/Displays/SparkFun%20RTK%20Boot%20Screen%20Version%20Number.png>)

*RTK Express with firmware v3.0*

During power-on, on devices that have a display, the firmware version will be shown.

![Main Menu showing RTK Firmware v3.0-Jan 19 2023](img/Terminal/SparkFun%20RTK%20Main%20Menu.png)

*Main Menu showing RTK Firmware v3.0-Jan 19 2023*

The firmware is displayed when the main menu is opened over a serial connection.

![WiFi Config page showing device firmware v2.7](<img/WiFi Config/SparkFun%20RTK%20WiFi%20Config%20Screen%20Version%20Number.png>)

*WiFi Config page showing device firmware v2.7 and ZED-F9P firmware HPG 1.32*

The firmware is shown at the top of the WiFi config page.

From time to time SparkFun will release new firmware for the RTK product line to add and improve functionality. For most users, firmware can be upgraded over WiFi using the OTA method.

* [OTA Method](firmware_update.md#updating-firmware-over-the-air): Connect over WiFi to SparkFun to download the latest firmware *over-the-air*. This can be done using the serial menu or while in WiFi AP Config Mode. Requires a local WiFi network.
* [GUI Method](firmware_update.md#updating-using-windows-gui): Use the [Windows, Linux, MacOS or Python GUI](https://github.com/sparkfun/SparkFun_RTK_Firmware_Uploader) and a USB cable. (The Python package has been tested on Raspberry Pi)
* [SD Method](firmware_update.md#updating-firmware-from-the-sd-card): Load the firmware on an SD card, then use a serial terminal with the *Firmware Upgrade* menu
* [WiFi Method](firmware_update.md#updating-firmware-from-wifi): Load the firmware over WiFi when the device is in WiFi AP Config Mode
* [CLI Method](firmware_update.md#updating-firmware-from-cli): Use the command line *batch_program.bat*

The OTA method is generally recommended. For more information see [here](firmware_update.md#updating-firmware-over-the-air).

Remember, all SparkFun RTK devices are open source hardware meaning you have total access to the [firmware](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware) and [hardware](https://github.com/sparkfun/SparkFun_RTK_Facet). Be sure to check out each repo for the latest firmware and hardware information.

## Updating Firmware Over-The-Air

![Updating Firmware from WiFi config page](<img/WiFi Config/RTK-Firmware-Update-OTA.gif>)

*Updating the firmware via WiFi config page*

![Updating the firmware via Firmware serial menu](<img/Terminal/SparkFun RTK Everywhere - Firmware Update Menu.png>)

*Updating the firmware via Firmware serial menu*

Firmware can be updated in the System Configuration section of the WiFi Config page, or over the Firmware menu of the serial interface. This makes checking and upgrading a unit very easy.

Additionally, users may opt to check for Beta firmware. This is the latest firmware that may have new features and is meant for testing. Beta firmware is not recommended for units deployed into the field as it may not be stable.

If you have a device with firmware lower than v3.0, you will need to use the [GUI](firmware_update.md#updating-firmware-using-the-uploader-gui) or a method listed below to get to v3.x.

Automatic release firmware update is supported over WiFi. Enabling this feature is done using the serial firmware menu. The polling period is specified in minutes and defaults to once a day. The automatic firmware update only checks for and installs the current SparkFun released firmware versions over top of any:

* Older released versions (continual upgrade)
* Beta firmware versions (newer or older, restore to released version)
* Locally built versions (newer or older, restore to released version)

## Updating Firmware Using The Uploader GUI

![RTK Firmware GUI](img/RTK_Uploader_Windows.png)

*RTK Firmware GUI*

This GUI makes it easy to point and click your way through a firmware update. There are versions for Windows, Linux, MacOS and a Python package installer.

The latest GUI release can be downloaded [here](https://github.com/sparkfun/SparkFun_RTK_Firmware_Uploader/releases).

Download the latest RTK firmware binary file located on the [releases page](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/releases) or from the [binaries repo](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries).

**To Use**

* Attach the RTK device to your computer using a USB cable.
* Turn the RTK device on.
* On Windows, open the Device Manager to confirm which COM port the device is operating on. On other platforms, check ```/dev```.

![Device Manager showing USB-Serial CH340 port on COM27](img/Serial/SparkFun%20RTK%20Firmware%20Uploader%20COM%20Port.jpg)

*Device Manager showing 'USB-Serial CH340' port on COM27*

* Get the latest binary file located on the [releases page](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/releases) or from the [binaries repo](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries).
* Run *RTKUploader.exe* (it takes a few seconds to start)
* Click *Browse* and select the binary file to upload
* Select the COM port previously seen in the Device Manager
* Click *Upload Firmware*

Once complete, the device will reset and power down.

If your RTK 'freezes' after the update, press ```Reset ESP32``` to get it going again.

## Updating Firmware From the SD Card

Torch: ![Feature Not Supported](img/Icons/RedDot.png) 

On devices that support it, the firmware can be loaded from an SD card.

![Firmware update menu](<img/Terminal/SparkFun_RTK_Firmware_Update-ProgressBar.jpg>)

*Firmware update taking place*

Download the latest binary file located on the [releases page](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/releases) or from the [binaries repo](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries).

The firmware upgrade menu will only display files that have the "RTK_Surveyor_Firmware*.bin" file name format so don't change the file names once loaded onto the SD card. Select the firmware you'd like to load and the system will proceed to load the new firmware, then reboot.

**Note:** The firmware is called `RTK_Surveyor_Firmware_vXX.bin` even though there are various RTK products (Facet, Express, Surveyor, etc). We united the different platforms into one. The [RTK Firmware](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware) runs on all our RTK products.

### Force Firmware Loading

In the rare event that a unit is not staying on long enough for new firmware to be loaded into a COM port, the RTK Firmware (as of version 1.2) has an override function. If a file named *RTK_Surveyor_Firmware_Force.bin* is detected on the SD card at boot that file will be used to overwrite the current firmware, and then be deleted. This update path is generally not recommended. Use the [GUI](firmware_update.md#updating-firmware-using-windows-gui) or [WiFi OTA](firmware_update.md#updating-firmware-from-wifi) methods as the first resort.

## Updating Firmware From WiFi

![Advanced system settings](<img/WiFi Config/SparkFun%20RTK%20System%20Config%20Upload%20BIN.png>)

**Note:** Firmware versions 1.1 to 1.9 have an issue that severely limits firmware upload over WiFi and is not recommended; use the [GUI](firmware_update.md#updating-firmware-using-the-uploader-gui) method instead. Firmware versions v1.10 and beyond support direct firmware updates via WiFi.

Firmware may be uploaded to the unit by clicking on 'Upload BIN', selecting the binary such as 'RTK_Surveyor_Firmware_v3_x.bin' and pressing upload. The unit will automatically reset once the firmware upload is complete.

## Updating Firmware From CLI

The command-line interface is also available. You’ll need to download the [RTK Firmware Binaries](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries) repo. This repo contains the binaries but also various supporting tools including esptool.exe and the three binaries required along with the firmware (bootloader, partitions, and app0).

### Windows

Connect a USB A to C cable from your computer to the ESP32 port on the RTK device. Turn the unit on. Now identify the COM port the RTK enumerated at. The easiest way to do this is to open the Device Manager:

![CH340 is on COM6 as shown in Device Manager](img/Serial/RTK_Surveyor_-_Firmware_Update_COM_Port.jpg)

*CH340 is on COM6 as shown in Device Manager*

If the COM port is not showing be sure the unit is turned **On**. If an unknown device is appearing, you’ll need to [install drivers for the CH340](https://learn.sparkfun.com/tutorials/how-to-install-ch340-drivers/all). Once you know the COM port, open a command prompt (Windows button + r then type ‘cmd’).

![batch_program.bat running esptool](<img/Terminal/SparkFun%20RTK%20Firmware%20Update%20CLI.png>)

*batch_program.bat running esptool*

Once the correct COM is identified, run 'batch_program.bat' along with the binary file name and COM port. For example *batch_program.bat RTK_Surveyor_Firmware_v2_0.bin COM6*. COM6 should be replaced by the COM port you identified earlier.

The batch file runs the following commands:

```
esptool.exe --chip esp32 --port COM6 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect 0x1000 ./bin/RTK_Surveyor.ino.bootloader.bin 0x8000 ./bin/RTK_Surveyor_Partitions_16MB.bin 0xe000 ./bin/boot_app0.bin 0x10000 ./RTK_Surveyor_Firmware_vxx.bin
```

Where *COM6* is replaced with the COM port that the RTK product enumerated at and *RTK_Surveyor_Firmware_vxx.bin* is the firmware you would like to load.

**Note:** Some users have reported the 921600bps baud rate does not work. Decrease this to 115200 as needed.

Upon completion, your RTK device will reset and power down.

### macOS / Linux

Get [esptool.py](https://github.com/espressif/esptool). Connect a USB A to C cable from your computer to the ESP32 port on the RTK device. Turn the unit on. Now identify the COM port the RTK enumerated at.

If the COM port is not showing be sure the unit is turned **On**. If an unknown device is appearing, you’ll need to [install drivers for the CH340](https://learn.sparkfun.com/tutorials/how-to-install-ch340-drivers/all). Once you know the COM port, run the following command:

py esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect 0x1000 ./bin/RTK_Surveyor.ino.bootloader.bin 0x8000 ./bin/RTK_Surveyor_Partitions_16MB.bin 0xe000 ./bin/boot_app0.bin 0x10000 ./RTK_Surveyor_Firmware_vxx.bin

Where */dev/ttyUSB0* is replaced with the port that the RTK product enumerated at and *RTK_Surveyor_Firmware_vxx.bin* is the firmware you would like to load.

**Note:** Some users have reported the 921600bps baud rate does not work. Decrease this to 115200 as needed.

Upon completion, your RTK device will reset.

## Updating u-blox Firmware

Torch: ![Feature Not Supported](img/Icons/RedDot.png) 

The following products contain the following u-blox receivers:

* RTK EVK: ZED-F9P

The firmware loaded onto the ZED-F9P and NEO-D9S receivers is written by u-blox and can vary depending on the manufacture date. The RTK Firmware (that runs on the ESP32) is designed to flexibly work with any u-blox firmware. Upgrading the ZED-F9x/NEO-D9S is a good thing to consider but is not crucial to the use of RTK products.

Not sure what firmware is loaded onto your RTK product? Open the [System Menu](menu_system.md) to display the module's current firmware version.

The firmware on u-blox devices can be updated using a [Windows-based GUI](firmware_update.md#updating-using-windows-gui) or [u-center](firmware_update.md#updating-using-u-center). A CLI method is also possible using the `ubxfwupdate.exe` tool provided with u-center. Additionally, u-blox offers the source for the ubxfwupdate tool that is written in C. It is currently released only under an NDA so contact your local u-blox Field Applications Engineer if you need a different method.

### Update Using Windows GUI

![SparkFun u-blox firmware update tool](img/SparkFun%20RTK%20Facet%20L-Band%20u-blox%20Firmware%20Update%20GUI.png)

*SparkFun RTK u-blox Firmware Update Tool*

The [SparkFun RTK u-blox Firmware Update Tool](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/tree/main/u-blox_Update_GUI) is a simple Windows GUI and python script that runs the ubxfwupdate.exe tool. This allows users to directly update module firmware without the need for u-center. Additionally, this tool queries the module to verify that the firmware type matches the module. Because the RTK Facet L-Band contains two u-blox modules that both appear as identical serial ports, it can be difficult and perilous to know which port to load firmware. This tool prevents ZED-F9P firmware from being accidentally loaded onto a NEO-D9S receiver and vice versa.

The SparkFun RTK u-blox Firmware Update Tool will only run on Windows as it relies upon u-blox's `ubxfwupdate.exe`. The full, integrated executable for Windows is available [here](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/raw/main/u-blox_Update_GUI/Windows_exe/RTK_u-blox_Update_GUI.exe).

* Attach the RTK device's USB port to your computer using a USB cable
* Turn the RTK device on
* Open Device Manager to confirm which COM port the device is operating on

![Device Manager showing USB Serial port on COM14](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/raw/main/u-blox_Update_GUI/SparkFun_RTK_u-blox_Updater_COM_Port.jpg)

*Device Manager showing USB Serial port on COM14*

* Get the latest binary firmware file from the [ZED Firmware](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/tree/main/ZED%20Firmware), [NEO Firmware](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/tree/main/NEO%20Firmware) folder, or the [u-blox](https://www.u-blox.com/) website
* Run *RTK_u-blox_Update_GUI.exe* (it takes a few seconds to start)
* Click the Firmware File *Browse* and select the binary file for the update
* Select the COM port previously seen in the Device Manager
* Click *Update Firmware*

Once complete, the u-blox module will restart.

### Update Using u-center

If you're familiar with u-center a tutorial with step-by-step instructions for locating the firmware version as well as changing the firmware can be found in [How to Upgrade Firmware of a u-blox Receiver](https://learn.sparkfun.com/tutorials/how-to-upgrade-firmware-of-a-u-blox-gnss-receiver/all).

### ZED-F9P Firmware Changes

This module is used in the Surveyor, Express, and Facet. It is capable of both Rover *and* base modes.

Most of these binaries can be found in the [ZED Firmware/ZED-F9P](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/tree/main/ZED%20Firmware/ZED-F9P) folder.

All field testing and device-specific performance parameters were obtained with ZED-F9P v1.30.

* v1.32 has a few SPARTN protocol-specific improvements. See [release notes](https://www.u-blox.com/sites/default/files/documents/ZED-F9P-FW100-HPG132_RN_UBX-22004887.pdf). This firmware is required for use with the NEO-D9S and the decryption of PMP messages.

* v1.30 has a few RTK and receiver performance improvements, I<sup>2</sup>C communication improvements, and most importantly support for SPARTN PMP packets. See [release notes](https://www.u-blox.com/sites/default/files/ZED-F9P-FW100-HPG130_RN_UBX-21047459.pdf).

* v1.13 has a few RTK and receiver performance improvements but introduces a bug that causes the RTK Status LED to fail when SBAS is enabled. See [release notes](https://content.u-blox.com/sites/default/files/ZED-F9P-FW100-HPG113_RN_%28UBX-20019211%29.pdf).

* v1.12 has the benefit of working with SBAS and an operational RTK status signal (the LED illuminates correctly). See [release notes](https://content.u-blox.com/sites/default/files/ZED-F9P-FW100-HPG112_RN_%28UBX-19026698%29.pdf).

### NEO-D9S Firmware Changes

This module is used in the Facet L-Band to receive encrypted PMP messages over ~1.55GHz broadcast via a geosynchronous Inmarsat.

This binary file can be found in the [NEO Firmware](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/tree/main/NEO%20Firmware) folder.

* v1.04 Initial release.

As of writing, no additional releases of the NEO-D9S firmware have been made.

## Updating UM980 Firmware

1. Download the latest UM980 firmware. As of writing, the UM980 firmware is [v11833](https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Torch/main/UM980_Firmware/UM980_R4.10Build11833.pkg). See the [RTK Torch repo](https://github.com/sparkfun/SparkFun_RTK_Torch) for the latest firmware.

2. Download and install [UPrecise](https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Torch/main/UM980_Firmware/uprecise-v2-0.exe). UPrecise is the software created by Unicore to configure and view output from Unicore GNSS modules. v2 of the software can be downloaded [here](https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Torch/main/UM980_Firmware/uprecise-v2-0.exe) but newer versions may be available.

    ![Serial Configuration via Arduino Serial Terminal](<img/Terminal/SparkFun RTK Everywhere - System Menu UM980 Direct Connect.png>)

    *Serial Configuration via Arduino Serial Terminal* 

3. Put the device into passthrough mode. From the main menu navigate to System 's'->Hardware 'h'->UM980 Direct Connect '13'. This will put the device into a pass-through mode, but with special character monitoring that allows the UM980 to be reset when bootloading is detected. Close the terminal connection so that UPrecise can connect to the COM port. **Note:** Some terminals cause the ESP32 to reset when closing the port. This reset causes the UM980 pass-through mode to exit. TeraTerm is known to cause this issue. To enter pass-through mode we recommend using the Arudino terminal.

    ![Connect button and connection settings in UPrecise](<img/UPrecise/SparkFun RTK Everywhere - UPrecise Inteface Connect.png>)

    *Connect button and connection settings in UPrecise*

4. Select the COM port and set the baud rate to 115200, then press 'Connect'

    ![Receiver Upgrade Button](<img/UPrecise/SparkFun RTK Everywhere - UPrecise Inteface Receiver Upgrade.png>)

    *Receiver Upgrade Button*

5. Once connected, press the **Receiver Upgrade** button.

    ![Firmware selection window](<img/UPrecise/SparkFun RTK Everywhere - UPrecise Inteface Firmware Selection.png>)

    *Firmware selection window*

6. Select the latest firmware file that was previously downloaded (See the [RTK Torch repo](https://github.com/sparkfun/SparkFun_RTK_Torch) for the latest firmware). Then press Start.

    ![Firmware update process](<img/UPrecise/SparkFun RTK Everywhere - UPrecise Inteface Firmware Upload.png>)

    *Firmware update process*

7. The update process can take multiple minutes. Once completed, close UPrecise, and power cycle the RTK Torch.

    ![UM980 Firmware version shown in System Menu](<img/Terminal/SparkFun RTK Everywhere - System Menu UM980 Firmware.png>)

    *UM980 Firmware version shown in System Menu*

8. Upon power-up, the firmware will be updated. Open the System Menu to confirm the firmware version.

## Compiling Source

### Windows

The SparkFun RTK Everywhere Firmware is compiled using Arduino CLI (currently [v0.35.3](https://github.com/arduino/arduino-cli/releases)). To compile:

1. Install [Arduino CLI](https://github.com/arduino/arduino-cli/releases).

2. Install the ESP32 core for Arduino:

        arduino-cli core install esp32:esp32@2.0.11

**Note**: Use v2.0.11 of the core. 

**Note:** We use the 'ESP32 Dev Module' for pin numbering. 

3. Obtain each of the libraries listed in [the workflow](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/blob/main/.github/workflows/compile-rtk-everywhere.yml#L72) either by using git or the Arduino CLI [library manager](https://arduino.github.io/arduino-cli/0.21/commands/arduino-cli_lib_install/). Be sure to obtain the version of the library reflected in the [workflow](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/blob/main/.github/workflows/compile-rtk-everywhere.yml#L72).

4. RTK Everywhere uses a custom partition file. Download the [RTKEverywhere.csv](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/blob/main/Firmware/RTKEverywhere.csv)file.

5. Add *RTKEverywhere.csv* partition table to the Arduino partitions folder. It should look seomthing like

        C:\Users\\[user name]\AppData\Local\Arduino15\packages\esp32\hardware\esp32\2.0.11\tools\partitions\RTKEverywhere.csv

    This will increase the program partitions, as well as the SPIFFs partition to utilize the full 16MB of flash.


6. Compile using the following command
        arduino-cli --fqbn esp32:esp32:esp32:FlashSize=16M,PSRAM=enabled, compile 'Firmware/RTK_Everywhere' --build-property build.partitions=RTKEverywhere;

7. Once compiled, upload to the device using the following command where `[COM_PORT]` is the COM port on which the RTK device is located (ie `COM42`).

        arduino-cli upload -p [COM_PORT] --fqbn esp32:esp32:esp32:UploadSpeed=512000,FlashSize=16M 'Firmware/RTK_Everywhere'

If you are seeing the error:

> text section exceeds available space ...

You have not replaced the partition file correctly. See the 'Change Partition table' step inside the [Windows instructions](firmware_update.md#windows_1).

**Note:** There are a variety of compile guards (COMPILE_WIFI, COMPILE_AP, etc) at the top of RTK_Everywhere.ino that can be commented out to remove them from compilation. This will greatly reduce the firmware size and allow for faster development of functions that do not rely on these features (serial menus, system configuration, logging, etc).

### Ubuntu 20.04

#### Virtual Machine

Execute the following commands to create the Linux virtual machine:

1. Using a browser, download the Ubuntu 20.04 Desktop image

2. virtualbox

    1. Click on the new button
    2. Specify the machine Name, e.g.: Sparkfun_RTK_20.04
    3. Select Type: Linux
    4. Select Version: Ubuntu (64-bit)
    5. Click the Next> button
    6. Select the memory size: 7168
    7. Click the Next> button
    8. Click on Create a virtual hard disk now
    9. Click the Create button
    10. Select VDI (VirtualBox Disk Image)
    11. Click the Next> button
    12. Select Dynamically allocated
    13. Click the Next> button
    14. Select the disk size: 128 GB
    15. Click the Create button
    16. Click on Storage
    17. Click the empty CD icon
    18. On the right-hand side, click the CD icon
    19. Click on Choose a disk file...
    20. Choose the ubuntu-20.04... iso file
    21. Click the Open button
    22. Click on Network
    23. Under 'Attached to:' select Bridged Adapter
    24. Click the OK button
    25. Click the Start button

3. Install Ubuntu 20.04

4. Log into Ubuntu

5. Click on Activities

6. Type terminal into the search box

7. Optionally install the SSH server

    1. In the terminal window
        1. sudo apt install -y net-tools openssh-server
        2. ifconfig

        Write down the IP address

    2. On the PC
        1. ssh-keygen -t rsa -f ~/.ssh/Sparkfun_RTK_20.04
        2. ssh-copy-id -o IdentitiesOnly=yes -i ~/.ssh/Sparkfun_RTK_20.04  &lt;username&gt;@&lt;IP address&gt;
        3. ssh -Y &lt;username&gt;@&lt;IP address&gt;

#### Build Environment

Execute the following commands to create the build environment for the SparkFun RTK Everywhere Firmware:

1. sudo adduser $USER dialout
2. sudo shutdown -r 0

    Reboot to ensure that the dialout privilege is available to the user

3. sudo apt update
4. sudo apt install -y  git  gitk  git-cola  minicom  python3-pip
5. sudo pip3 install pyserial
6. mkdir ~/SparkFun
7. mkdir ~/SparkFun/esptool
8. cd ~/SparkFun/esptool
9. git clone https://github.com/espressif/esptool .
10. cd ~/SparkFun
11. nano serial-port.sh

    Insert the following text into the file:

    ```C++
    #!/bin/bash
    #   serial-port.sh
    #
    #   Shell script to read the serial data from the RTK Express ESP32 port
    #
    #   Parameters:
    #       1:  ttyUSBn
    #
    sudo minicom -b 115200 -8 -D /dev/$1 < /dev/tty
    ```

12. chmod +x serial-port.sh
13. nano new-firmware.sh

    Insert the following text into the file:

    ```C++
    #!/bin/bash
    #   new-firmware.sh
    #
    #   Shell script to load firmware into the RTK Express via the ESP32 port
    #
    #   Parameters:
    #      1: ttyUSBn
    #      2: Firmware file
    #
    sudo python3 ~/SparkFun/RTK_Binaries/Uploader_GUI/esptool.py --chip esp32 --port /dev/$1 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect \
    0x1000   ~/SparkFun/RTK_Binaries/bin/RTK_Surveyor.ino.bootloader.bin \
    0x8000   ~/SparkFun/RTK_Binaries/bin/RTK_Surveyor_Partitions_16MB.bin \
    0xe000   ~/SparkFun/RTK_Binaries/bin/boot_app0.bin \
    0x10000  $2
    ```

14. chmod +x new-firmware.sh
15. nano new-firmware-4mb.sh

    Insert the following text into the file:

    ```C++
    #!/bin/bash
    #   new-firmware-4mb.sh
    #
    #   Shell script to load firmware into the 4MB RTK Express via the ESP32 port
    #
    #   Parameters:
    #      1: ttyUSBn
    #      2: Firmware file
    #
    sudo python3 ~/SparkFun/RTK_Binaries/Uploader_GUI/esptool.py --chip esp32 --port /dev/$1 --baud 921600 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 80m --flash_size detect \
    0x1000   ~/SparkFun/RTK_Binaries/bin/RTK_Surveyor.ino.bootloader.bin \
    0x8000   ~/SparkFun/RTK_Binaries/bin/RTK_Surveyor_Partitions_4MB.bin \
    0xe000   ~/SparkFun/RTK_Binaries/bin/boot_app0.bin \
    0x10000  $2
    ```

16. chmod +x new-firmware-4mb.sh

    Get the SparkFun RTK Everywhere Firmware sources

17. mkdir ~/SparkFun/RTK
18. cd ~/SparkFun/RTK
19. git clone https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware .

    Get the SparkFun RTK binaries

20. mkdir ~/SparkFun/RTK_Binaries
21. cd ~/SparkFun/RTK_Binaries
22. git clone https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries.git .

    Install the Arduino IDE

23. mkdir ~/SparkFun/arduino
24. cd ~/SparkFun/arduino
25. wget https://downloads.arduino.cc/arduino-1.8.15-linux64.tar.xz
26. tar -xvf ./arduino-1.8.15-linux64.tar.xz
27. cd arduino-1.8.15/
28. sudo ./install.sh

    Add the ESP32 support

29. Arduino

    1. Click on File in the menu bar
    2. Click on Preferences
    3. Go down to the Additional Boards Manager URLs text box
    4. Only if the textbox already has a value, go to the end of the value or values and add a comma
    5. Add the link: https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
    6. Note the value in Sketchbook location
    7. Click the OK button
    8. Click on File in the menu bar
    9. Click on Quit

    Get the required external libraries, then add to the Sketchbook location from above

30. cd   ~/Arduino/libraries
31. mkdir AsyncTCP
32. cd AsyncTCP/
33. git clone https://github.com/me-no-dev/AsyncTCP.git .
34. cd ..
35. mkdir ESPAsyncWebServer
36. cd ESPAsyncWebServer
37. git clone https://github.com/me-no-dev/ESPAsyncWebServer .

    Connect the Config ESP32 port of the RTK to a USB port on the computer

38. ls /dev/ttyUSB*

    Enable the libraries in the Arduino IDE

39. Arduino

    1. From the menu, click on File
    2. Click on Open...
    3. Select the ~/SparkFun/RTK/Firmware/RTK_Surveyor/RTK_Surveyor.ino file
    4. Click on the Open button

    Select the ESP32 development module

    5. From the menu, click on Tools
    6. Click on Board
    7. Click on Board Manager…
    8. Click on esp32
    9. Select version 2.0.2
    10. Click on the Install button in the lower right
    11. Close the Board Manager...
    12. From the menu, click on Tools
    13. Click on Board
    14. Click on ESP32 Arduino
    15. Click on ESP32 Dev Module

    Load the required libraries

    16. From the menu, click on Tools
    17. Click on Manage Libraries…
    18. For each of the following libraries:

        1. Locate the library
        2. Click on the library
        3. Select the version listed in the compile-rtk-firmware.yml file for the [main](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/blob/main/.github/workflows/compile-rtk-firmware.yml) or the [release_candidate](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/blob/release_candidate/.github/workflows/compile-rtk-firmware.yml) branch
        4. Click on the Install button in the lower right

        Library List:

        * ArduinoJson
        * ESP32Time
        * ESP32-OTA-Pull
        * ESP32_BleSerial
        * Ethernet
        * JC_Button
        * MAX17048 - Used for “Test Sketch/Batt_Monitor”
        * PubSubClient
        * SdFat
        * SparkFun LIS2DH12 Arduino Library
        * SparkFun MAX1704x Fuel Gauge Arduino Library
        * SparkFun Qwiic OLED Graphics Library
        * SparkFun u-blox GNSS v3
        * SparkFun_WebServer_ESP32_W5500

    19. Click on the Close button

    Select the terminal port

    20. From the menu, click on Tools
    21. Click on Port, Select the port that was displayed in step 38 above
    22. Select /dev/ttyUSB0
    23. Click on Upload Speed
    24. Select 230400

    Setup the partitions for the 16 MB flash

    25. From the menu, click on Tools
    26. Click on Flash Size
    27. Select 16MB
    28. From the menu, click on Tools
    29. Click on Partition Scheme
    30. Click on 16M Flash (3MB APP/9MB FATFS)
    31. From the menu click on File
    32. Click on Quit

40. cd ~/SparkFun/RTK/
41. cp  Firmware/app3M_fat9M_16MB.csv  ~/.arduino15/packages/esp32/hardware/esp32/2.0.2/tools/partitions/app3M_fat9M_16MB.csv
