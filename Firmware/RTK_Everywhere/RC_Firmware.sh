#!/bin/bash
#
# RC_Firmware.sh
#    Script to update the contents of RTK-Everywhere-Variants.csv
#    with a new release candidate file
########################################################################
set -e
#set -o verbose
#set -o xtrace

# Get the parameters
major=$1
minor=$2

# Get the date values
year=$(date +%Y)
month=$(date +%m)
day_of_month=$(date +%d)

# Make the test program
pushd   ../Tools
make File_CRC
popd

# Set the output file name
csv_tmp_file=temp.tmp
csv_file=temp.txt

# Delete any previous output
if [ -f "$csv_file" ]; then
    rm $csv_file
fi
if [ -f "$csv_tmp_file" ]; then
    rm $csv_tmp_file
fi

# Get the most recent file
wget   --output-document=$csv_file   https://github.com/sparkfun/SparkFun_RTK_Everywhere_Firmware_Binaries/raw/refs/heads/main/RTK-Everywhere-Variants.csv

# Remove the release candidate (RC) line
awk   '!/\*,SOC,ESP32/||!/,1,/'   $csv_file   >   $csv_tmp_file

# Rename temp file to output file
mv   $csv_tmp_file $csv_file

# Append the new SOC,ESP32 release candidate line to the end of the file
../Tools/File_CRC   ../RTK_Everywhere/build/esp32.esp32.esp32/RTK_Everywhere.ino.bin "*,SOC,ESP32,$major,$minor,0,0,1,RTK_Everywhere_Firmware_RC_"$year$month$day_of_month".bin" >> $csv_file

# Display the updated file
cat $csv_file
