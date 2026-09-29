# OTA Bootstrap - maintenance notes

OTA_Bootstrap is a small ESP32 program for the production line. It identifies the RTK product it is running on, joins the production Wi-Fi network, and updates every subsystem (IMU, LoRa, GNSS, then the ESP32) to the product release firmware listed in the manifest. The ESP32 update installs the RTK Everywhere firmware, so the unit boots into it and the bootstrap is gone.

Almost all of the update code is copied from `Firmware/RTK_Everywhere`. These notes say where each piece came from, what was changed, and how to keep the bootstrap in step as the firmware and the subsystem update code change.

The copies were taken from branch `addCliUpdate`, commit `3fe88d9f9` (2026-09-28), including the uncommitted `getServerFromUrl()` fix in `System.ino` on that branch.

## Production use

1. Load the bootstrap with the RTK Firmware Uploader. Use the uploader rather than the Arduino IDE: it writes the partition table that matches the unit's flash size (see [Partitions](#partitions)).
2. Open the unit's serial port at 115200 baud. The bootstrap prints the product, the subsystems it found, and a menu.
3. Press `u`.
4. Success: `All updates succeeded`, two short beeps (Torch, Torch X2, Facet FP), and the unit reboots into the RTK Everywhere firmware.
5. Failure: `UPDATE FAILED` and one long beep. The ESP32 is not updated, so the bootstrap is still installed. Fix the cause shown above the message and press `u` again.

Menu:

| Key | Action |
|-----|--------|
| `u` | Update all subsystems |
| `s` | Change the Wi-Fi SSID (saved in NVS, namespace `otaBootstrap`) |
| `p` | Change the Wi-Fi password (saved in NVS) |
| `w` | Restore the default network, `sparkfun-iot` / `iot001100` |
| `d` | Debug output on/off (`settings.debugFirmwareUpdate` in the copied code) |
| `r` | Reset the ESP32 |

Only `u` starts an update. Line endings are ignored, so a terminal that sends Enter after each key does not start one by accident.

## What gets updated

| Product | ESP32 | GNSS | LoRa | IMU |
|---------|-------|------|------|-----|
| Torch | Yes | No (UM980 has no update path) | Yes | Yes (IM19 assumed fitted) |
| Torch X2 | Yes | LG290P | No (not fitted) | No |
| Postcard | Yes | LG290P | No | No |
| Facet FP | Yes | LG290P, mosaic-X5 or ZED-X20P (ZED-F9P: no update path) | Yes (assumed fitted) | Yes, if an IM19 is detected |
| Facet mosaic | Yes | No (not supported on this product) | No | No |
| EVK | Yes | No (ZED-F9P has no update path) | No | No |

This follows the firmware's `otaSubsystemInfoTable` (`OTA.ino`): the same chips have update paths. The bootstrap limits mosaic-X5 to the Facet FP in its table, because `mosaicFirmwareUpdatePort()` refuses every other product.

## How it differs from the firmware's OTA

The flow follows `otaStateGetSystemsToUpdate()` and `otaStateFirmwareUpdate()` in `OTA.ino`, with these deliberate differences:

- **No version check.** Every subsystem is updated to the product release entry, even if it already runs that version. A unit leaving the line always has a known set of firmware, and the bootstrap never has to read versions from the chips.
- **Product release only.** For each subsystem, the first manifest line for that subsystem whose chip is fitted and whose `release_candidate` is 0. That is the firmware's `OTA_REQUEST_PRODUCT_RELEASE` rule. Release candidates are never used.
- **The ESP32 is skipped if anything else failed.** The firmware still updates the ESP32 after a failure, then declines to reboot. The bootstrap stops so it stays installed and the worker can press `u` again.
- **No web, BLE or display reporting.** `firmwareUpdateStatusWebsocket()` does nothing, and progress goes to the serial port only.
- **Torch serial is restored after the LoRa update.** The LoRa update borrows ESP32 UART0 (the USB serial port) at 8E1. The firmware reboots right after, so it never puts it back; the bootstrap restores 115200 8N1 and the USB mux so the GNSS and ESP32 progress can be seen.

