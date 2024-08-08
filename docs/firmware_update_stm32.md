# Updating STM32 Firmware

Torch: ![Feature Supported](img/Icons/GreenDot.png) / EVK: ![Feature Not Supported](img/Icons/RedDot.png)

The STM32WLE firmware runs the 915MHz LoRa radio inside the RTK Torch. The firmware version number is displayed in the radio menu:

![RTK Express with firmware v3.0](<img/Displays/SparkFun%20RTK%20Boot%20Screen%20Version%20Number.png>)

*RTK Torch LoRa radio firmware version 2.0.1*

Firmware updates to the STM32WLE can only be done over the serial interface. Follow these steps to update the LoRa radio firmware on the RTK Torch.

   ![UI of STM32CubeProgrammer](<img/Firmware/SparkFun RTK Everywhere - STM32CubeProgrammer.png>)

1. Download and install [STM32CubeProgrammer](https://www.st.com/en/development-tools/stm32cubeprog.html). While it *is* available for Windows/Linux/iOS, ST makes it rather difficult to get this software. We're sorry!

    ![Debug menu showing the STM32 direct connect option](<img/Firmware/SparkFun RTK Everywhere - STM32 Passthrough Menu.png>)

2. Open the main menu and select System (**s**), Hardware Debug (**h**), STM32 direct connect (**17**).

   ![Passthrough mode output on reset](<img/Firmware/SparkFun RTK Everywhere - STM32 Passthrough 1.png>)

3. The device will automatically reset and show very little including various symbols. The device is now operating in pass-through mode at 57600bps, and ready to be programmed. If needed, to exit this pass-through mode, press and release the main power button.

4. Close the terminal connection. This will likely cause the device to reset - that is ok.

   ![Settings in STM32CubeProgrammer](<img/Firmware/SparkFun RTK Everywhere - STM32CubeProgrammer Callouts.png>)

5. Open STM32CubeProgrammer. Select UART (blue box) and the programming interface. Set the Baudrate to 57600, No parity, RTS and DTR set to high.

   ![Don't select Read Unprotected](<img/Firmware/SparkFun RTK Everywhere - STM32CubeProgrammer Read Unprotected.png>)

6. Avoid **Read Unprotected**. Do not enable. This setting writes to fuse bits and if there is a problem with serial communication, it can lead to an inoperable bootloader. The device can be repaired but only at SparkFun (we have to use a ST-Link to reprogram the fust bits over the SWD interface).

7. Select the COM port associated with COM-B of the RTK device. Not sure? Read [here](https://docs.sparkfun.com/SparkFun_RTK_Everywhere_Firmware/configure_with_serial/#rtk-torch). Once selected, click **Connect**.

