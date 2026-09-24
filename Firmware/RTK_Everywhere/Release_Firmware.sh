#!/bin/bash
#
# Release_Firmware.sh
#    Script to update the contents of RTK-Everywhere-Variants.csv
#    with a new release file
########################################################################
set -e
#set -o verbose
#set -o xtrace

# Get the parameters
major=$1
minor=$2

# Set the output file name
csv_tmp_file=temp.tmp
csv_file=temp.txt

# Make the test program
pushd   ../Tools
make File_CRC
popd

# Delete any previous output
if [ -f "$csv_file" ]; then
    rm $csv_file
fi
if [ -f "$csv_tmp_file" ]; then
    rm $csv_tmp_file
fi

# Get the most recent file
wget   --output-document=$csv_file   https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/raw/refs/heads/main/RTK-Everywhere-Variants.csv
cat $csv_file

# Remove the latest release lines, remember v3.4 uses ESP32,ESP32 instead of SOC,ESP32
awk   '!/\*,SOC,ESP32/||!/,0,0,0,/'   $csv_file   >   $csv_tmp_file
awk   '!/\*,ESP32,ESP32/||!/,0,0,0,/'   $csv_tmp_file   >   $csv_file

# Delete the temporary file
rm   $csv_tmp_file

# Append the ESP32,ESP32 release line for v3.4 to the end of the file
../Tools/File_CRC   ../RTK_Everywhere/build/esp32.esp32.esp32/RTK_Everywhere.ino.bin "*,ESP32,ESP32,$major,$minor,0,0,0,soc/esp32/RTK_Everywhere_Firmware_v"$major"_"$minor".bin" >> temp.txt

# Append the SOC,ESP32 release line for > v3.4 to the end of the file
../Tools/File_CRC   ../RTK_Everywhere/build/esp32.esp32.esp32/RTK_Everywhere.ino.bin "*,SOC,ESP32,$major,$minor,0,0,0,RTK_Everywhere_Firmware_v"$major"_"$minor".bin" >> temp.txt

# Display the updated file
cat temp.txt
