#!/bin/bash
#
# check.sh
#    Script to modify RTK_Everywhere.ino to comment out various defines
#    and verify that the code still builds successfully.  This script
#    stops execution upon error due to the following command.
########################################################################
set -e
#set -o verbose
#set -o xtrace

# Start fresh
git reset --hard --quiet HEAD
make

# Bluetooth
sed -i 's|#define COMPILE_BT|//#define COMPILE_BT|' RTK_Everywhere.ino
make
git reset --hard --quiet HEAD

# Network
sed -i 's|#define COMPILE_WIFI|//#define COMPILE_WIFI|' RTK_Everywhere.ino
sed -i 's|#define COMPILE_ETHERNET|//#define COMPILE_ETHERNET|' RTK_Everywhere.ino
sed -i 's|#define COMPILE_CELLULAR|//#define COMPILE_CELLULAR|' RTK_Everywhere.ino
make
git reset --hard --quiet  HEAD

# Cellular
sed -i 's|#define COMPILE_CELLULAR|//#define COMPILE_CELLULAR|' RTK_Everywhere.ino
make
git reset --hard --quiet  HEAD

# Ethernet
sed -i 's|#define COMPILE_ETHERNET|//#define COMPILE_ETHERNET|' RTK_Everywhere.ino
make
git reset --hard --quiet  HEAD

# WiFi
sed -i 's|#define COMPILE_WIFI|//#define COMPILE_WIFI|' RTK_Everywhere.ino
make
git reset --hard --quiet  HEAD

# Soft AP
sed -i 's|#define COMPILE_AP|//#define COMPILE_AP|' RTK_Everywhere.ino
make
git reset --hard --quiet  HEAD

# ESPNOW
sed -i 's|#define COMPILE_ESPNOW|//#define COMPILE_ESPNOW|' RTK_Everywhere.ino
make
git reset --hard --quiet  HEAD

# WiFi, SOFT AP, ESPNOW
sed -i 's|#define COMPILE_WIFI|//#define COMPILE_WIFI|' RTK_Everywhere.ino
sed -i 's|#define COMPILE_AP|//#define COMPILE_AP|' RTK_Everywhere.ino
sed -i 's|#define COMPILE_ESPNOW|//#define COMPILE_ESPNOW|' RTK_Everywhere.ino
make
git reset --hard --quiet  HEAD

# LG290P
sed -i 's|#define COMPILE_LG290P|//#define COMPILE_LG290P|' RTK_Everywhere.ino
make
git reset --hard --quiet  HEAD

# MOSAIC X5
sed -i 's|#define COMPILE_MOSAICX5|//#define COMPILE_MOSAICX5|' RTK_Everywhere.ino
make
git reset --hard --quiet  HEAD

# UM980
sed -i 's|#define COMPILE_UM980|//#define COMPILE_UM980|' RTK_Everywhere.ino
make
git reset --hard --quiet  HEAD

# ZED F9P
sed -i 's|#define COMPILE_ZED|//#define COMPILE_ZED|' RTK_Everywhere.ino
make
git reset --hard --quiet  HEAD

# GNSS None
sed -i 's|#define COMPILE_LG290P|//#define COMPILE_LG290P|' RTK_Everywhere.ino
sed -i 's|#define COMPILE_MOSAICX5|//#define COMPILE_MOSAICX5|' RTK_Everywhere.ino
sed -i 's|#define COMPILE_UM980|//#define COMPILE_UM980|' RTK_Everywhere.ino
sed -i 's|#define COMPILE_ZED|//#define COMPILE_ZED|' RTK_Everywhere.ino
make
git reset --hard --quiet  HEAD

# L-Band
sed -i 's|#define COMPILE_L_BAND|//#define COMPILE_L_BAND|' RTK_Everywhere.ino
make
git reset --hard --quiet  HEAD

# IM19_IMU
sed -i 's|#define COMPILE_IM19_IMU|//#define COMPILE_IM19_IMU|' RTK_Everywhere.ino
make
git reset --hard --quiet  HEAD

# MP2762A Charger
sed -i 's|#define COMPILE_MP2762A_CHARGER|//#define COMPILE_MP2762A_CHARGER|' RTK_Everywhere.ino
make
git reset --hard --quiet  HEAD

# PointPerfect Library
sed -i 's|#define COMPILE_POINTPERFECT_LIBRARY|//#define COMPILE_POINTPERFECT_LIBRARY|' RTK_Everywhere.ino
make
git reset --hard --quiet  HEAD

# BQ40Z50
sed -i 's|#define COMPILE_BQ40Z50|//#define COMPILE_BQ40Z50|' RTK_Everywhere.ino
make
git reset --hard --quiet  HEAD

# MQTT Client
sed -i 's|#define COMPILE_MQTT_CLIENT|//#define COMPILE_MQTT_CLIENT|' RTK_Everywhere.ino
make
git reset --hard --quiet  HEAD

# OTA Auto
sed -i 's|#define COMPILE_OTA_AUTO|//#define COMPILE_OTA_AUTO|' RTK_Everywhere.ino
make
git reset --hard --quiet  HEAD

# HTTP Client
sed -i 's|#define COMPILE_HTTP_CLIENT|//#define COMPILE_HTTP_CLIENT|' RTK_Everywhere.ino
make
git reset --hard --quiet  HEAD
