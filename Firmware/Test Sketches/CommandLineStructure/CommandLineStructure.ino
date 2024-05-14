/*
  Demonstration of the command line interface as specified by Avinan Malla
*/

int pin_UART1_TX = 4;
int pin_UART1_RX = 13;

const uint16_t bufferLen = 1024;
char cmdBuffer[bufferLen];

void setup()
{
  Serial.begin(115200);
  delay(250);
  Serial.println();
  Serial.println("SparkFun Command Line Interface Tests");

  sprintf(cmdBuffer, "$CMD*4A"); //Bad command
  Serial.printf("command: %s (BAD) - ", cmdBuffer);
  commandParser();

  sprintf(cmdBuffer, "$SPCMD*AA"); //Bad checksum
  Serial.printf("command: %s (BAD) - ", cmdBuffer);
  commandParser();

  sprintf(cmdBuffer, "$SPCMD*49"); //Valid command
  Serial.printf("command: %s (GOOD) - ", cmdBuffer);
  commandParser();

  Serial.println();

  sprintf(cmdBuffer, "$SPGET,elvMask,15*1A"); //Bad command
  Serial.printf("command: %s (BAD) - ", cmdBuffer);
  commandParser();

  sprintf(cmdBuffer, "$SPGET*55"); //Bad command
  Serial.printf("command: %s (BAD) - ", cmdBuffer);
  commandParser();

  sprintf(cmdBuffer, "$SPGET,maxHeight*0F"); //Unknown setting
  Serial.printf("command: %s (BAD) - ", cmdBuffer);
  commandParser();

  sprintf(cmdBuffer, "$SPGET,elvMask*32"); //Valid command
  Serial.printf("command: %s (GOOD) - ", cmdBuffer);
  commandParser();

  Serial.println();

  sprintf(cmdBuffer, "$SPSET,elvMask*26"); //Incorrect number of arguments
  Serial.printf("command: %s (BAD) - ", cmdBuffer);
  commandParser();

  sprintf(cmdBuffer, "$SPSET,maxHeight,15*33"); //Unknown setting
  Serial.printf("command: %s (BAD) - ", cmdBuffer);
  commandParser();

  sprintf(cmdBuffer, "$SPSET,elvMask,0.77*14"); //Valid
  Serial.printf("command: %s (GOOD) - ", cmdBuffer);
  commandParser();

  Serial.println();

  sprintf(cmdBuffer, "$SPEXE*5B"); //Incorrect number of arguments
  Serial.printf("command: %s (BAD) - ", cmdBuffer);
  commandParser();

  sprintf(cmdBuffer, "$SPEXE,maxHeight*01"); //Unknown command
  Serial.printf("command: %s (BAD) - ", cmdBuffer);
  commandParser();

  sprintf(cmdBuffer, "$SPEXE,EXIT*77"); //Valid
  Serial.printf("command: %s (GOOD) - ", cmdBuffer);
  commandParser();

  sprintf(cmdBuffer, "$SPEXE,APPLY*23"); //Valid
  Serial.printf("command: %s (GOOD) - ", cmdBuffer);
  commandParser();

  sprintf(cmdBuffer, "$SPEXE,SAVE*76"); //Valid
  Serial.printf("command: %s (GOOD) - ", cmdBuffer);
  commandParser();

  sprintf(cmdBuffer, "$SPEXE,REBOOT*76"); //Valid
  Serial.printf("command: %s (GOOD) - ", cmdBuffer);
  commandParser();
  
  sprintf(cmdBuffer, "$SPEXE,LIST*75"); //Valid
  Serial.printf("command: %s (GOOD) - ", cmdBuffer);
  commandParser();
}

