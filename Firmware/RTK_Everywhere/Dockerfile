FROM ubuntu:latest

ARG DEBIAN_FRONTEND=noninteractive

ENV FILENAME_PREFIX=RTK_Everywhere_Firmware
ENV FIRMWARE_VERSION_MAJOR=99
ENV FIRMWARE_VERSION_MINOR=99
ENV CORE_VERSION=3.0.7

# Get curl
RUN apt-get update && apt-get install -y curl && apt-get clean

# Setup Arduino CLI
#RUN curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=/usr/local/bin sh
RUN curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh

# Start config file
RUN arduino-cli config init --additional-urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json,https://espressif.github.io/arduino-esp32/package_esp32_dev_index.json

# Update core index
RUN arduino-cli core update-index

# Update library index
RUN arduino-cli lib update-index

# Install platform
RUN arduino-cli core install "esp32:esp32@${CORE_VERSION}"

# Get Known Libraries
RUN arduino-cli lib install ArduinoJson@7.0.4
RUN arduino-cli lib install ESP32Time@2.0.0
RUN arduino-cli lib install ESP32_BleSerial@2.0.1
RUN arduino-cli lib install "ESP32-OTA-Pull"@1.0.0
RUN arduino-cli lib install JC_Button@2.1.2
RUN arduino-cli lib install PubSubClient@2.8.0
RUN arduino-cli lib install "SdFat"@2.1.1
RUN arduino-cli lib install "SparkFun LIS2DH12 Arduino Library"@1.0.3
RUN arduino-cli lib install "SparkFun MAX1704x Fuel Gauge Arduino Library"@1.0.4
RUN arduino-cli lib install "SparkFun u-blox GNSS v3"@3.1.10
RUN arduino-cli lib install "SparkFun Qwiic OLED Arduino Library"@1.0.13
RUN arduino-cli lib install SSLClientESP32@2.0.0
RUN arduino-cli lib install "SparkFun Extensible Message Parser"@1.0.4
RUN arduino-cli lib install "SparkFun BQ40Z50 Battery Manager Arduino Library"@1.0.0
RUN arduino-cli lib install "ArduinoMqttClient"@0.1.8
RUN arduino-cli lib install "SparkFun u-blox PointPerfect Library"@1.11.4
RUN arduino-cli lib install "SparkFun IM19 IMU Arduino Library"@1.0.1
RUN arduino-cli lib install "SparkFun UM980 Triband RTK GNSS Arduino Library"@1.0.5
RUN arduino-cli lib install "SparkFun LG290P Quadband RTK GNSS Arduino Library"@1.0.8
RUN arduino-cli lib install "SparkFun I2C Expander Arduino Library"@1.0.1
RUN arduino-cli lib install "SparkFun Apple Accessory Arduino Library"@3.0.9
RUN arduino-cli lib install "SparkFun Authentication Coprocessor Arduino Library"@1.0.0
RUN arduino-cli lib install "SparkFun Toolkit"@1.0.6

# Create work directory
WORKDIR /work
ADD . .

# Get current date
RUN echo "$(date +'%b_%d_%Y')" > date_scores.txt
RUN DATE_SCORES=$(cat date_scores.txt) && echo "Date: ${DATE_SCORES}"

RUN echo "$(date +'%b %d %Y')" > date_no_scores.txt
RUN DATE_NO_SCORES=$(cat date_no_scores.txt) && echo "Date: ${DATE_NO_SCORES}"

# Patch libmbedtls and libbt
WORKDIR /work/Patch
RUN ESP_IDF=$(ls /root/.arduino15/packages/esp32/tools/esp32-arduino-libs | grep idf-release) \
    && echo "ESP IDF Version: ${ESP_IDF}" \
    && cp libmbedtls.a "/root/.arduino15/packages/esp32/tools/esp32-arduino-libs/${ESP_IDF}/esp32/lib/libmbedtls.a" \
    && cp libmbedtls_2.a "/root/.arduino15/packages/esp32/tools/esp32-arduino-libs/${ESP_IDF}/esp32/lib/libmbedtls_2.a" \
    && cp libmbedcrypto.a "/root/.arduino15/packages/esp32/tools/esp32-arduino-libs/${ESP_IDF}/esp32/lib/libmbedcrypto.a" \
    && cp libmbedx509.a "/root/.arduino15/packages/esp32/tools/esp32-arduino-libs/${ESP_IDF}/esp32/lib/libmbedx509.a" \
    && cp libbt.a "/root/.arduino15/packages/esp32/tools/esp32-arduino-libs/${ESP_IDF}/esp32/lib/libbt.a"

# Patch NetworkEvents
RUN cp NetworkEvents.* "/root/.arduino15/packages/esp32/hardware/esp32/${CORE_VERSION}/libraries/Network/src/"

