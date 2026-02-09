######################################################################
# makefile
#
# Builds the EVK firmware
######################################################################

##########
# Source files
##########

SKETCH=RTK_Everywhere.ino
ESP_CORE_VERSION=3.0.7
ESP_IDF_VERSION=idf-release_v5.1-632e0c2a

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
ENABLE_EXECUTION=
TERMINAL_APP=
TERMINAL_PORT=
TERMINAL_PORT_LG209P=
TERMINAL_PORT_TORCH=
TERMINAL_PARAMS=

# Windows NT generic paths
USER_DIRECTORY_PATH=C:\Users\$(USERNAME)\
TMP_PATH=$(USER_DIRECTORY_PATH)
ARDUINO_LIBRARY_PATH=$(USER_DIRECTORY_PATH)Documents\Arduino\libraries
HOME_BOARD_PATH=$(USER_DIRECTORY_PATH)AppData\Local\Arduino15\packages\esp32
PATCH_SRC_PATH=Patch\

# Windows NT patch source paths
FIRMWARE_PATH=..\
RTK_EVERYWHERE_PATH=.\
AP_CONFIG_PATH=$(RTK_EVERYWHERE_PATH)AP-Config
PARTITION_SRC_PATH=$(FIRMWARE_PATH)$(PARTITION_CSV_FILE).csv
PATCH_SRC_PATH=$(RTK_EVERYWHERE_PATH)Patch\

# Windows NT patch destination paths
MBED_LIB_DEST_PATH=$(HOME_BOARD_PATH)\tools\esp32-arduino-libs\${{ env.ESP_IDF }}\esp32/lib\
NET_EVENT_PATCH_PATH=$(HOME_BOARD_PATH)\hardware\esp32\$(ESP_CORE_VERSION)\libraries\Network\src\
PARTITION_DST_PATH=$(HOME_BOARD_PATH)\hardware\esp32\$(ESP_CORE_VERSION)\tools\partitions\$(PARTITION_CSV_FILE).csv

# Windows Sketch path
BUILD_PATH=build\esp32.esp32.esp32\
RTK_BIN_PATH=$(BUILD_PATH)$(SKETCH).bin

# Windows upload support
BOOT_LOADER_PATH=$(USER_DIRECTORY_PATH)SparkFun\SparkFun_RTK_Firmware_Uploader\RTK_Firmware_Uploader\resource\
ESPTOOL_PATH=$(USER_DIRECTORY_PATH)Arduino\hardware\espressif\esp32\tools\esptool\

else
# Linux utilities
CLEAR=clear
COPY=cp
DELETE=rm -Rf
DIR_LISTING=ls
ENABLE_EXECUTION=chmod +x
TERMINAL_APP=minicom
TERMINAL_PORT=/dev/ttyUSB0
TERMINAL_PORT_LG290P=/dev/ttyACM0
TERMINAL_PORT_TORCH=/dev/ttyACM1
TERMINAL_PARAMS=-b 115200 -8 < /dev/tty

# Linux generic paths
USER_DIRECTORY_PATH=~/
TMP_PATH=/tmp/
ARDUINO_LIBRARY_PATH=$(USER_DIRECTORY_PATH)Arduino/libraries
ESP_IDF_PATH=$(HOME_BOARD_PATH)/tools/esp32-arduino-libs
HOME_BOARD_PATH=$(USER_DIRECTORY_PATH).arduino15/packages/esp32

# Linux patch source paths
FIRMWARE_PATH=../
RTK_EVERYWHERE_PATH=./
AP_CONFIG_PATH=$(RTK_EVERYWHERE_PATH)AP-Config
PARTITION_SRC_PATH=$(FIRMWARE_PATH)$(PARTITION_CSV_FILE).csv
PATCH_SRC_PATH=$(RTK_EVERYWHERE_PATH)Patch/

# Linux patch destination paths
BT_LIB_DEST_PATH=$(ESP_IDF_PATH)/$(ESP_IDF_VERSION)/esp32/lib/
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

.PHONY: index-update

index-update:
	fgrep "update-index" $(FIRMWARE_PATH)Dockerfile > $(TMP_PATH)arduino_core_update.sh
	sed -i 's|RUN arduino-cli|arduino-cli|' $(TMP_PATH)arduino_core_update.sh
	$(ENABLE_EXECUTION) $(TMP_PATH)arduino_core_update.sh
	$(TMP_PATH)arduino_core_update.sh
	rm $(TMP_PATH)arduino_core_update.sh

.PHONY: core-install

core-install:	index-update
	$(DELETE) $(HOME_BOARD_PATH)
	arduino-cli core install "esp32:esp32@${ESP_CORE_VERSION}"

.PHONY: lib-install

lib-install:	index-update
	fgrep "lib install" $(FIRMWARE_PATH)Dockerfile > $(TMP_PATH)arduino_library_update.sh
	sed -i 's|RUN arduino-cli|arduino-cli|' $(TMP_PATH)arduino_library_update.sh
	$(ENABLE_EXECUTION) $(TMP_PATH)arduino_library_update.sh
	$(TMP_PATH)arduino_library_update.sh
	rm $(TMP_PATH)arduino_library_update.sh

.PHONY: get-idf-version

get-idf-version:
	ESP_IDF_VERSION=$(ls ~/.arduino15/packages/esp32/tools/esp32-arduino-libs | grep idf-release)
	echo "ESP IDF Version: ${ESP_IDF_VERSION}"