void commandParser()
{
  //Verify command structure
  if (commandValid(cmdBuffer) == false)
  {
    commandSendErrorResponse("SP", "Bad command structure");
    return;
  }

  //Remove $
  char *command = cmdBuffer + 1;

  //Remove * and CRC
  command[strlen(command) - 3] = '\0';

  const uint16_t bufferLen = 1024;
  const int MAX_TOKENS = 10;
  char valueBuffer[100];
  char responseBuffer[200];

  char *tokens[MAX_TOKENS];
  const char *delimiter = ",";
  int tokenCount = 0;
  tokens[tokenCount] = strtok(command, delimiter);

  while (tokens[tokenCount] != nullptr && tokenCount < MAX_TOKENS)
  {
    tokenCount++;
    tokens[tokenCount] = strtok(nullptr, delimiter);
  }
  //  if (tokenCount == 0)
  //    continue;

  //Valid commands: CMD, GET, SET, EXE,

  if (strcmp(tokens[0], "SPCMD") == 0)
  {
    commandSendOkResponse(tokens[0]);
  }
  else if (strcmp(tokens[0], "SPGET") == 0)
  {
    if (tokenCount != 2)
      commandSendErrorResponse(tokens[0], "Incorrect number of arguments");
    else
    {
      auto field = tokens[1];
      if (getSettingValue(field, valueBuffer) == true)
        commandSendValueResponse(tokens[0], field, valueBuffer); //Send structured response
      else
        commandSendErrorResponse(tokens[0], field, "Unknown setting");
    }
  }
  else if (strcmp(tokens[0], "SPSET") == 0)
  {
    if (tokenCount != 3)
      commandSendErrorResponse(tokens[0], "Incorrect number of arguments"); //Incorrect number of arguments
    else
    {
      auto field = tokens[1];
      auto value = tokens[2];
      if (updateSettingWithValue(field, value) == true)
        commandSendValueOkResponse(tokens[0], field, value);
      else
        commandSendErrorResponse(tokens[0], field, "Unknown setting");
    }
  }
  else if (strcmp(tokens[0], "SPEXE") == 0)
  {
    if (tokenCount != 2)
      commandSendErrorResponse(tokens[0], "Incorrect number of arguments"); //Incorrect number of arguments
    else
    {
      if (strcmp(tokens[1], "EXIT") == 0)
      {
        commandSendExecuteOkResponse(tokens[0], tokens[1]);
        //Do an exit...
      }
      else if (strcmp(tokens[1], "APPLY") == 0)
      {
        commandSendExecuteOkResponse(tokens[0], tokens[1]);
        //Do an apply...
      }
      else if (strcmp(tokens[1], "SAVE") == 0)
      {
        commandSendExecuteOkResponse(tokens[0], tokens[1]);
        //Do a save...
      }
      else if (strcmp(tokens[1], "REBOOT") == 0)
      {
        commandSendExecuteOkResponse(tokens[0], tokens[1]);
        //Do a reboot...
      }
      else if (strcmp(tokens[1], "LIST") == 0)
      {
        Serial.println(); //TODO remove, just needed for pretty printing

        //Respond with list of variables
        commandSendExecuteListResponse("observationSeconds", "int", "10");
        commandSendExecuteListResponse("observationPositionAccuracy", "float", "0.5");

        commandSendExecuteOkResponse(tokens[0], tokens[1]);
      }
      else
        commandSendErrorResponse(tokens[0], tokens[1], "Unknown command");
    }
  }
  else
  {
    commandSendErrorResponse(tokens[0], "Unknown command");
  }
}

void loop()
{
  if (Serial.available())
  {
    byte incoming = Serial.read();
    if (incoming == 'r')
      ESP.restart();
  }
}

//Given a command, send structured OK response
//Ex: SPCMD = "$SPCMD,OK*61"
void commandSendOkResponse(char *command)
{
  //Create string between $ and * for checksum calculation
  char innerBuffer[200];
  sprintf(innerBuffer, "%s,OK", command);
  commandSendResponse(innerBuffer);
}

//Given a command, send response sentence with OK and checksum and <CR><LR>
//Ex: SPEXE,EXIT =
void commandSendExecuteOkResponse(char *command, char *settingName)
{
  //Create string between $ and * for checksum calculation
  char innerBuffer[200];
  sprintf(innerBuffer, "%s,%s,OK", command, settingName);
  commandSendResponse(innerBuffer);
}

//Given a command, send response sentence with OK and checksum and <CR><LR>
//Ex: observationPositionAccuracy,float,0.5 =
void commandSendExecuteListResponse(char *settingName, char *settingType, char *settingValue)
{
  //Create string between $ and * for checksum calculation
  char innerBuffer[200];
  sprintf(innerBuffer, "SPLST,%s,%s,%s", settingName, settingType, settingValue);
  commandSendResponse(innerBuffer);
}

