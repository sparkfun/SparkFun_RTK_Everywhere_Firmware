FROM ubuntu:latest AS upstream

ARG DEBIAN_FRONTEND=noninteractive

ARG CORE_VERSION=3.0.7

ARG FIRMWARE_VERSION_MAJOR=99
ARG FIRMWARE_VERSION_MINOR=99

# ESP32 Core Debug Level
# We use "none" for releases and "error" for release_candidate
# You may find "verbose" useful while you are debugging your changes
ARG DEBUG_LEVEL=error

# Developer Mode
# You may find "true" useful while you are making changes
# Set to false for releases
ARG ENABLE_DEVELOPER=true

# arduino-cli warnings: none default more all
ARG WARNINGS=default

# If you have your own u-blox PointPerfect token(s), define them here
# or pass in as args when building the Dockerfile
# POINTPERFECT_LBAND_TOKEN is the token for the deprecated u-blox L-band SPARTN service
# POINTPERFECT_IP_TOKEN is the token for the deprecated u-blox IP SPARTN service
# POINTPERFECT_LBAND_IP_TOKEN is the token for the deprecated u-blox combined L-band and IP SPARTN service
# POINTPERFECT_RTCM_TOKEN is the token for the u-blox RTCM over NTRIP service
ARG POINTPERFECT_LBAND_TOKEN=0xAA,0xBB,0xCC,0xDD,0x00,0x11,0x22,0x33,0x0A,0x0B,0x0C,0x0D,0x00,0x01,0x02,0x03
ARG POINTPERFECT_IP_TOKEN=0xAA,0xBB,0xCC,0xDD,0x00,0x11,0x22,0x33,0x0A,0x0B,0x0C,0x0D,0x00,0x01,0x02,0x03
ARG POINTPERFECT_LBAND_IP_TOKEN=0xAA,0xBB,0xCC,0xDD,0x00,0x11,0x22,0x33,0x0A,0x0B,0x0C,0x0D,0x00,0x01,0x02,0x03
ARG POINTPERFECT_RTCM_TOKEN=0xAA,0xBB,0xCC,0xDD,0x00,0x11,0x22,0x33,0x0A,0x0B,0x0C,0x0D,0x00,0x01,0x02,0x03

# Get curl and python3
RUN apt-get update \
    && apt-get install -y curl python3 python3-pip python3-venv \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Avoid the externally managed environment constraint
RUN PYTHON_VER=$(ls /usr/lib | grep python3.) \
    && echo "Python version: ${PYTHON_VER}" \
    && rm /usr/lib/${PYTHON_VER}/EXTERNALLY-MANAGED

# Install Python dependencies - esptool needs pyserial
#RUN python3 -m pip install --upgrade pip && \
RUN pip install pyserial

# Setup Arduino CLI
#RUN curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=/usr/local/bin sh
RUN curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
#RUN curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh -s nightly-latest

# Start config file
RUN arduino-cli config init --additional-urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json,https://espressif.github.io/arduino-esp32/package_esp32_dev_index.json

# Update core index
RUN arduino-cli core update-index

# Update library index
RUN arduino-cli lib update-index

# Install platform
RUN arduino-cli core install "esp32:esp32@${CORE_VERSION}"

# Get Known Libraries
RUN arduino-cli lib install ArduinoJson@7.4.2
RUN arduino-cli lib install ESP32Time@2.0.0
RUN arduino-cli lib install ESP32_BleSerial@3.0.0
RUN arduino-cli lib install "ESP32-OTA-Pull"@1.0.0
RUN arduino-cli lib install JC_Button@2.1.2
RUN arduino-cli lib install PubSubClient@2.8.0
RUN arduino-cli lib install "SdFat"@2.1.1
RUN arduino-cli lib install "SparkFun LIS2DH12 Arduino Library"@1.0.3
RUN arduino-cli lib install "SparkFun MAX1704x Fuel Gauge Arduino Library"@1.0.4
RUN arduino-cli lib install "SparkFun u-blox GNSS v3"@3.1.10
RUN arduino-cli lib install "SparkFun Qwiic OLED Arduino Library"@1.0.15
RUN arduino-cli lib install SSLClientESP32@2.0.0
RUN arduino-cli lib install "SparkFun Extensible Message Parser"@1.0.8
RUN arduino-cli lib install "SparkFun BQ40Z50 Battery Manager Arduino Library"@1.0.0
RUN arduino-cli lib install "ArduinoMqttClient"@0.1.8
RUN arduino-cli lib install "SparkFun u-blox PointPerfect Library"@1.11.4
RUN arduino-cli lib install "SparkFun IM19 IMU Arduino Library"@1.0.1
RUN arduino-cli lib install "SparkFun UM980 Triband RTK GNSS Arduino Library"@1.0.10
RUN arduino-cli lib install "SparkFun LG290P Quadband RTK GNSS Arduino Library"@1.0.8
RUN arduino-cli lib install "SparkFun I2C Expander Arduino Library"@1.0.1
RUN arduino-cli lib install "SparkFun Apple Accessory Arduino Library"@3.1.0
RUN arduino-cli lib install "SparkFun Authentication Coprocessor Arduino Library"@1.0.0
RUN arduino-cli lib install "SparkFun Toolkit"@1.0.6

