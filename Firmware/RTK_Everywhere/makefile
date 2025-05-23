######################################################################
# makefile
#
# Builds the EVK firmware
######################################################################

##########
# Source files
##########

SKETCH=RTK_Everywhere.ino

# Uncomment all five lines for a complete update
#EXECUTABLES = arduino-config lib-update
EXECUTABLES += RTK

PARTITION_CSV_FILE=RTKEverywhere

ifeq ($(OS),Windows_NT)
# Windows NT utilities
CLEAR=cls
COPY=copy
DELETE=del /s
DIR_LISTING=dir
TERMINAL_APP=
TERMINAL_PORT=
TERMINAL_PARAMS=
TERMINAL_PORT_TORCH=
TERMINAL_PARAMS_TORCH=

# Windows NT generic paths
USER_DIRECTORY_PATH=C:\Users\$(USERNAME)
ARDUINO_LIBRARY_PATH=$(USER_DIRECTORY_PATH)\Documents\Arduino\libraries
HOME_BOARD_PATH=$(USER_DIRECTORY_PATH)\AppData\Local\Arduino15\packages\esp32
PATCH_SRC_PATH=Patch\

# Windows NT patch source paths
PARTITION_SRC_PATH=..\$(PARTITION_CSV_FILE).csv
PATCH_SRC_PATH=Patch\

# Windows NT patch destination paths
MBED_LIB_DEST_PATH=$(HOME_BOARD_PATH)\tools\esp32-arduino-libs\${{ env.ESP_IDF }}\esp32/lib\
NET_EVENT_PATCH_PATH=$(HOME_BOARD_PATH)\hardware\esp32\$(ESP_CORE_VERSION)\libraries\Network\src\
PARTITION_DST_PATH=$(HOME_BOARD_PATH)\hardware\esp32\$(ESP_CORE_VERSION)\tools\partitions\$(PARTITION_CSV_FILE).csv

# Windows Sketch path
BUILD_PATH=build\esp32.esp32.esp32\
RTK_BIN_PATH=$(BUILD_PATH)$(SKETCH).bin

# Windows upload support
BOOT_LOADER_PATH=$(USER_DIRECTORY_PATH)\SparkFun\SparkFun_RTK_Firmware_Uploader\RTK_Firmware_Uploader\resource\
ESPTOOL_PATH=$(USER_DIRECTORY_PATH)\Arduino\hardware\espressif\esp32\tools\esptool\

else
# Linux utilities
CLEAR=clear
COPY=cp
DELETE=rm -Rf
DIR_LISTING=ls
TERMINAL_APP=minicom
TERMINAL_PORT=/dev/ttyUSB0
TERMINAL_PARAMS=-b 115200 -8 -D $(TERMINAL_PORT) < /dev/tty
TERMINAL_PORT_TORCH=/dev/ttyACM1
TERMINAL_PARAMS_TORCH=-b 115200 -8 -D $(TERMINAL_PORT_TORCH) < /dev/tty

# Linux generic paths
USER_DIRECTORY_PATH=~
ARDUINO_LIBRARY_PATH=$(USER_DIRECTORY_PATH)/Arduino/libraries
ESP_IDF_PATH=$(HOME_BOARD_PATH)/tools/esp32-arduino-libs
HOME_BOARD_PATH=$(USER_DIRECTORY_PATH)/.arduino15/packages/esp32

# Linux patch source paths
PARTITION_SRC_PATH=../$(PARTITION_CSV_FILE).csv
PATCH_SRC_PATH=Patch/

# Linux patch destination paths
MBED_LIB_DEST_PATH=$(ESP_IDF_PATH)/$(ESP_IDF_VERSION)/esp32/lib/
NET_EVENT_PATCH_PATH=$(HOME_BOARD_PATH)/hardware/esp32/$(ESP_CORE_VERSION)/libraries/Network/src/
PARTITION_DST_PATH=$(HOME_BOARD_PATH)/hardware/esp32/$(ESP_CORE_VERSION)/tools/partitions/$(PARTITION_CSV_FILE).csv