Update order is the firmware's: IMU, LoRa, GNSS, ESP32.

## Device detection

| Step | Source in RTK_Everywhere | Bootstrap |
|------|--------------------------|-----------|
| Torch (no ID resistors): look for the BQ40Z50, MP2762A or HUSB238 on I2C (SDA 15, SCL 4) without the MFi chip | `Begin.ino` `testI2cDevices()`, `pinI2CDetectTask()` | `Board.ino` `testI2cDevices()`, run directly instead of in a task |
| Everything else: ID resistor divider on GPIO 35 | `Begin.ino` `identifyBoard()`, `idWithAdc()`; `support.ino` `getProductPropertiesFromAdcValue()` | `Board.ino` `identifyBoard()` |
| Product table (resistors, names) | `settings.h` `productPropertiesTable`, `productHousingPropertiesTable` (tilt possible) | `Bootstrap.h` `productPropertiesTable` |
| Pins and power-on states | `Begin.ino` `beginBoard()` | `Board.ino` `beginBoard()`: only the pins the updates use |
| Facet FP GNSS module present | `System.ino` `gpioExpanderDetectGnssCommon()` | `Board.ino` `gpioExpanderDetectGnss()` |
| Facet FP GNSS type | `GNSS.ino` `gnssDetectReceiverType()` with `lg290pIsPresentOnFacetFP()`, `f9pIsPresentOnFacetFP()`, `x20pIsPresentOnFacetFP()`, `mosaicIsPresentOnFacetFP()` | `Board.ino` `detectFacetFpGnss()`: same order (LG290P, ZED, mosaic-X5) |
| Facet FP tilt (IM19) | `Tilt.ino` `tiltDetect()` | `Board.ino` `detectFacetFpTilt()` |
| Manifest model name | `support.ino` `assembleDeviceName()` (`platformPrefix`) | `Board.ino` `detectSubsystems()` |

Detection differences, to keep in mind when the firmware's detection changes:

