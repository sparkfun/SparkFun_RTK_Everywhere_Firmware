######################################################################
# makefile
#
# Builds the EVK firmware
######################################################################

##########
# Source files
##########

EXECUTABLES += RTK

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
	arduino-cli config init --additional-urls "https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json"

.PHONY: lib-update

ESP_CORE_VERSION=3.0.0

lib-update:
	arduino-cli core update-index
	arduino-cli core install esp32:esp32@$(ESP_CORE_VERSION)
	arduino-cli lib install \
          ArduinoJson@6.19.4 \
          "ArduinoMqttClient"@0.1.8 \
          ESP32Time@2.0.0 \
          ESP32_BleSerial@1.0.4 \
          "ESP32-OTA-Pull"@1.0.0 \
          Ethernet@2.0.2 \
          JC_Button@2.1.2 \
          PubSubClient@2.8.0 \
          "SdFat"@2.1.1 \
          "SparkFun BQ40Z50 Battery Manager Arduino Library"@1.0.0 \
          "SparkFun Extensible Message Parser"@1.0.0 \
          "SparkFun IM19 IMU Arduino Library"@1.0.1 \
          "SparkFun LIS2DH12 Arduino Library"@1.0.3 \
          "SparkFun MAX1704x Fuel Gauge Arduino Library"@1.0.4 \
          "SparkFun Qwiic OLED Arduino Library"@1.0.13 \
          "SparkFun u-blox GNSS v3"@3.1.5 \
          "SparkFun u-blox PointPerfect Library"@1.11.4 \
          "SparkFun UM980 Triband RTK GNSS Arduino Library"@1.0.4 \
          SparkFun_WebServer_ESP32_W5500@1.5.5 \
          SSLClientESP32@2.0.0

	arduino-cli config set library.enable_unsafe_install true
	arduino-cli lib install --git-url \
          https://github.com/me-no-dev/ESPAsyncWebServer.git \
          https://github.com/me-no-dev/AsyncTCP.git \

	cp ../RTKEverywhere.csv ~/.arduino15/packages/esp32/hardware/esp32/$(ESP_CORE_VERSION)/tools/partitions/RTKEverywhere.csv

DEBUG_LEVEL=debug
ENABLE_DEVELOPER=false
FIRMWARE_VERSION_MAJOR=99
FIRMWARE_VERSION_MINOR=99
POINTPERFECT_TOKEN=

form.h:	* AP-Config/* AP-Config/src/* AP-Config/src/fonts/*
	python ../Tools/index_html_zipper.py AP-Config/index.html form.h
	python ../Tools/main_js_zipper.py AP-Config/src/main.js form.h

RTK:	RTK_Everywhere.ino *ino *.h
	arduino-cli compile --fqbn "esp32:esp32:esp32":DebugLevel=$(DEBUG_LEVEL) RTK_Everywhere.ino \
          --build-property build.partitions=RTKEverywhere \
          --build-property upload.maximum_size=3145728 \
          --build-property "compiler.cpp.extra_flags=-MMD -c \"-DPOINTPERFECT_TOKEN=$(POINTPERFECT_TOKEN)\" \"-DFIRMWARE_VERSION_MAJOR=$(FIRMWARE_VERSION_MAJOR)\" \"-DFIRMWARE_VERSION_MINOR=$(FIRMWARE_VERSION_MINOR)\" \"-DENABLE_DEVELOPER=$(ENABLE_DEVELOPER)\"" \
          --export-binaries
#	arduino-cli compile --fqbn "esp32:esp32:esp32":DebugLevel=$(DEBUG_LEVEL) RTK_Everywhere.ino  --build-property build.partitions=app3M_fat9M_16MB  --build-property upload.maximum_size=3145728 --build-property "compiler.cpp.extra_flags=\"-DPOINTPERFECT_TOKEN=$(POINTPERFECT_TOKEN)\" \"-DFIRMWARE_VERSION_MAJOR=$(FIRMWARE_VERSION_MAJOR)\" \"-DFIRMWARE_VERSION_MINOR=$(FIRMWARE_VERSION_MINOR)\" \"-DENABLE_DEVELOPER=$(ENABLE_DEVELOPER)\"" --export-binaries

########
# Clean the build directory
##########