# Linux Sketch path
BUILD_PATH=build/esp32.esp32.esp32/
RTK_BIN_PATH=$(BUILD_PATH)$(SKETCH).bin

# Linux upload support
BOOT_LOADER_PATH=~/SparkFun/SparkFun_RTK_Firmware_Uploader/RTK_Firmware_Uploader/resource/
ESPTOOL_PATH=~/Arduino/hardware/espressif/esp32/tools/esptool/

endif

##########
# Buid tools and rules
##########

##########
# Buid all the sources - must be first
##########

.PHONY: all

all: $(EXECUTABLES)

##########
# Buid RTK firmware
##########

.PHONY: arduino-config

arduino-config:
	arduino-cli config init --overwrite --additional-urls "https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json,https://espressif.github.io/arduino-esp32/package_esp32_dev_index.json"

.PHONY: core-update

core-update:
	$(DELETE) $(HOME_BOARD_PATH)
	arduino-cli core update-index
	arduino-cli core install esp32:esp32@$(ESP_CORE_VERSION)

.PHONY: lib-update

ESP_CORE_VERSION=3.0.7
ESP_IDF_VERSION=idf-release_v5.1-632e0c2a

lib-update:	core-update
	arduino-cli lib install \
		ArduinoJson@7.0.4 \
		"ArduinoMqttClient"@0.1.8 \
		ESP32_BleSerial@2.0.1 \
		"ESP32-OTA-Pull"@1.0.0 \
		ESP32Time@2.0.0 \
		JC_Button@2.1.2 \
		PubSubClient@2.8.0 \
		"SdFat"@2.1.1 \
		"SparkFun BQ40Z50 Battery Manager Arduino Library"@1.0.0 \
		"SparkFun Extensible Message Parser"@1.0.2 \
		"SparkFun I2C Expander Arduino Library"@1.0.1 \
		"SparkFun IM19 IMU Arduino Library"@1.0.1 \
		"SparkFun LG290P Quadband RTK GNSS Arduino Library"@1.0.2
		"SparkFun LIS2DH12 Arduino Library"@1.0.3 \
		"SparkFun MAX1704x Fuel Gauge Arduino Library"@1.0.4 \
		"SparkFun Qwiic OLED Arduino Library"@1.0.13 \
		"SparkFun u-blox GNSS v3"@3.1.8 \
		"SparkFun u-blox PointPerfect Library"@1.11.4 \
		"SparkFun UM980 Triband RTK GNSS Arduino Library"@1.0.4 \
		SSLClientESP32@2.0.0

$(PARTITION_DST_PATH):	$(PARTITION_SRC_PATH)
	$(COPY)  $(PARTITION_SRC_PATH)  $(PARTITION_DST_PATH)

.PHONY: partition

partition:	$(PARTITION_DST_PATH)

$(MBED_LIB_DEST_PATH)libmbedtls.a: 	$(PATCH_SRC_PATH)libmbedtls.a  $(ESP_IDF_PATH)
	$(DIR_LISTING) $(ESP_IDF_PATH)
	echo ESP_
	$(COPY)  $<  $@

$(MBED_LIB_DEST_PATH)libmbedtls_2.a: 	$(PATCH_SRC_PATH)libmbedtls_2.a
	$(COPY)  $<  $@

$(MBED_LIB_DEST_PATH)libmbedcrypto.a: 	$(PATCH_SRC_PATH)libmbedcrypto.a
	$(COPY)  $<  $@

$(MBED_LIB_DEST_PATH)libmbedx509.a: 	$(PATCH_SRC_PATH)libmbedx509.a
	$(COPY)  $<  $@

.PHONY: patch

patch:
	touch   $(PATCH_SRC_PATH)libmbedtls.a
	touch   $(PATCH_SRC_PATH)libmbedtls_2.a
	touch   $(PATCH_SRC_PATH)libmbedcrypto.a
	touch   $(PATCH_SRC_PATH)libmbedx509.a

