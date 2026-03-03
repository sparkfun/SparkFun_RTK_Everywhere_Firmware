######################################################################
# makefile
#
# Builds the example
######################################################################

.ONESHELL:
SHELL=/bin/bash

##########
# Source files
##########

SKETCH=Save_Files

DEPENDS_ON_FILES += makefile
DEPENDS_ON_FILES += *.ino

PARTITION_CSV_FILE=RTKEverywhere
BOOT_LOADER_BIN=RTK_Surveyor.ino.bootloader.bin
PARTITION_BIN=RTK_Surveyor_Partitions_16MB.bin
BOOT_APP0_BIN=boot_app0.bin

ifeq ($(OS),Windows_NT)
# Windows NT utilities
CLEAR=cls
DELETE=del /s
TERMINAL_APP=
TERMINAL_PORT=
TERMINAL_PARAMS=

# Windows NT generic paths
USER_DIRECTORY_PATH=C:\Users\$(USERNAME)\

# Windows build support
BUILD_PATH=build\esp32.esp32.esp32\
ARDUINO_LIBRARY_PATH=$(USER_DIRECTORY_PATH)Documents\Arduino\libraries
HOME_BOARD_PATH=$(USER_DIRECTORY_PATH)AppData\Local\Arduino15\packages\esp32

# Windows upload support
ESPTOOL_PATH=$(USER_DIRECTORY_PATH)Arduino\hardware\espressif\esp32\tools\esptool\
BOOT_LOADER_PATH=$(USER_DIRECTORY_PATH)SparkFun\SparkFun_RTK_Firmware_Uploader\RTK_Firmware_Uploader\resource\

else
# Linux utilities
CLEAR=clear
DELETE=rm -Rf
TERMINAL_APP=minicom
TERMINAL_PARAMS=-b 115200 -8 < /dev/tty

# Linux generic paths
USER_DIRECTORY_PATH=~/
ARDUINO_LIBRARY_PATH=$(USER_DIRECTORY_PATH)Arduino/libraries
HOME_BOARD_PATH=$(USER_DIRECTORY_PATH).arduino15/packages/esp32

# Linux build  support
BUILD_PATH=build/esp32.esp32.esp32/

# Linux upload support
ESPTOOL_PATH=~/Arduino/hardware/espressif/esp32/tools/esptool/
BOOT_LOADER_PATH=~/SparkFun/SparkFun_RTK_Firmware_Uploader/RTK_Firmware_Uploader/resource/

endif

##########
# Buid all the sources - must be first
##########

EXAMPLE += $(BUILD_PATH)$(SKETCH).ino.bin

.PHONY: all

all: $(EXAMPLE)

##########
# Terminal
##########

.PHONY:	terminal

terminal:
	if   [   -a   "/dev/ttyACM0"   ];   then
	    export TERMINAL_PORT=/dev/ttyACM0
	else
	    if   [   -a   "/dev/ttyACM1"   ];   then
	        export TERMINAL_PORT=/dev/ttyACM1
	    else
	        if   [   -a   "/dev/ttyACM2"   ];   then
	            export TERMINAL_PORT=/dev/ttyACM2
	        else
	            if   [   -a   "/dev/ttyACM3"   ];   then
	                export TERMINAL_PORT=/dev/ttyACM3
	            else
	                if   [   -a   "/dev/ttyUSB0"   ];   then
	                    export TERMINAL_PORT=/dev/ttyUSB0
	                else
	                    if   [   -a   "/dev/ttyUSB1"   ];   then
	                        export TERMINAL_PORT=/dev/ttyUSB1
	                    else
	                        if   [   -a   "/dev/ttyUSB2"   ];   then
	                            export TERMINAL_PORT=/dev/ttyUSB2
	                        else
	                            if   [   -a   "/dev/ttyUSB3"   ];   then
	                                export TERMINAL_PORT=/dev/ttyUSB3
	                            else
	                                ls /dev/ttyACM*
	                                ls /dev/ttyUSB*
	                                ps -ax | grep bossac
	                            fi
	                        fi
	                    fi
	                fi
	            fi
	        fi
	    fi
	fi
	echo $$TERMINAL_PORT
	$(TERMINAL_APP)   -D $$TERMINAL_PORT   $(TERMINAL_PARAMS)

##########
# Upload the firmware
##########

.PHONY: upload_common

