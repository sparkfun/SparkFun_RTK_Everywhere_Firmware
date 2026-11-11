# Firmware Overview

The system consists of several subsystems (large blocks of electronics):
* System-On-a-Chip (SOC) - Espressif ESP32
* GNSS - Various chips (LG290P, Mosaic-X5, UM980, ZED-F9P and ZED-X20P)
* IMU (optional) - IM19
* LoRa (optional) - STM32WL

# Firmware Updates

Each of these subsystems use firmware that may be updated depending
upon the platform.  The update path may be via a physical connection
(USB cable) or network connection.  Please note that not all systems
support [firmware updates](firmware_update.md) on all components or
via network connection.

## Main System Firmware Updates

Starting points:
* [Firmware Menu](menu_firmware.md)
* [ESP32 update](firmware_update_esp32.md)

## GNSS Receiver Firmware Updates

Starting points:
* [Firmware Menu](menu_firmware.md)
* [LG290P](firmware_update_lg290p.md)
* [Mosaic-X5](firmware_update_mosaicX5.md)
* [UM980](firmware_update_um980.md)
* [ZED-F9P](firmware_update_ublox.md)
* [ZED-X20P](firmware_update_ublox.md)

## IMU Firmware Updates

Starting point:
* [Firmware Menu](menu_firmware.md)

## LoRa Firmware Updates

Starting points:
* [Firmware Menu](menu_firmware.md)
* [STM32WL](firmware_update_stm32.md)

# Over-The-Air (OTA) Firmware Updates

The [firmware menu](menu_firmware.md) along with the developer options
support updating various subsystems over WiFi or Ethernet.  This is done
by reading a configuration file and selecting the desired firmware that
matches subsystem components in the product (model).  The next step is
to update the desired subsystems with firmware using the files specified
in the configuration file.

## Configuration File Fields

The following fields exist in the RTK-Everywhere-Variants.csv file:
* model - Specifies the product for the firmware update
* subsystem - Specifies the subsystem for the firmware update
* chip - Specifies the chip for the firmware update
* version_major - Major version number associated with this file
* version_minor - Minor version number associated with this file
* version_patch - Patch number associated with this file
* version_revision - Revision number associated with this file
* release_candidate - 0 (Not an RC build), 1 (Is an RC build)
* file_name - file name on SparkFun's GitHub site or full path and file name on a customer's site
* file_bytes - Size of the file in bytes, verified during firmware update
* file_crc32 - CRC32 compute over the entire file, verified during firmware update

Model (Product) field values:
* `*` - Wildcard to match all systems
* `EVK`
* `Facet X5`
* `Torch`
* `Postcard`
* `TX2`
* `FP`
* `FPL`
* `FPL-T`
* `FPM`
* `FPM-T`
* `FPX`
* `FPX-T`

Subsystem field values:
* `SOC`
* `GNSS`
* `IMU`
* `LORA`

Chip field values:
* `ESP32`
* `IM19`
* `LG290P`
* `Mosaic-X5`
* `STM32WL`
* `UM980`
* `ZED-F9P`
* `ZED-X20P`

The File_CRC32 program in the Firmware/Tools subdirectory of the [source repository](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware/Firmware/Tools) and displays the file size and CRC for the specified file.  These are the last two values on each of the lines in the .CSV file.

## Firmware Update Menu

Each of the requests below determine how the system processes the data
within the
[configuration file](https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/main/RTK-Everywhere-Variants.csv)
when multiple entries specify the same model, subsystem and chip.

### Requests

1) Product release
Use the first file found for the specified product, subsystem and chip<br>
2) Skip update
No firmware update done for the specified subsystem on the specified product<br>
3) Check version
Use the last (non-RC) entry found for the specified product, subsystem and chip<br>
4) Use RC version
Use the release candidate (RC) entry for the specified product, subsystem and chip; if none is listed then use the last (non-RC) entry found for the specified product, subsystem and chip<br>
5) Always update
Use the last (non-RC) entry found for the specified product, subsystem and chip

### Firmware Menu Shortcuts

`c`) Check for product firmware updates - Updates any subsystem that is below the current product release, uses first entry found and version number comparisons to eliminate duplicate firmware loads<br>
`P`) Update to latest product specific firmware - Always updates all firmware, skipping version checks, for the product subsystems even if the same firmware is already loaded

### Developer Specific Option Shortcuts

`C`) Check for newer firmware for all subsystems - Uses the last entry found with version number comparisons for all subsystems on the specified product<br>
`F`) Force updates - Uses the last entry found for all subsystems on the specified product, updates all subsystems skipping version checks

## Customer Configuration - Restricting OTA Firmware Updates

Customers may have the need to use specific firmware versions for an entire project to ensure that consistent results across all project devices.  Example: Project ABC utilizes some FPL-T and FPX-T products but the firmware needs to be frozen for the length of the project to ensure consistent results.

To support this, customers may make a copy of the [configuration file](https://raw.githubusercontent.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/main/RTK-Everywhere-Variants.csv) and place it on their own web server renaming it as needed.  The 'S' command under the firmwware development options specifies the link to this file. Note that http or https may be used with the CSV path specified with the 'S' command or with the file name path specified in the CVS file.

Customers should replace the original contents of the configuration file with just the lines that they need to support their devices.  For the example above, each of the devices should be configured with the 'S' command to point to

`http://your.company/project-directory/ABC.csv`

which contains lines similar to the following:

`*,SOC,ESP32,3,4,0,0,0,RTK_Everywhere_Firmware_v3_4.bin,3347248,0x1beb293e`<br>
`*,GNSS,LG290P,2,1,0,0,0,LG290P03AANR02A01S.pkg,2688712,0xb852abe9`<br>
`*,GNSS,ZED-X20P,2,10,0,0,0,UBX_20_HPG_210_ZED_X20P-01B.512369040097ce18fd3475e71e7c627f.bin,1161860,0x87d9de3f`<br>
`*,IMU,IM19,11,4,1,0,0,20260522185649_VH2_B2.2_A11.4.1_131b44ecee0bdad5670c7.enc,260128,0x30ad8ea6`<br>
`*,LoRa,STM32WL,3,0,1,0,0,SparkPNT_LoRa_3.0.1.bin,81424,0xaa992f57`<br>

The FPL-T contains the follow subsystems and chips:
* SOC, ESP32
* GNSS, LG290P
* IMU, IM19
* LoRa, STM32WL

The FPX-T contains the follow subsystems and chips:
* SOC, ESP32
* GNSS, ZED-X20P
* IMU, IM19
* LoRa, STM32WL

The file_name, file_bytes and file_crc32 fields contains the data describing the file.  By default, the file is found in the subdirectories of the SparkFun [binary repository](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries).  Customers may replace the file name with a full path and file name such as:

`https://your.company/project-directory/subsystem/chip/firmware_file.extension`

# Building the Firmware

These products use open source firmware for the ESP32 (SOC).  [Contributions](contribute.md) to the source and documentation are welcome.

Starting points:
* [Source repository](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware)
* [Binary repository](https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries)
* [Compiling the firmware](firmware_compile.md)
* Updating the product firmware, see the previous sections on this page