#DEBUG_LEVEL=none
#DEBUG_LEVEL=error
#DEBUG_LEVEL=warn
#DEBUG_LEVEL=info
DEBUG_LEVEL=debug
#DEBUG_LEVEL=verbose

ENABLE_DEVELOPER=false
FIRMWARE_VERSION_MAJOR=99
FIRMWARE_VERSION_MINOR=99
POINTPERFECT_TOKEN=

form.h:	AP-Config/* AP-Config/src/* AP-Config/src/fonts/*
	python ../Tools/index_html_zipper.py AP-Config/index.html form.h
	python ../Tools/main_js_zipper.py AP-Config/src/main.js form.h

$(RTK_BIN_PATH):	RTK_Everywhere.ino   *.ino   *.h   makefile   $(PARTITION_DST_PATH)   $(MBED_LIB_DEST_PATH)libmbedtls.a   $(MBED_LIB_DEST_PATH)libmbedtls_2.a   $(MBED_LIB_DEST_PATH)libmbedcrypto.a   $(MBED_LIB_DEST_PATH)libmbedx509.a
	$(CLEAR)
	arduino-cli compile --fqbn "esp32:esp32:esp32":DebugLevel=$(DEBUG_LEVEL),PSRAM=enabled RTK_Everywhere.ino \
		--warnings default \
		--build-property build.partitions=$(PARTITION_CSV_FILE) \
		--build-property upload.maximum_size=6291456 \
		--build-property "compiler.cpp.extra_flags=-MMD -c \"-DPOINTPERFECT_TOKEN=$(POINTPERFECT_TOKEN)\" \"-DFIRMWARE_VERSION_MAJOR=$(FIRMWARE_VERSION_MAJOR)\" \"-DFIRMWARE_VERSION_MINOR=$(FIRMWARE_VERSION_MINOR)\" \"-DENABLE_DEVELOPER=$(ENABLE_DEVELOPER)\"" \
		--export-binaries

RTK:	$(RTK_BIN_PATH)

##########
# Upload the firmware
##########

.PHONY: upload

upload:	$(RTK_BIN_PATH)
	python3 $(ESPTOOL_PATH)esptool.py \
        --chip   esp32 \
        --port   $(TERMINAL_PORT) \
        --baud   921600 \
        --before   default_reset \
        --after   hard_reset \
        write_flash \
        --flash_mode dio \
        --flash_freq 80m \
        --flash_size detect \
        --compress \
         0x1000   $(BOOT_LOADER_PATH)RTK_Surveyor.ino.bootloader.bin \
         0x8000   $(BOOT_LOADER_PATH)RTK_Surveyor_Partitions_16MB.bin \
         0xe000   $(BOOT_LOADER_PATH)boot_app0.bin \
        0x10000   $<
	$(TERMINAL_APP) $(TERMINAL_PARAMS)

.PHONY: upload_torch

upload_torch:	$(RTK_BIN_PATH)
	python3 $(ESPTOOL_PATH)esptool.py \
        --chip   esp32 \
        --port   $(TERMINAL_PORT_TORCH) \
        --baud   460800 \
        --before   default_reset \
        --after   hard_reset \
        write_flash \
        --flash_mode dio \
        --flash_freq 80m \
        --flash_size detect \
        --compress \
         0x1000   $(BOOT_LOADER_PATH)RTK_Surveyor.ino.bootloader.bin \
         0x8000   $(BOOT_LOADER_PATH)RTK_Surveyor_Partitions_16MB.bin \
         0xe000   $(BOOT_LOADER_PATH)boot_app0.bin \
        0x10000   $<
	$(TERMINAL_APP) $(TERMINAL_PARAMS_TORCH)

.PHONY:	terminal

terminal:
	$(TERMINAL_APP) $(TERMINAL_PARAMS)

.PHONY:	terminal_torch

terminal_torch:
	$(TERMINAL_APP) $(TERMINAL_PARAMS_TORCH)

########
# Clean the build directory
##########

.PHONY: clean

clean:
	$(DELETE) build
