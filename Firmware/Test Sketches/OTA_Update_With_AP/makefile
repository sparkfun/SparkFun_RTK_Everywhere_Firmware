######################################################################
# makefile
#
# Builds the example
######################################################################

##########
# Source files
##########

EXAMPLE_SKETCH=OTA_Update_With_AP

EXECUTABLES += example

PARTITION_CSV_FILE=RTKEverywhere

##########
# Windows NT utilities
##########
ifeq ($(OS),Windows_NT)

CLEAR=cls
COPY=copy
DELETE=rmdir /s
DIR_LISTING=dir

# Terminal settings
TERMINAL_APP=
TERMINAL_PORT="COM3"
TERMINAL_PARAMS=

# Windows NT generic paths
USER_DIRECTORY_PATH=C:\Users\$(USERNAME)\
ARDUINO_LIBRARY_PATH=$(USER_DIRECTORY_PATH)\Documents\Arduino\libraries\
HOME_BOARD_PATH=$(USER_DIRECTORY_PATH)\AppData\Local\Arduino15\packages\esp32\
BUILD_PATH=build\esp32.esp32.esp32\

##########
# Linux utilities
##########
else

CLEAR=clear
COPY=cp
DELETE=rm -Rf
DIR_LISTING=ls
TERMINAL_APP=minicom

# Terminal settings
TERMINAL_PORT=/dev/ttyUSB0
TERMINAL_PORT_LG290P=/dev/ttyACM0
TERMINAL_PORT_TORCH=/dev/ttyACM1
TERMINAL_PARAMS=-b 115200 -8 < /dev/tty

# Linux generic paths
USER_DIRECTORY_PATH=~/
ARDUINO_LIBRARY_PATH=$(USER_DIRECTORY_PATH)/Arduino/libraries/
BUILD_PATH=build/esp32.esp32.esp32/
ESP_IDF_PATH=$(HOME_BOARD_PATH)/tools/esp32-arduino-libs/
HOME_BOARD_PATH=$(USER_DIRECTORY_PATH)/.arduino15/packages/esp32/
ESPTOOL_PATH=~/Arduino/hardware/espressif/esp32/tools/esptool/
BOOT_LOADER_PATH=~/SparkFun/SparkFun_RTK_Firmware_Uploader/RTK_Firmware_Uploader/resource/

endif

##########
# Buid all the sources - must be first
##########

.PHONY: all

all: $(EXECUTABLES)

##########
# Add ESP32 board support
##########

.PHONY: arduino-config

arduino-config:
	arduino-cli config init --overwrite --additional-urls "https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json,https://espressif.github.io/arduino-esp32/package_esp32_dev_index.json"

##########
# Build the example
##########

.PHONY: example

example:	$(BUILD_PATH)$(EXAMPLE_SKETCH).ino.bin

DEBUG_LEVEL=none
#DEBUG_LEVEL=debug

$(BUILD_PATH)$(EXAMPLE_SKETCH).ino.bin:	$(EXAMPLE_SKETCH).ino   *.ino    *.h   makefile
	$(CLEAR)
	arduino-cli compile --fqbn "esp32:esp32:esp32":DebugLevel=$(DEBUG_LEVEL),PSRAM=enabled $< \
		--warnings default \
		--build-property build.partitions=$(PARTITION_CSV_FILE) \
		--build-property upload.maximum_size=6291456 \
		--build-property "compiler.cpp.extra_flags=-MMD -c \"-DPOINTPERFECT_TOKEN=$(POINTPERFECT_TOKEN)\" \"-DFIRMWARE_VERSION_MAJOR=$(FIRMWARE_VERSION_MAJOR)\" \"-DFIRMWARE_VERSION_MINOR=$(FIRMWARE_VERSION_MINOR)\" \"-DENABLE_DEVELOPER=$(ENABLE_DEVELOPER)\"" \
		--export-binaries

##########
# EVK
##########

.PHONY: upload

upload:	$(BUILD_PATH)$(EXAMPLE_SKETCH).ino.bin
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

.PHONY: terminal

terminal:
	$(TERMINAL_APP)   -D $(TERMINAL_PORT)   $(TERMINAL_PARAMS)

##########
# LG290P
##########

.PHONY: upload_lg290p

upload_lg290p:	$(BUILD_PATH)$(EXAMPLE_SKETCH).ino.bin
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

.PHONY:	terminal_lg290p

terminal_lg290p:
	$(TERMINAL_APP)   -D $(TERMINAL_PORT_LG290P)   $(TERMINAL_PARAMS)

##########
# Torch
##########

.PHONY: upload_torch

upload_torch:	$(BUILD_PATH)$(EXAMPLE_SKETCH).ino.bin
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

.PHONY:	terminal_torch

terminal_torch:
	$(TERMINAL_APP)   -D $(TERMINAL_PORT_TORCH)   $(TERMINAL_PARAMS)

##########
# Clean up the example
##########

clean:
	$(DELETE) build