upload_common:	$(BUILD_PATH)$(SKETCH).ino.bin
	if   [   -a   "/dev/ttyACM0"   ];   then
	    export TERMINAL_PORT=/dev/ttyACM0
	else
	    if   [   -a   "/dev/ttyACM1"   ];   then
	        export TERMINAL_PORT=/dev/ttyACM1
	    else
	        if   [   -a   "/dev/ttyACM2"   ];   then
	            export TERMINAL_PORT=/dev/ttyACM2
	        else
	            if   [   -a   "/dev/ttyACM3"   ];   then
	                export TERMINAL_PORT=/dev/ttyACM3
	            else
	                if   [   -a   "/dev/ttyUSB0"   ];   then
	                    export TERMINAL_PORT=/dev/ttyUSB0
	                else
	                    if   [   -a   "/dev/ttyUSB1"   ];   then
	                        export TERMINAL_PORT=/dev/ttyUSB1
	                    else
	                        if   [   -a   "/dev/ttyUSB2"   ];   then
	                            export TERMINAL_PORT=/dev/ttyUSB2
	                        else
	                            if   [   -a   "/dev/ttyUSB3"   ];   then
	                                export TERMINAL_PORT=/dev/ttyUSB3
	                            else
	                                ls /dev/ttyACM*
	                                ls /dev/ttyUSB*
	                                ps -ax | grep bossac
	                            fi
	                        fi
	                    fi
	                fi
	            fi
	        fi
	    fi
	fi
	echo $$TERMINAL_PORT
	python3 $(ESPTOOL_PATH)esptool.py \
        --chip   esp32 \
        --port   $$TERMINAL_PORT \
        --baud   460800 \
        --before   default_reset \
        --after   hard_reset \
        write_flash \
        --flash_mode dio \
        --flash_freq 80m \
        --flash_size detect \
        --compress \
         0x1000   $(BOOT_LOADER_PATH)$(BOOT_LOADER_BIN) \
         0x8000   $(BOOT_LOADER_PATH)$(PARTITION_BIN) \
         0xe000   $(BOOT_LOADER_PATH)$(BOOT_APP0_BIN) \
        0x10000   $<

##########
# Buid ??? firmware
##########

DEBUG_LEVEL=none
#DEBUG_LEVEL=error
#DEBUG_LEVEL=warn
#DEBUG_LEVEL=info
#DEBUG_LEVEL=debug
#DEBUG_LEVEL=verbose

$(BUILD_PATH)$(SKETCH).ino.bin:	$(SKETCH).ino   $(DEPENDS_ON_FILES)
	$(CLEAR)
	echo $(SKETCH)
	arduino-cli \
		compile \
		--fqbn   "esp32:esp32:esp32":DebugLevel=$(DEBUG_LEVEL)   $(SKETCH).ino \
		--warnings   default \
		--build-property   build.partitions=$(PARTITION_CSV_FILE) \
		--build-property   upload.maximum_size=6291456 \
		--build-property   "compiler.cpp.extra_flags=-MMD   -c" \
		--export-binaries

.PHONY: upload

upload:	$(BUILD_PATH)$(SKETCH).ino.bin
	make   upload_common

##########
# Buid POSTCARD firmware
##########

.PHONY: postcard

postcard:	$(SKETCH).ino   $(DEPENDS_ON_FILES)
	$(CLEAR)
	echo $(SKETCH) RTK Postcard
	arduino-cli \
		compile \
		--fqbn   "esp32:esp32:esp32":DebugLevel=$(DEBUG_LEVEL)   $(SKETCH).ino \
		--warnings   default \
		--build-property   build.partitions=$(PARTITION_CSV_FILE) \
		--build-property   upload.maximum_size=6291456 \
		--build-property   "compiler.cpp.extra_flags=-MMD   -c   -DPOSTCARD" \
		--export-binaries

.PHONY: postcard_upload

postcard_upload:	postcard
	make   upload_common

##########
# Buid RPI_FLEX firmware
##########

.PHONY: rpi_flex

rpi_flex:	$(SKETCH).ino   $(DEPENDS_ON_FILES)
	$(CLEAR)
	echo $(SKETCH) - RPI Flex
	arduino-cli \
		compile \
		--fqbn   "esp32:esp32:esp32":DebugLevel=$(DEBUG_LEVEL)   $(SKETCH).ino \
		--warnings   default \
		--build-property   build.partitions=$(PARTITION_CSV_FILE) \
		--build-property   upload.maximum_size=6291456 \
		--build-property   "compiler.cpp.extra_flags=-MMD   -c   -DESP32_RPI_FLEX" \
		--export-binaries

.PHONY: rpi_flex_upload

rpi_flex_upload:	rpi_flex
	make   upload_common

########
# Clean the build directory
##########

.PHONY: clean

clean:
	$(DELETE) build
