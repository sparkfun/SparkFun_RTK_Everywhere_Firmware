// Enable printfs to various endpoints
// https://stackoverflow.com/questions/42131753/wrapper-for-printf
void systemPrintf(const char *format, ...)
{
  va_list args;
  va_start(args, format);

  va_list args2;
  va_copy(args2, args);
  char buf[vsnprintf(nullptr, 0, format, args) + 1];

  vsnprintf(buf, sizeof buf, format, args2);

  systemPrint(buf);

  va_end(args);
  va_end(args2);
}

// If we are printing to all endpoints, BT gets priority
int systemAvailable()
{
//  if (bluetoothIsConnected())
//    return (bluetoothAvailable());

  return (Serial.available());
}

// If we are reading all endpoints, BT gets priority
int systemRead()
{
//  if (bluetoothIsConnected())
//    return (bluetoothRead());

  return (Serial.read());
}

// Output a buffer of the specified length to the serial port
void systemWrite(const uint8_t *buffer, uint16_t length)
{
  //bluetoothWrite(buffer, length);
  Serial.write(buffer, length);
}

// Ensure all serial output has been transmitted, FIFOs are empty
void systemFlush()
{
  //bluetoothFlush();
  Serial.flush();
}

// Output a byte to the serial port
void systemWrite(uint8_t value)
{
  systemWrite(&value, 1);
}

// Point the string at the selected endpoint
void systemPrint(const char *string)
{
  systemWrite((const uint8_t *)string, strlen(string));
}

// Print a string with a carriage return and linefeed
void systemPrintln(const char *value)
{
  systemPrint(value);
  systemPrintln();
}

// Print an integer value
void systemPrint(int value)
{
  char temp[20];
  snprintf(temp, sizeof(temp), "%d", value);
  systemPrint(temp);
}

// Print an integer value as HEX or decimal
void systemPrint(int value, uint8_t printType)
{
  char temp[20];

  if (printType == HEX)
    snprintf(temp, sizeof(temp), "%08X", value);
  else if (printType == DEC)
    snprintf(temp, sizeof(temp), "%d", value);

  systemPrint(temp);
}

// Pretty print IP addresses
void systemPrint(IPAddress ipaddress)
{
  systemPrint(ipaddress.toString().c_str());
}
void systemPrintln(IPAddress ipaddress)
{
  systemPrint(ipaddress);
  systemPrintln();
}

// Print an integer value with a carriage return and line feed
void systemPrintln(int value)
{
  systemPrint(value);
  systemPrintln();
}

// Print an 8-bit value as HEX or decimal
void systemPrint(uint8_t value, uint8_t printType)
{
  char temp[20];

  if (printType == HEX)
    snprintf(temp, sizeof(temp), "%02X", value);
  else if (printType == DEC)
    snprintf(temp, sizeof(temp), "%d", value);

  systemPrint(temp);
}

// Print an 8-bit value as HEX or decimal with a carriage return and linefeed
void systemPrintln(uint8_t value, uint8_t printType)
{
  systemPrint(value, printType);
  systemPrintln();
}

// Print a 16-bit value as HEX or decimal
void systemPrint(uint16_t value, uint8_t printType)
{
  char temp[20];

  if (printType == HEX)
    snprintf(temp, sizeof(temp), "%04X", value);
  else if (printType == DEC)
    snprintf(temp, sizeof(temp), "%d", value);

  systemPrint(temp);
}

// Print a 16-bit value as HEX or decimal with a carriage return and linefeed
void systemPrintln(uint16_t value, uint8_t printType)
{
  systemPrint(value, printType);
  systemPrintln();
}

// Print a floating point value with a specified number of decimal places
void systemPrint(float value, uint8_t decimals)
{
  char temp[20];
  snprintf(temp, sizeof(temp), "%.*f", decimals, value);
  systemPrint(temp);
}

// Print a floating point value with a specified number of decimal places and a
// carriage return and linefeed
void systemPrintln(float value, uint8_t decimals)
{
  systemPrint(value, decimals);
  systemPrintln();
}

// Print a double precision floating point value with a specified number of decimal places
void systemPrint(double value, uint8_t decimals)
{
  char temp[30];
  snprintf(temp, sizeof(temp), "%.*f", decimals, value);
  systemPrint(temp);
}

// Print a double precision floating point value with a specified number of decimal
// places and a carriage return and linefeed
void systemPrintln(double value, uint8_t decimals)
{
  systemPrint(value, decimals);
  systemPrintln();
}

// Print a string
void systemPrint(String myString)
{
  systemPrint(myString.c_str());
}
void systemPrintln(String myString)
{
  systemPrint(myString);
  systemPrintln();
}

// Print a carriage return and linefeed
void systemPrintln()
{
  systemPrint("\r\n");
}