# Copy RTK_Everywhere and build deployment image
FROM upstream AS deployment

# Add the source files
ADD . .

# Patch libmbedtls and libbt
RUN cd ./RTK_Everywhere/Patch \
    && ESP_IDF=$(ls /root/.arduino15/packages/esp32/tools/esp32-arduino-libs | grep idf-release) \
    && echo "ESP IDF Version: ${ESP_IDF}" \
    && cp libmbedtls.a "/root/.arduino15/packages/esp32/tools/esp32-arduino-libs/${ESP_IDF}/esp32/lib/libmbedtls.a" \
    && cp libmbedtls_2.a "/root/.arduino15/packages/esp32/tools/esp32-arduino-libs/${ESP_IDF}/esp32/lib/libmbedtls_2.a" \
    && cp libmbedcrypto.a "/root/.arduino15/packages/esp32/tools/esp32-arduino-libs/${ESP_IDF}/esp32/lib/libmbedcrypto.a" \
    && cp libmbedx509.a "/root/.arduino15/packages/esp32/tools/esp32-arduino-libs/${ESP_IDF}/esp32/lib/libmbedx509.a" \
    && cp libbt.a "/root/.arduino15/packages/esp32/tools/esp32-arduino-libs/${ESP_IDF}/esp32/lib/libbt.a"

# Update form.h with index_html and main_js
RUN cd ./Tools \
    && python3 index_html_zipper.py ../RTK_Everywhere/AP-Config/index.html ../RTK_Everywhere/form.h \
    && python3 main_js_zipper.py ../RTK_Everywhere/AP-Config/src/main.js ../RTK_Everywhere/form.h

# Copy custom RTKEverywhere.csv
# Use the 16MB partitions by default. 8MB (Postcard) partitions must be compiled separately
RUN cp RTKEverywhere.csv "/root/.arduino15/packages/esp32/hardware/esp32/${CORE_VERSION}/tools/partitions/RTKEverywhere.csv"

# Compile Sketch
RUN cd ./RTK_Everywhere \
    && arduino-cli compile --fqbn "esp32:esp32:esp32":DebugLevel=${DEBUG_LEVEL},PSRAM=enabled \
    --warnings ${WARNINGS} \
    ./RTK_Everywhere.ino \
    --build-property build.partitions=RTKEverywhere \
    --build-property upload.maximum_size=4055040 \
    --build-property "compiler.cpp.extra_flags=-MMD -c \
    \"-DPOINTPERFECT_LBAND_TOKEN=${POINTPERFECT_LBAND_TOKEN}\" \
    \"-DPOINTPERFECT_IP_TOKEN=${POINTPERFECT_IP_TOKEN}\" \
    \"-DPOINTPERFECT_LBAND_IP_TOKEN=${POINTPERFECT_LBAND_IP_TOKEN}\" \
    \"-DPOINTPERFECT_RTCM_TOKEN=${POINTPERFECT_RTCM_TOKEN}\"  \
    \"-DFIRMWARE_VERSION_MAJOR=${FIRMWARE_VERSION_MAJOR}\" \
    \"-DFIRMWARE_VERSION_MINOR=${FIRMWARE_VERSION_MINOR}\" \
    \"-DENABLE_DEVELOPER=${ENABLE_DEVELOPER}\"" \
    --export-binaries

# Copy the compile output. List the files
FROM deployment AS output
COPY --from=deployment ./RTK_Everywhere/build/esp32.esp32.esp32 /
CMD echo $(ls /*.*)