- **ZED detection** first checks that a device answers on I2C at 0x42 (where the firmware's u-blox library finds the ZED). Only then does it identify the ZED with the X20P update code's UBX MON-VER poll over the GNSS UART (`ZED_Detect.ino`), instead of the u-blox library. That keeps the u-blox library out of the build. F9P and X20P are told apart the way the firmware does: the F9P reports `MOD=ZED-F9P` and the X20P reports no module name. This is why `X20P_RX_PAYLOAD_MAX` is 300 in `Bootstrap.h` (40 in the firmware): the extension strings would otherwise be cut off.
  - The I2C check matters. Without it, a Facet FP with a mosaic-X5 got about 14 seconds of UBX bytes at seven baud rates on its UART while booting, and then failed detection. The firmware never sends anything on the UART during its ZED test.
- **mosaic-X5 detection** is the firmware's `mosaicIsPresentOnFacetFP()` (`Mosaic_Detect.ino`). It sends `sdio,COM1,auto,RTCMv3+SBF+NMEA+Encapsulate` and waits for `DataInOut`, 5 tries at 115200, then raises COM1 to 460800, tries 5 more times and saves the configuration (`eccf,Current,Boot`). An earlier version used the update code's `mosaicFindCommandPrompt()` (escape sequence, wait for `COM1>`); it did not detect a freshly reset X5 on an FPM-T.
- **The GPIO expander** (TCA9534 at 0x21) is driven through its registers (`gpioExpanderDigitalWrite()` and friends in `Board.ino`) instead of the SparkFun I2C expander library.
- **Nothing is saved.** The firmware records the detected GNSS and tilt in NVM so it only detects once. The bootstrap detects on every boot, and the firmware detects again on its first boot.

On a Facet FP with a mosaic-X5, detection takes longest: the LG290P check has to time out first, and the X5 check alone can take about 12 seconds while the receiver boots.

Tilt (IM19) detection only runs once the GNSS is identified, as in the firmware. If the GNSS is not found, the IMU is not found either.

## Files

"Verbatim" means copied unchanged except where marked `BOOTSTRAP CHANGE` in the code. Each copied block starts with a comment giving its source file and the line range at copy time.

| File | Contents | Source |
|------|----------|--------|
| `OTA_Bootstrap.ino` | `setup()`, menu, Wi-Fi connect, NVS credentials | New |
| `Bootstrap.h` | Types, pins, globals and print/malloc helpers, named as in the firmware so the copies compile | `settings.h`, `OTA.h`, `GNSS_ZED.h` (`UbxMsg`), `support.ino` |
| `Certificates.h` | `GITHUB_RAW_PUBLIC_CERT` (ISRG Root X1) | `settings.h`, verbatim |
| `Board.ino` | Identification, pins, mux and GPIO helpers, GPIO expander, Facet FP GNSS and tilt detection, beeper | Adapted from `Begin.ino`, `System.ino`, `GNSS.ino`, `Tilt.ino`, `support.ino` (see [Device detection](#device-detection)) |
| `ZED_Detect.ino` | `zedDetectOnSerial()` | New. Must sort after `Update_X20P.ino` (uses its `#define`s) |
| `Mosaic_Detect.ino` | `mosaicIsPresentOnFacetFP()`, `mosaicIsPresentOnSerial()`, `mosaicSendWithResponse()` | `GNSS_Mosaic.ino` `mosaicIsPresentOnFacetFP()`, `GNSS_MOSAIC::isPresentOnSerial()`, `GNSS_MOSAIC::sendWithResponse()`. Class methods made free functions; uses `serialGNSS` instead of a local UART2 on the same pins |
| `Update.ino` | Subsystem table, manifest target selection, update loop, progress bar, `reportFatalError()` | Adapted from `OTA.ino` (`otaSubsystemInfoTable`, `otaGetSubsystemInfo()`, `otaIsChipSupported()`, `otaGetUrl()`, `otaGetRequiredUpdates()`, `otaFirmwareUpdate()`, `otaStateFirmwareUpdate()`, `otaCompareVersions()`, `otaFormatVersion()`), `System.ino` (`firmwareUpdateProgressReset()`, `firmwareUpdateProgressCallback()`) |
| `Manifest.ino` | Download and parse the manifest | `CSV.ino`: `csvCleanup()`, `csvGetNumber()`, `csvGetProductLines()`, `csvNextLine()`, `csvOpenCsvFile()`, `csvGetField()`, `csvFileParse()`. Changes: display/dump calls removed; `csvNextLine()` bounded by `bufferEnd`; `csvOpenCsvFile()` clears the counts only on failure |
| `Server.ino` | Certificate lookup, DNS, TLS pre-check, HTTP GET | `System.ino`: `getCertFromServer()` (GitHub only), `getCertFromUrl()`, `getCertName()`, `getServerIpAddress()`, `securelyConnectToServer()`, `serverConnectUsingUrl()`, `getServerFromUrl()` |
| `CRC32.ino` | CRC-32 and CRC combine | `CRC32.ino`, whole file |
| `Update_ESP32.ino` | `otaEsp32StreamFirmware()` | `menuFirmware.ino`, verbatim |
| `Update_LG290P.ino` | `lg290pFirmwareUpdateBegin()`, `lg290pFirmwareUpdate()`, `lg290pFirmwareUpdateEnd()`, `lg290pStreamFirmware()` | `GNSS_LG290P.ino`. `((GNSS_LG290P *)gnss)->` calls go to the library object `lg290p->`; the version print uses `getFirmwareVersionMajor()/Minor()` |
| `Update_Mosaic.ino` | mosaic-X5 update, from `MOSAIC_FW_UPDATE_TRIGGER_CMD` to `mosaicFirmwareUpdate()` | `GNSS_Mosaic.ino`, verbatim |
| `Update_X20P.ino` | ZED-X20P update, from `X20P_FIRMWARE_UPDATE_BAUD` to `x20pDisplayVersion()` | `GNSS_ZED.ino`, verbatim. `UbxMsg` moved to `Bootstrap.h` |
| `Update_LoRa.ino` | `loraEnableBootloader()` .. `loraReset()`, `loraWrite()` .. `loraRead()`, `stm32*()` .. `stm32StreamFirmware()` | `LoRa.ino`, verbatim |
| `Update_IM19.ino` | `imuReset()` .. `im19ResetImu()`, `im19UpdateFirmwareBegin()` .. `im19GetVersionString()` | `Tilt.ino`, verbatim |
| `partitions.csv` | Build partition table | `Firmware/RTKEverywhere_8MB.csv` |

## Building

Match the firmware release build (`Firmware/Dockerfile`, `.github/workflows/compile-rtk-everywhere.yml`):

- ESP32 Arduino core **3.0.7**.
- Libraries: **SparkFun LG290P Quadband RTK GNSS** (with the `updateFirmware*()` API; 3.1.1 at the time of writing) and **SparkFun IM19 IMU** (1.1.1). The LG290P library pulls in the SparkFun Extensible Message Parser. No other RTK libraries are needed.
- Board: ESP32 Dev Module, Flash Size 8MB (the partition table needs it; 16MB units are fine).

Arduino uses `partitions.csv` from the sketch folder automatically.

The bootstrap has no version gate on the ESP32 core the way the firmware does. If the firmware moves to a new core, move the bootstrap with it: the copied HTTP, TLS and `Update` code is tested against the firmware's core.

## Partitions

The ESP32 update writes the RTK Everywhere image into the OTA partition the bootstrap is not running from. That partition must hold the release image:

| Table | App partitions | Fits the release image? |
|-------|----------------|-------------------------|
| `RTKEverywhere_8MB.csv` (bootstrap build) | 2 x 0x3DE000 (3.87 MB) | Yes (v3.3 is 3,236,512 bytes) |
| `RTKEverywhere.csv` (16MB, firmware build) | 2 x 0x600000 (6 MB) | Yes |
| Arduino default 4MB | 2 x 1.25 MB | **No** - the ESP32 update fails |

- Building against the 8MB table caps the bootstrap at 0x3DE000, so the same binary loads on 8MB and 16MB units.
- The partition table on the unit is whatever the loading tool wrote. The RTK Firmware Uploader writes the table for the unit's flash size, so load production units with it.
- Uploading from the Arduino IDE writes `partitions.csv` (the 8MB table) even on a 16MB unit. The firmware still runs, but with a 100 KB LittleFS instead of 3.5 MB.
- **Check the release image size** whenever the firmware grows. If it passes 0x3DE000 (4,055,040 bytes), the 8MB products need a new partition table, and so does this sketch.

## Keeping the bootstrap current

### The manifest changes (new firmware releases)

Nothing to do. The bootstrap reads `RTK-Everywhere-Variants.csv` at update time and installs whatever the product release lines point to. It uses the columns `model`, `subsystem`, `chip`, `version_major` .. `version_revision`, `release_candidate`, `file_name`, `file_bytes` and `file_crc32`. If the column names or the URL change, update `Update.ino` `otaGetTargets()` and `OTA_FIRMWARE_CSV_URL` in `Bootstrap.h` (it must match `RTK_Everywhere.ino`).

Product-specific lines are matched on `model` = `platformPrefix` (for example `TX2`, `FPLT`), as in the firmware. `*` lines apply to every product.

### A subsystem's update code changes in the firmware

1. Find the functions in the [Files](#files) table and copy the new versions from `RTK_Everywhere` over the old ones.
2. Re-apply every `BOOTSTRAP CHANGE` in that file.
3. Check for new calls into the rest of the firmware. Each one needs a matching helper in `Board.ino`, `Update.ino` or `Bootstrap.h`: a `gpio*()` or `mux*()` function, a `settings.` or `present.` field, a global. Two known traps:
   - Types used in function signatures must be declared in `Bootstrap.h`. Arduino places its generated prototypes ahead of the `.ino` files, so a struct defined in an `.ino` breaks them (why `UbxMsg` lives in `Bootstrap.h`).
   - A file that uses another file's `#define`s must sort after it (why `ZED_Detect.ino` is named that way).
4. Update the source line ranges in the comment at the top of each copied block.

### A new chip or subsystem gets an update path

1. Add it to `OTA_CHIP` and `otaChipName[]` (or `OTA_SUBSYSTEM` and `otaSubsystem[]`) in `Bootstrap.h`. Names must match the manifest `chip` / `subsystem` columns.
2. Copy its update code into a new `Update_<chip>.ino`.
3. Add a `present.` flag, set it during detection, and add a row to `otaSubsystemInfoTable` in `Update.ino`, copying the firmware's row (packet size, directory).
4. If it needs pins or power control, add them to `beginBoard()`.
5. Update the tables in these notes.

### A new product

1. Add it to `ProductVariant` (same number as the firmware) and `productPropertiesTable` in `Bootstrap.h`.
2. Add its pins to `beginBoard()`: GNSS UART and reset, LoRa power/boot/reset, IMU UART, muxes, and anything that must be held for the unit to stay powered (the Facet FP's `pin_powerFastOff` must be driven low).
3. If it has swappable modules like the Facet FP, add detection to `detectSubsystems()`.
4. Update [What gets updated](#what-gets-updated).

### The GitHub certificate changes

`Certificates.h` holds the root CA for `raw.githubusercontent.com`. If the firmware's `GITHUB_RAW_PUBLIC_CERT` changes, copy it here too. A wrong certificate shows up as `TLS socket connect failed` before any download.

## Known limitations

- **Torch LoRa update output.** While the LoRa radio is being programmed, the USB serial port is shared with the STM32 bootloader, so the terminal shows some garbage and the progress lines come and go. This is the same in the firmware.
- **LoRa and IM19 are assumed fitted** on the Torch and the Facet FP (LoRa), as in the firmware. A unit built without one fails that update, so the ESP32 update is skipped. If such builds exist, add a presence check before relying on the bootstrap for them.
- **A failed update can leave a subsystem erased**, as with the firmware's OTA. Whether pressing `u` again recovers it depends on the chip. Chips entered through a hardware reset or ROM bootloader (LG290P on Torch X2 and Postcard, ZED-X20P, LoRa) can be retried. Chips reached through their running firmware cannot: the Facet FP LG290P (software reset only, and detection needs the running firmware) and the mosaic-X5 (needs its command prompt). Those need the manufacturer's recovery tools.
- **Time.** Every subsystem is always updated. Expect several minutes per unit, most of it the GNSS. The IM19 alone takes roughly 2 minutes (about 1,000 frames paced 100 ms apart).
- **Wi-Fi.** 2.4 GHz, WPA2 personal only (`WiFi.begin()`), 30 second connect timeout.

## Firmware issues found while building this

Both are fixed in the bootstrap's copy only, marked `BOOTSTRAP CHANGE`.

- `CSV.ino` `csvOpenCsvFile()` sets `*fieldCount` and `*lineCount` to zero on success as well as failure (under the comment "Cleanup upon failure"). As far as I can read it, `otaGetRequiredUpdates()` then scans zero lines, so the firmware's OTA would always report that there is nothing to update. Worth confirming on hardware.
- `CSV.ino` `csvNextLine()` skips trailing zero bytes without checking `bufferEnd`, so after the last line it can read past the end of the CSV buffer.