$(PARTITION_DST_PATH):	$(PARTITION_SRC_PATH)
	$(COPY)  $(PARTITION_SRC_PATH)  $(PARTITION_DST_PATH)

.PHONY: partition

partition:	$(PARTITION_DST_PATH)

$(MBED_LIB_DEST_PATH)libmbedtls.a: 	$(PATCH_SRC_PATH)libmbedtls.a  $(ESP_IDF_PATH)
	$(DIR_LISTING) $(ESP_IDF_PATH)
	echo "ESP IDF Version: $(ESP_IDF_VERSION)"
	$(COPY)  $<  $@

$(MBED_LIB_DEST_PATH)libmbedtls_2.a: 	$(PATCH_SRC_PATH)libmbedtls_2.a
	$(COPY)  $<  $@

$(MBED_LIB_DEST_PATH)libmbedcrypto.a: 	$(PATCH_SRC_PATH)libmbedcrypto.a
	$(COPY)  $<  $@

$(MBED_LIB_DEST_PATH)libmbedx509.a: 	$(PATCH_SRC_PATH)libmbedx509.a
	$(COPY)  $<  $@

$(BT_LIB_DEST_PATH)libbt.a: 	$(PATCH_SRC_PATH)libbt.a
	$(COPY)  $<  $@

.PHONY: patch

patch:
	touch   $(PATCH_SRC_PATH)libmbedtls.a
	touch   $(PATCH_SRC_PATH)libmbedtls_2.a
	touch   $(PATCH_SRC_PATH)libmbedcrypto.a
	touch   $(PATCH_SRC_PATH)libmbedx509.a
	touch   $(PATCH_SRC_PATH)libbt.a
	make  $(MBED_LIB_DEST_PATH)libmbedtls.a
	make  $(MBED_LIB_DEST_PATH)libmbedtls_2.a
	make  $(MBED_LIB_DEST_PATH)libmbedcrypto.a
	make  $(MBED_LIB_DEST_PATH)libmbedx509.a
	make  $(BT_LIB_DEST_PATH)libbt.a

DEBUG_LEVEL=none
#DEBUG_LEVEL=error
#DEBUG_LEVEL=warn
#DEBUG_LEVEL=info
#DEBUG_LEVEL=debug
#DEBUG_LEVEL=verbose

ENABLE_DEVELOPER=false
FIRMWARE_VERSION_MAJOR=99
FIRMWARE_VERSION_MINOR=99
POINTPERFECT_TOKEN=

form.h:	$(AP_CONFIG_PATH)/* $(AP_CONFIG_PATH)/src/* $(AP_CONFIG_PATH)/src/fonts/*
	python ../Tools/index_html_zipper.py $(AP_CONFIG_PATH)/index.html form.h
	python ../Tools/main_js_zipper.py $(AP_CONFIG_PATH)/src/main.js form.h

$(RTK_BIN_PATH):	$(SKETCH)   *.ino   *.h   makefile   $(PARTITION_DST_PATH)   $(MBED_LIB_DEST_PATH)libmbedtls.a   $(MBED_LIB_DEST_PATH)libmbedtls_2.a   $(MBED_LIB_DEST_PATH)libmbedcrypto.a   $(MBED_LIB_DEST_PATH)libmbedx509.a   $(BT_LIB_DEST_PATH)libbt.a
	$(CLEAR)
	arduino-cli compile --fqbn "esp32:esp32:esp32":DebugLevel=$(DEBUG_LEVEL),PSRAM=enabled $(SKETCH) \
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
	$(TERMINAL_APP)   -D $(TERMINAL_PORT)   $(TERMINAL_PARAMS)

.PHONY: upload_lg290p

upload_lg290p:	$(RTK_BIN_PATH)
	python3 $(ESPTOOL_PATH)esptool.py \
        --chip   esp32 \
        --port   $(TERMINAL_PORT_LG290P) \
        --baud   460800 \
        --before   default_reset \
        --after   hard_reset \
        write_flash \
        --flash_mode dio \
        --flash_freq 80m \
        --flash_size detect \
        --compress \
         0x1000   $(BOOT_LOADER_PATH)RTK_Surveyor.ino.bootloader.bin \
         0x8000   $(BOOT_LOADER_PATH)RTK_Everywhere_Partitions_8MB.bin \
         0xe000   $(BOOT_LOADER_PATH)boot_app0.bin \
        0x10000   $<
	$(TERMINAL_APP)   -D $(TERMINAL_PORT_LG290P)   $(TERMINAL_PARAMS)

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
	$(TERMINAL_APP)   -D $(TERMINAL_PORT_TORCH)   $(TERMINAL_PARAMS)

.PHONY:	terminal

terminal:
	$(TERMINAL_APP)   -D $(TERMINAL_PORT)   $(TERMINAL_PARAMS)

.PHONY:	terminal_lg290p

terminal_lg290p:
	$(TERMINAL_APP)   -D $(TERMINAL_PORT_LG290P)   $(TERMINAL_PARAMS)

.PHONY:	terminal_torch

terminal_torch:
	$(TERMINAL_APP)   -D $(TERMINAL_PORT_TORCH)   $(TERMINAL_PARAMS)

########
# Clean the build directory
##########

.PHONY: clean

clean:
	$(DELETE) build
