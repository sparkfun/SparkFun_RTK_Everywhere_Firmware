######################################################################
# makefile
#
# Builds the RTK support programs
######################################################################

.ONESHELL:
SHELL=/bin/bash

##########
# Source files
##########

EXECUTABLES  = Compare
EXECUTABLES += NMEA_Client
EXECUTABLES += Read_Map_File
EXECUTABLES += RTK_Reset
EXECUTABLES += Split_Messages
EXECUTABLES += X.509_crt_bundle_bin_to_c

INCLUDES  = crc24q.h

##########
# Buid tools and rules
##########

GCC = gcc
CFLAGS = -flto -O3 -Wpedantic -pedantic-errors -Wall -Wextra -Werror -Wno-unused-variable -Wno-unused-parameter
CC = $(GCC) $(CFLAGS)

%.o: %.c $(INCLUDES)
	$(CC) -c -o $@ $<

%: %.c $(INCLUDES)
	$(CC) $(CFLAGS) -o $@ $<

##########
# Buid all the sources - must be first
##########

.PHONY: all

all: $(EXECUTABLES)

##########
# Buid RTK firmware
##########

DEBUG_LEVEL=debug
ENABLE_DEVELOPER=false
FIRMWARE_VERSION_MAJOR=99
FIRMWARE_VERSION_MINOR=99
POINTPERFECT_LBAND_TOKEN=
PORT_NAME=/dev/ttyUSB0
TEST_SKETCH=PSRAM_HeapCapsTest

libraries:
	echo ----------------------------------
	cd  ~/Arduino/libraries

	echo ArduinoJson
	cd   ArduinoJson
	git  checkout  --quiet  7.x
	git  pull  --quiet  origin  7.x
	git  checkout  --quiet  6.x
	git  pull  --quiet  origin  6.x
	git  checkout  --quiet  v6.19.4

	echo ArduinoMqttClient
	cd   ../ArduinoMqttClient
	git  checkout  --quiet  master
	git  pull  --quiet  origin  master
	git  checkout  --quiet  0.1.7

	echo ArduinoWebsockets
	cd   ../ArduinoWebsockets
	git  checkout  --quiet  master
	git  pull  --quiet  origin  master
	git  checkout  --quiet  0.5.3

	echo AsyncTCP
	cd   ../AsyncTCP
	git  checkout  --quiet  master
	git  pull  --quiet  origin  master

	echo ESP32_BleSerial
	cd   ../ESP32_BleSerial
	git  checkout  --quiet  master
	git  pull  --quiet  origin  master
	git  checkout  --quiet  v1.0.4

	echo ESP32-OTA-Pull
	cd   ../ESP32-OTA-Pull
	git  checkout  --quiet  main
	git  pull  --quiet  origin  main
	git  checkout  --quiet  v1.0.0-beta.1

	echo ESP32Time
	cd   ../ESP32Time
	git  checkout  --quiet  main
	git  pull  --quiet  origin  main
	git  checkout  --quiet  v2.0.0

	echo ESPAsyncWebServer
	cd   ../ESPAsyncWebServer
	git  checkout  --quiet  master
	git  pull  --quiet  origin  master

	echo Ethernet
	cd   ../Ethernet
	git  checkout  --quiet  master
	git  pull  --quiet  origin  master
	git  checkout  --quiet  2.0.2

	echo JC_Button
	cd   ../JC_Button
	git  checkout  --quiet  master
	git  pull  --quiet  origin  master
	git  checkout  --quiet  2.1.2

	echo NimBLE-Arduino
	cd   ../NimBLE-Arduino
	git  checkout  --quiet  release/1.4
	git  pull  --quiet  origin  release/1.4
	git  checkout  --quiet  1.4.1

	echo PubSubClient
	cd   ../PubSubClient
	git  checkout  --quiet  master
	git  pull  --quiet  origin  master
	git  checkout  --quiet  v2.8

	echo RadioLib
	cd   ../RadioLib
	git  checkout  --quiet  master
	git  pull  --quiet  origin  master
	git  checkout  --quiet  5.6.0

	echo SdFat
	cd   ../SdFat
	git  checkout  --quiet  master
	git  pull  --quiet  origin  master
	git  checkout  --quiet  2.1.1

	echo SparkFun_LIS2DH12_Arduino_Library
	cd   ../SparkFun_LIS2DH12_Arduino_Library
	git  checkout  --quiet  master
	git  pull  --quiet  origin  master
	git  checkout  --quiet  v1.0.3

	echo SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library
	cd   ../SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library
	git  checkout  --quiet  main
	git  pull  --quiet  origin  main
	git  checkout  --quiet  v1.0.4

	echo SparkFun_Qwiic_OLED_Arduino_Library
	cd   ../SparkFun_Qwiic_OLED_Arduino_Library
	git  checkout  --quiet  main
	git  pull  --quiet  origin  main
	git  checkout  --quiet  v1.0.9

	echo SparkFun_u-blox_GNSS_Arduino_Library
	cd   ../SparkFun_u-blox_GNSS_Arduino_Library
	git  checkout  --quiet  main
	git  pull  --quiet  origin  main
	git  checkout  --quiet  v2.2.24

	echo SparkFun_u-blox_GNSS_v3
	cd   ../SparkFun_u-blox_GNSS_v3
	git  checkout  --quiet  main
	git  pull  --quiet  origin  main
	git  checkout  --quiet  v3.0.14

	echo SparkFun_u-blox_SARA-R5_Arduino_Library
	cd   ../SparkFun_u-blox_SARA-R5_Arduino_Library
	git  checkout  --quiet  main
	git  pull  --quiet  origin  main
	git  checkout  --quiet  v1.1.6

	echo SparkFun_WebServer_ESP32_W5500
	cd   ../SparkFun_WebServer_ESP32_W5500
	git  checkout  --quiet  main
	git  pull  --quiet  origin  main
	git  checkout  --quiet  v1.5.5

	echo SSLClientESP32
	cd   ../SSLClientESP32
	git  checkout  --quiet  master
	git  pull  --quiet  origin  master
	git  checkout  --quiet  v2.0.0

	echo WiFiManager
	cd   ../WiFiManager
	git  checkout  --quiet  master
	git  pull  --quiet  origin  master
	git  checkout  --quiet  v2.0.16-rc.2

	echo ----------------------------------
	~/Arduino/arduino-cli lib list

