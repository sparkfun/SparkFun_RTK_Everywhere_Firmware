//----------------------------------------
// Configure UART2 serial port shared between LoRa and Tilt
// This only applies to the FP. The Torch has tilt connected direct to ESP UART0 (shared with USB)
//----------------------------------------
bool beginUart2Serial()
{
    // Determine if serial port is already configured
    if (uart2Serial)
        return true;

    // Allocate the serial port object
    uart2Serial = new HardwareSerial(2);

    // Determine if the allocation failed
    if (uart2Serial == nullptr)
    {
        systemPrintf("ERROR: Failed to allocate the uart2Serial port!\r\n");
        return false;
    }

    // Configure the serial port
    //uart2Serial->setRxBufferSize(settings.uartReceiveBufferSize);
    //uart2Serial->setTimeout(settings.serialTimeoutGNSS); // Requires serial traffic on the UART pins for detection
    uart2Serial->begin(115200, SERIAL_8N1, pin_IMU_RX, pin_IMU_TX);
    return true;
}

//----------------------------------------
// Select the desired *.enc file from the microSD root directory
// Return the file name in the global char array fileName
// Use the global file object (File / File32 / ExFile / FsFile) to check the file size
// Use the global dir object to access the directory
//----------------------------------------
bool selectFirmwareFile()
{
    systemPrintln("Select firmware file:");

    // Open root directory
    if (!dir.open("/")) {
        systemPrintln("ERROR: dir.open failed");
        return false;
    }

    // Open next file in root.
    // Warning, openNext starts at the current position of dir so a
    // rewind may be necessary in your application.
    bool fileSelected = false;
    while (file.openNext(&dir, O_RDONLY)) {
        if (!file.isDir()) {
            file.getName(fileName, sizeof(fileName));
            size_t fileNameLen = strlen(fileName);
            if (fileNameLen > 10) {
                if ((fileName[fileNameLen - 4] == '.') &&
                    ((fileName[fileNameLen - 3] == 'E') || (fileName[fileNameLen - 3] == 'e')) &&
                    ((fileName[fileNameLen - 2] == 'N') || (fileName[fileNameLen - 2] == 'n')) &&
                    ((fileName[fileNameLen - 1] == 'C') || (fileName[fileNameLen - 1] == 'c'))) {
                    systemPrint("Update with ");
                    systemPrint(fileName);
                    systemPrint(" (");
                    systemPrint(file.fileSize());
                    systemPrintln(" bytes) (y/N)?");
                    do {
                        if (systemAvailable())
                        {
                            byte incoming = systemRead();
                            if ((incoming == 'Y') || (incoming == 'y'))
                                fileSelected = true;
                            break;
                        }
                    } while (1);
                }
            }
        }
        if (!fileSelected)
            file.close();
        else
            break;
    }

    if (!file)
    {
        systemPrintln("ERROR: no firmware file found / selected");
        return false;
    }

    file.close(); // im19 methods will re-open the file

    return true;
}

//----------------------------------------
// Print the menu to systemPrint (Serial)
//----------------------------------------
void printMenu()
{
    systemPrintln();
    systemPrintln("r) Reset ESP32");
    systemPrintln("u) Update firmware with *.enc from the SD card root folder");
}
