/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Device_Update_LoRa.ino

  Update LoRa (STM32) firmware
  See https://www.st.com/resource/en/application_note/CD00264342.pdf

  Example Linux command to covert Intel Hex files into binary files:

  objcopy   --input-target=ihex   --output-target=binary   --gap-fill 0xff   SparkPNT_LoRa_3.0.1.hex   SparkPNT_LoRa_3.0.1.bin

=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#ifdef  COMPILE_LORA

//----------------------------------------
// Perform the cleanup after the firmware download
//----------------------------------------
void dfuLoRaClose(DEVICE_FIRMWARE_CTX * ctx)
{
    systemPrintf("Update Complete. Resetting IC...");

    gpioExpanderLoraBootDisable();  // Pull BOOT0 low to exit bootloader mode on reset
    dfuLoRaReset();                 // Power cycle LoRa to reset into normal mode
}

//----------------------------------------
// Initialize the device specific context structure
//----------------------------------------
bool dfuLoRaCtxInit(DEVICE_FIRMWARE_CTX * ctx)
{
    DFU_STM32_CONTEXT * stm32Ctx;

    stm32Ctx = (DFU_STM32_CONTEXT *)(ctx->_devCtx);
    stm32Ctx->_stm32Serial = SerialForLoRa;
    return true;
}

//----------------------------------------
// Get the firmware version
//----------------------------------------
String dfuLoRaGetFirmwareVersion(DEVICE_FIRMWARE_CTX * ctx)
{
    return String(loraFirmwareVersion);
}

//----------------------------------------
// There is not a hardware reset pin exposed. Power cycle the device.
//----------------------------------------
void dfuLoRaReset()
{
    gpioExpanderLoraDisable(); // Power off
    delay(100);
    gpioExpanderLoraEnable(); // Power on
    delay(100);
}

//----------------------------------------
// Reset the LoRa (STM32) and start the boot loader
//----------------------------------------
bool dfuLoRaReset(DEVICE_FIRMWARE_CTX * ctx, uint32_t currentMsec)
{
    // Connect ESP32 UART2 to LoRa UART2 via SW3 for configuration and bootloading/firmware updates
    gpioExpanderSelectLoraConfigure();
    serialInputClear(SerialForLoRa);

    // Pull BOOT0 high to enter bootloader mode on reset
    gpioExpanderLoraBootEnable();

    // Power cycle the LoRa device to perform the reset and enter
    // bootloader mode
    dfuLoRaReset();

    // Send 0x7F for auto-baud detection
    return dfuStm32Autobaud(ctx);
}

#endif  // COMPILE_LORA