partition:	../RTKEverywhere.csv
	ESP_PATH=/home/$$USER/.arduino15/packages/esp32/hardware/esp32/*
	for file in $$ESP_PATH; do \
		ESP_VERSION_PATH=$${file}; \
	done
	ESP_VERSION=$$(basename $$ESP_VERSION_PATH)
	cp $< ~/.arduino15/packages/esp32/hardware/esp32/$$ESP_VERSION/tools/partitions/app3M_fat9M_16MB.csv
	cp $< ~/.arduino15/packages/esp32/hardware/esp32/$$ESP_VERSION/tools/partitions/

../RTK_Everywhere/form.h:	makefile  ../RTK_Everywhere/*  ../RTK_Everywhere/AP-Config/* ../RTK_Everywhere/AP-Config/src/* ../RTK_Everywhere/AP-Config/src/fonts/*
	python index_html_zipper.py ../RTK_Everywhere/AP-Config/index.html ../RTK_Everywhere/form.h
	python main_js_zipper.py ../RTK_Everywhere/AP-Config/src/main.js ../RTK_Everywhere/form.h

../RTK_Everywhere/build/esp32.esp32.esp32/RTK_Everywhere.ino.bin:	../RTK_Everywhere/*.ino  ../RTK_Everywhere/*.h
	~/Arduino/arduino-cli  compile  --fqbn  "esp32:esp32:esp32":DebugLevel=$(DEBUG_LEVEL)  ../RTK_Everywhere/RTK_Everywhere.ino  --build-property build.partitions=app3M_fat9M_16MB  --build-property upload.maximum_size=3145728  --build-property "compiler.cpp.extra_flags=\"-DPOINTPERFECT_LBAND_TOKEN=$(POINTPERFECT_LBAND_TOKEN)\" \"-DFIRMWARE_VERSION_MAJOR=$(FIRMWARE_VERSION_MAJOR)\" \"-DFIRMWARE_VERSION_MINOR=$(FIRMWARE_VERSION_MINOR)\" \"-DENABLE_DEVELOPER=$(ENABLE_DEVELOPER)\""  --export-binaries

RTK:	../RTK_Everywhere/build/esp32.esp32.esp32/RTK_Everywhere.ino.elf
	ls -al ../RTK_Everywhere/build/esp32.esp32.esp32/RTK_Everywhere.ino.bin

upload:	../RTK_Everywhere/build/esp32.esp32.esp32/RTK_Everywhere.ino.elf
	~/Arduino/arduino-cli  upload  --log  --log-level debug  --port $(PORT_NAME)  --fqbn esp32:esp32:esp32  ../RTK_Everywhere

../Test\ Sketches/$(TEST_SKETCH)/build/esp32.esp32.esp32/$(TEST_SKETCH).ino.elf:	makefile  ../Test\ Sketches/$(TEST_SKETCH)/*.ino
	~/Arduino/arduino-cli  compile  --fqbn  "esp32:esp32:esp32":DebugLevel=$(DEBUG_LEVEL)  ../Test\ Sketches/$(TEST_SKETCH)/$(TEST_SKETCH).ino  --build-property build.partitions=app3M_fat9M_16MB  --build-property upload.maximum_size=3145728  --build-property "compiler.cpp.extra_flags=\"-DBOARD_HAS_PSRAM\""  --export-binaries

upload-test:	../Test\ Sketches/$(TEST_SKETCH)/build/esp32.esp32.esp32/$(TEST_SKETCH).ino.elf
	~/Arduino/arduino-cli  upload  --log  --log-level debug  --port $(PORT_NAME)  --fqbn esp32:esp32:esp32  ../Test\ Sketches/$(TEST_SKETCH)

########
# Clean the build directory
##########

.PHONY: clean

clean:
	rm -f *.o *.a $(EXECUTABLES)
