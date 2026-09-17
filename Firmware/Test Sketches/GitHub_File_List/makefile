######################################################################
# makefile
#
# Builds the EVK firmware
######################################################################

##########
# Source files
##########

SKETCH=GitHub_File_List
ESP_CORE_VERSION=3.0.7
ESP_IDF_VERSION=idf-release_v5.1-632e0c2a

# Uncomment all five lines for a complete update
#EXECUTABLES = arduino-config lib-update
EXECUTABLES += Sketch

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

# Windows Sketch path
BUILD_PATH=build\esp32.esp32.esp32\
BIN_PATH=$(BUILD_PATH)$(SKETCH).ino.bin

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

# Linux Sketch path
BUILD_PATH=build/esp32.esp32.esp32/
BIN_PATH=$(BUILD_PATH)$(SKETCH).ino.bin

# Linux upload support
BOOT_LOADER_PATH=~/SparkFun/SparkFun_RTK_Firmware_Uploader/RTK_Firmware_Uploader/resource/
ESPTOOL_PATH=~/Arduino/hardware/espressif/esp32/tools/esptool/

endif

##########
# BuiLd all the sources - must be first
##########

.PHONY: all

all: $(EXECUTABLES)

##########
# Build example
##########

DEBUG_LEVEL=none
#DEBUG_LEVEL=error
#DEBUG_LEVEL=warn
#DEBUG_LEVEL=info
#DEBUG_LEVEL=debug
#DEBUG_LEVEL=verbose

$(BIN_PATH):	$(SKETCH).ino   *.ino   *.h   makefile
	$(CLEAR)
	arduino-cli compile --fqbn "esp32:esp32:esp32":DebugLevel=$(DEBUG_LEVEL),PSRAM=enabled $(SKETCH).ino \
		--warnings default \
		--build-property build.partitions=$(PARTITION_CSV_FILE) \
		--build-property upload.maximum_size=6291456 \
		--build-property "compiler.cpp.extra_flags=-MMD -c" \
		--export-binaries

Sketch:	$(BIN_PATH)

##########
# Upload the firmware
##########

.PHONY: upload

upload:	$(BIN_PATH)
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

upload_lg290p:	$(BIN_PATH)
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

upload_torch:	$(BIN_PATH)
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
# Handle crashes
##########

.PHONY: crash

crash:
	~/SparkFun/rc/RTK/Firmware/Tools/Read_Map_File   build/e*/*.ino.map

########
# Clean the build directory
##########

.PHONY: clean

clean:
	$(DELETE) build
