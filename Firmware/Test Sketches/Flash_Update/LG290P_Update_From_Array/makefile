######################################################################
# makefile
#
# Builds the sketch
######################################################################

##########
# Source files
##########

SKETCH=LG290P_Update_From_Array
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
ARDUINO_LIBRARY_PATH=$(USER_DIRECTORY_PATH)Documents\Arduino\libraries
HOME_BOARD_PATH=$(USER_DIRECTORY_PATH)AppData\Local\Arduino15\packages\esp32
PATCH_SRC_PATH=Patch\

# Windows Sketch path
BUILD_PATH=build\esp32.esp32.esp32\
RTK_BIN_PATH=$(BUILD_PATH)$(SKETCH).ino.bin

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
TERMINAL_PORT_FACET_FP=/dev/ttyACM2
TERMINAL_PORT_LG290P=/dev/ttyACM0
TERMINAL_PORT_TORCH=/dev/ttyACM1
TERMINAL_PORT_TX2=/dev/ttyACM1
TERMINAL_PARAMS=-b 115200 -8 < /dev/tty

# Linux generic paths
USER_DIRECTORY_PATH=~/
ARDUINO_LIBRARY_PATH=$(USER_DIRECTORY_PATH)Arduino/libraries
ESP_IDF_PATH=$(HOME_BOARD_PATH)/tools/esp32-arduino-libs
HOME_BOARD_PATH=$(USER_DIRECTORY_PATH).arduino15/packages/esp32

# Linux Sketch path
BUILD_PATH=build/esp32.esp32.esp32/
RTK_BIN_PATH=$(BUILD_PATH)$(SKETCH).ino.bin

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
# Build the sketch
##########

DEBUG_LEVEL=none
#DEBUG_LEVEL=error
#DEBUG_LEVEL=warn
#DEBUG_LEVEL=info
#DEBUG_LEVEL=debug
#DEBUG_LEVEL=verbose

$(RTK_BIN_PATH):	$(SKETCH).ino   *.ino   *.h   makefile
	$(CLEAR)
	arduino-cli compile --fqbn "esp32:esp32:esp32":DebugLevel=$(DEBUG_LEVEL),PSRAM=enabled $(SKETCH).ino \
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

.PHONY: upload_facet_fp

upload_facet_fp:	$(RTK_BIN_PATH)
	python3 $(ESPTOOL_PATH)esptool.py \
        --chip   esp32 \
        --port   $(TERMINAL_PORT_FACET_FP) \
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
	$(TERMINAL_APP)   -D $(TERMINAL_PORT_FACET_FP)   $(TERMINAL_PARAMS)

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

.PHONY: upload_tx2

upload_tx2:	upload_torch

.PHONY:	terminal

terminal:
	$(TERMINAL_APP)   -D $(TERMINAL_PORT)   $(TERMINAL_PARAMS)

.PHONY:	terminal_facet_fp

terminal_facet_fp:
	$(TERMINAL_APP)   -D $(TERMINAL_PORT_FACET_FP)   $(TERMINAL_PARAMS)

.PHONY:	terminal_lg290p

terminal_lg290p:
	$(TERMINAL_APP)   -D $(TERMINAL_PORT_LG290P)   $(TERMINAL_PARAMS)

.PHONY:	terminal_torch

terminal_torch:
	$(TERMINAL_APP)   -D $(TERMINAL_PORT_TORCH)   $(TERMINAL_PARAMS)

.PHONY:	terminal_tx2

terminal_tx2:	terminal_torch

########
# Clean the build directory
##########

.PHONY: clean

clean:
	$(DELETE) build
