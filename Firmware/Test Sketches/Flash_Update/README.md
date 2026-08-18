Flash Update Examples
===========================================================

The following sketches demonstrate how to update the sub-system's firmware over WiFi, SD, or compiled-in array. Generally, a new subsystem's update path is proven to work over SD or an array, but because the main firmware (RTK Everywhere) will rely on WiFi, that is the path we focus on. 

* ESP32 ✅: Torch ✅ / TX2 ✅ / FP ✅ / Postcard ✅ / Facet mosaic-X5 ✅
* STM32: Torch ✅ / FP ✅
* IM19 ✅: Torch ⚠️ - Update completes successfully but on the original Torch series has old 2022 firmware that seems not to allow new firmware. / FP ✅ - The final version check is failing but shows correct version after system reset.
* X20P: FPX ✅ Works repeatably on X20P on FP.
* LG290P ✅: TX2 / FPL / Postcard - There is an array example here, but the LG290P update is built into the library and is already working in the RC branch of RTK Everywhere.
* mosaic-X5: FPM ❌ / Facet mosaic-X5 ❌ - No support yet
