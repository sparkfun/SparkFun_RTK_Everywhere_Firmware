# Compiling Source

This is information about how to compile the RTK Everywhere firmware from source. This is for advanced users who would like to modify the functionality of the RTK products.

Please see the [compilation workflow](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/blob/main/.github/workflows/compile-rtk-everywhere.yml#L77) on Github for the latest core and library versions.

## Windows

The SparkFun RTK Everywhere Firmware is compiled using Arduino CLI (currently [v1.0.4](https://github.com/arduino/arduino-cli/releases)). To compile:

1. Install [Arduino CLI](https://github.com/arduino/arduino-cli/releases).
2. Install the ESP32 core for Arduino:

		arduino-cli core install esp32:esp32@3.0.7

	!!! note
		Use v3.0.7 of the core.

	!!! note
		We use the 'ESP32 Dev Module' for pin numbering.

3. Obtain each of the libraries listed in [the workflow](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/blob/main/.github/workflows/compile-rtk-everywhere.yml#L77) either by using git or the Arduino CLI [library manager](https://arduino.github.io/arduino-cli/0.21/commands/arduino-cli_lib_install/). Be sure to obtain the version of the library reflected in the [workflow](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/blob/main/.github/workflows/compile-rtk-everywhere.yml#L77). Be sure to include the external libraries (You may have to enable external library support in the CLI).
4. RTK Everywhere uses a custom partition file. Download the [RTKEverywhere.csv](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/blob/main/Firmware/RTKEverywhere.csv) or [RTKEverywhere_8MB.csv](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/blob/main/Firmware/RTKEverywhere_8MB.csv) for the RTK Postcard.
5. Add *RTKEverywhere.csv* partition table to the Arduino partitions folder. It should look something like

		C:\Users\\[user name]\AppData\Local\Arduino15\packages\esp32\hardware\esp32\3.0.7\tools\partitions\RTKEverywhere.csv

	This will increase the program partitions, as well as the SPIFFs partition to utilize the full 16MB of flash (8MB in the case of the Postcard).

6. Compile using the following command

		arduino-cli compile 'Firmware/RTK_Everywhere' --build-property build.partitions=RTKEverywhere --build-property upload.maximum_size=4055040 --fqbn esp32:esp32:esp32:FlashSize=16M,PSRAM=enabled

8. Once compiled, upload to the device using the following command where `[COM_PORT]` is the COM port on which the RTK device is located (ie `COM42`).

		arduino-cli upload -p [COM_PORT] --fqbn esp32:esp32:esp32:UploadSpeed=512000,FlashSize=16M 'Firmware/RTK_Everywhere'

If you are seeing the error:

> text section exceeds available space ...

You have either not replaced the partition file correctly or failed to include the 'upload.maximum_size' argument in your compile command. See steps 4 through 6 above.

!!! note
	There are a variety of compile guards (COMPILE_WIFI, COMPILE_AP, etc) at the top of RTK_Everywhere.ino that can be commented out to remove them from compilation. This will greatly reduce the firmware size and allow for faster development of functions that do not rely on these features (serial menus, system configuration, logging, etc).

## Ubuntu 20.04

### Virtual Machine

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

### Build Environment

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

		- ArduinoJson
		- ESP32Time
		- ESP32-OTA-Pull
		- ESP32_BleSerial
		- Ethernet
		- JC_Button
		- MAX17048 - Used for “Test Sketch/Batt_Monitor”
		- PubSubClient
		- SdFat
		- SparkFun LIS2DH12 Arduino Library
		- SparkFun MAX1704x Fuel Gauge Arduino Library
		- SparkFun Qwiic OLED Graphics Library
		- SparkFun u-blox GNSS v3
		- SparkFun_WebServer_ESP32_W5500

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