//Given a command, and a value, send response sentence with checksum and <CR><LR>
//Ex: SPGET,elvMask,0.25 = "$SPGET,elvMask,0.25*07"
void commandSendValueResponse(char *command, char *settingName, char *valueBuffer)
{
  //Create string between $ and * for checksum calculation
  char innerBuffer[200];
  sprintf(innerBuffer, "%s,%s,%s", command, settingName, valueBuffer);
  commandSendResponse(innerBuffer);
}

//Given a command, and a value, send response sentence with OK and checksum and <CR><LR>
//Ex: SPSET,elvMask,0.77 = "$SPSET,elvMask,0.77,OK*3C"
void commandSendValueOkResponse(char *command, char *settingName, char *valueBuffer)
{
  //Create string between $ and * for checksum calculation
  char innerBuffer[200];
  sprintf(innerBuffer, "%s,%s,%s,OK", command, settingName, valueBuffer);
  commandSendResponse(innerBuffer);
}

// Given a command, send structured ERROR response
// Response format: $SPxET,[setting name],,ERROR,[Verbose error description]*FF<CR><LF>
// Ex: SPGET,maxHeight,'Unknown setting' = "$SPGET,maxHeight,,ERROR,Unknown setting*58"
void commandSendErrorResponse(char *command, char *settingName, char *errorVerbose)
{
    // Create string between $ and * for checksum calculation
    char innerBuffer[200];
    snprintf(innerBuffer, sizeof(innerBuffer), "%s,%s,,ERROR,%s", command, settingName, errorVerbose);
    commandSendResponse(innerBuffer);
}
// Given a command, send structured ERROR response
// Response format: $SPxET,,,ERROR,[Verbose error description]*FF<CR><LF>
// Ex: SPGET, 'Incorrect number of arguments' = "$SPGET,ERROR,Incorrect number of arguments*1E"
void commandSendErrorResponse(char *command, char *errorVerbose)
{
    // Create string between $ and * for checksum calculation
    char innerBuffer[200];
    snprintf(innerBuffer, sizeof(innerBuffer), "%s,,,ERROR,%s", command, errorVerbose);
    commandSendResponse(innerBuffer);
}

//Given an inner buffer, send response sentence with checksum and <CR><LR>
//Ex: SPGET,elvMask,0.25 = $SPGET,elvMask,0.25*07
void commandSendResponse(char *innerBuffer)
{
  char responseBuffer[200];

  uint8_t calculatedChecksum = 0; // XOR chars between '$' and '*'
  for (int x = 0; x < strlen(innerBuffer); x++)
    calculatedChecksum = calculatedChecksum ^ innerBuffer[x];

  sprintf(responseBuffer, "$%s*%02X\r\n", innerBuffer, calculatedChecksum);

  Serial.print(responseBuffer);
}

// Checks structure of command and checksum
// If valid, returns true
// $SPCMD*49
// $SPGET,elvMask,15*1A
// getUserInputString() has removed any trailing <CR><LF> to the command
bool commandValid(char* commandString)
{
  //Check $/*
  if (commandString[0] != '$')
    return (false);

  if (commandString[strlen(commandString) - 3] != '*')
    return (false);

  //Check checksum is HEX
  char checksumMSN = commandString[strlen(commandString) - 2];
  char checksumLSN = commandString[strlen(commandString) - 1];
  if (isxdigit(checksumMSN) == false || isxdigit(checksumLSN) == false)
    return (false);

  //Convert checksum from ASCII to int
  char checksumChar[3] = {'\0'};
  checksumChar[0] = checksumMSN;
  checksumChar[1] = checksumLSN;
  uint8_t checksum = strtol(checksumChar, NULL, 16);

  // From: http://engineeringnotes.blogspot.com/2015/02/generate-crc-for-nmea-strings-arduino.html
  uint8_t calculatedChecksum = 0; // XOR chars between '$' and '*'
  for (int x = 1; x < strlen(commandString) - 3; x++)
    calculatedChecksum = calculatedChecksum ^ commandString[x];

  if (checksum != calculatedChecksum)
    return (false);

  return (true);
}
