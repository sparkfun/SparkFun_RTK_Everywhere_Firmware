/**********************************************************************
  Serial.ino

  Robots-For-All (R4A)
  Serial stream support
**********************************************************************/

#include "Telnet.h"

//*********************************************************************
// Read a line of input from a Serial port into a String
String * r4aReadLine(bool echo, String * buffer, HardwareSerial * port)
{
    char data;
    String * line;

    // Wait for an input character
    line = nullptr;
    while (port->available())
    {
        // Get the input character
        data = port->read();
        if ((data != '\r') && (data != '\n'))
        {
            // Handle backspace
            if (data == 8)
            {
                // Output a bell when the buffer is empty
                if (buffer->length() <= 0)
                    port->write(7);
                else
                {
                    // Remove the character from the line
                    port->write(data);
                    port->write(' ');
                    port->write(data);

                    // Remove the character from the buffer
                    *buffer = buffer->substring(0, buffer->length() - 1);
                }
            }
            else
            {
                // Echo the character
                if (echo)
                    port->write(data);

                // Add the character to the line
                *buffer += data;
            }
        }

        // Echo a carriage return and linefeed
        else if (data == '\r')
        {
            if (echo)
                port->println();
            line = buffer;
            break;
        }

        // Echo the linefeed
        else if (echo && (data == '\n'))
            port->println();
    }

    // Return the line when it is complete
    return line;
}

//*********************************************************************
// Repeatedly display a fatal error message
void r4aReportFatalError(const char * errorMessage,
                         Print * display)
{
    static uint32_t lastDisplayMsec;

    while (true)
    {
        if ((millis() - lastDisplayMsec) >= (15 * R4A_MILLISECONDS_IN_A_SECOND))
        {
            lastDisplayMsec = millis();
            display->printf("ERROR: %s\r\n", errorMessage);
        }
    }
}

//*********************************************************************
// Process serial menu item
void r4aSerialMenu()
{
    const char * command;
    String * line;
    static String serialBuffer;

    // Process input from the serial port
    line = r4aReadLine(true, &serialBuffer, &Serial);
    if (line)
    {
        // Get the command
        command = line->c_str();

        // Process the command
        processMenu(command, &Serial);

        // Start building the next command
        serialBuffer = "";
    }
}
