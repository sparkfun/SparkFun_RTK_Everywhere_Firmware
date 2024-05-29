/*
  Demonstration of the command line interface as specified by Avinab Malla
*/

typedef enum
{
  SETTING_UNKNOWN = 0,
  SETTING_KNOWN,
  SETTING_KNOWN_STRING,
} SettingValueResponse;

const uint16_t bufferLen = 1024;
char cmdBuffer[bufferLen];

void setup()
{
  Serial.begin(115200);
  while (!Serial);
  delay(250);
  Serial.println();
  Serial.println("SparkFun Command Line Interface Tests");

  sprintf(cmdBuffer, "$CMD*4A"); //Bad command
  Serial.printf("command: %s (Unknown command) - ", cmdBuffer);
  commandParser();

  sprintf(cmdBuffer, "$SPCMD*AA"); //Bad checksum
  Serial.printf("command: %s (Bad checksum) - ", cmdBuffer);
  commandParser();

  sprintf(cmdBuffer, "$SPCMD*49"); //Valid command
  Serial.printf("command: %s (Valid) - ", cmdBuffer);
  commandParser();

  Serial.println();

  sprintf(cmdBuffer, "$SPGET,elvMask,15*1A"); //Too many arguments
  Serial.printf("command: %s (Too many arguments) - ", cmdBuffer);
  commandParser();

  sprintf(cmdBuffer, "$SPGET*55"); //Bad command
  Serial.printf("command: %s (Missing command) - ", cmdBuffer);
  commandParser();

  sprintf(cmdBuffer, "$SPGET,maxHeight*32"); //Unknown setting
  Serial.printf("command: %s (Unknown setting) - ", cmdBuffer);
  commandParser();

  sprintf(cmdBuffer, "$SPGET,elvMask*32"); //Valid command
  Serial.printf("command: %s (Valid) - ", cmdBuffer);
  commandParser();

  sprintf(cmdBuffer, "$SPGET,ntripClientCasterUserPW*35"); //Valid command, response with escaped quote
  Serial.printf("command: %s (Valid) - ", cmdBuffer);
  commandParser();


  Serial.println();

  sprintf(cmdBuffer, "$SPSET,elvMask*26"); //Incorrect number of arguments
  Serial.printf("command: %s (Wrong number of arguments) - ", cmdBuffer);
  commandParser();

  sprintf(cmdBuffer, "$SPSET,maxHeight,15*0E"); //Unknown setting
  Serial.printf("command: %s (Unknown setting) - ", cmdBuffer);
  commandParser();

  sprintf(cmdBuffer, "$SPSET,ntripClientCasterUserPW,\"casterPW*3A"); //Missing closing quote
  Serial.printf("command: %s (Missing quote) - ", cmdBuffer);
  commandParser();

  sprintf(cmdBuffer, "$SPSET,elvMask,0.77*14"); //Valid
  Serial.printf("command: %s (Valid) - ", cmdBuffer);
  commandParser();

  sprintf(cmdBuffer, "$SPSET,ntripClientCasterUserPW,\"MyPass\"*08"); //Valid string
  Serial.printf("command: %s (Valid) - ", cmdBuffer);
  commandParser();

  sprintf(cmdBuffer, "$SPSET,ntripClientCasterUserPW,\"complex,password\"*5E"); //Valid string with an internal comma
  Serial.printf("command: %s (Valid) - ", cmdBuffer);
  commandParser();

  sprintf(cmdBuffer, "$SPSET,ntripClientCasterUserPW,\"pwWith\\\"quote\"*2C"); //Valid string with an escaped quote
  Serial.printf("command: %s (Valid) - ", cmdBuffer);
  commandParser();

  sprintf(cmdBuffer, "$SPSET,ntripClientCasterUserPW,\"a55G\\\"e,e#\"*5A"); //Valid string with an internal escaped quote, and internal comma
  Serial.printf("command: %s (Valid) - ", cmdBuffer);
  commandParser();

  Serial.println();

  sprintf(cmdBuffer, "$SPEXE*5B"); //Incorrect number of arguments
  Serial.printf("command: %s (Wrong number of arguments) - ", cmdBuffer);
  commandParser();

  sprintf(cmdBuffer, "$SPEXE,maxHeight*3C"); //Unknown command
  Serial.printf("command: %s (Unknown command) - ", cmdBuffer);
  commandParser();

  sprintf(cmdBuffer, "$SPEXE,EXIT*77"); //Valid
  Serial.printf("command: %s (Valid) - ", cmdBuffer);
  commandParser();

  sprintf(cmdBuffer, "$SPEXE,APPLY*23"); //Valid
  Serial.printf("command: %s (Valid) - ", cmdBuffer);
  commandParser();

  sprintf(cmdBuffer, "$SPEXE,SAVE*76"); //Valid
  Serial.printf("command: %s (Valid) - ", cmdBuffer);
  commandParser();

  sprintf(cmdBuffer, "$SPEXE,REBOOT*76"); //Valid
  Serial.printf("command: %s (Valid) - ", cmdBuffer);
  commandParser();

  sprintf(cmdBuffer, "$SPEXE,LIST*75"); //Valid
  Serial.printf("command: %s (Valid) - ", cmdBuffer);
  commandParser();
}

void commandParser()
{
  //Verify command structure
  if (commandValid(cmdBuffer) == false)
  {
    commandSendErrorResponse((char*)"SP", (char*)"Bad command structure or checksum");
    return;
  }

  //Remove $
  memmove(cmdBuffer, &cmdBuffer[1], sizeof(cmdBuffer) - 1);

  //Change * to , and null terminate on the first CRC character
  cmdBuffer[strlen(cmdBuffer) - 3] = ',';
  cmdBuffer[strlen(cmdBuffer) - 2] = '\0';

  const int MAX_TOKENS = 10;
  char valueBuffer[100];

  char *tokens[MAX_TOKENS];
  int tokenCount = 0;
  int originalLength = strlen(cmdBuffer);

  //We can't use strtok because there may be ',' inside of settings (ie, wifi password: "hello,world")

  tokens[tokenCount++] = &cmdBuffer[0]; //Point at first token

  bool inQuote = false;
  bool inEscape = false;
  for (int spot = 0; spot < originalLength ; spot++)
  {
    if (cmdBuffer[spot] == ',' && inQuote == false)
    {
      if (spot < (originalLength - 1))
      {
        cmdBuffer[spot++] = '\0';
        tokens[tokenCount++] = &cmdBuffer[spot];
      }
      else
      {
        //We are at the end of the string, just terminate the last token
        cmdBuffer[spot] = '\0';
      }

      if (inEscape == true)
        inEscape = false;
    }

    //Handle escaped quotes
    if (cmdBuffer[spot] == '\\' && inEscape == false)
    {
      //Ignore next character from quote checks
      inEscape = true;
    }
    else if (cmdBuffer[spot] == '\"' && inEscape == true)
      inEscape = false;
    else if (cmdBuffer[spot] == '\"' && inQuote == false && inEscape == false)
      inQuote = true;
    else if (cmdBuffer[spot] == '\"' && inQuote == true)
      inQuote = false;
  }

  if (inQuote == true)
  {
    commandSendErrorResponse((char*)"SP", (char*)"Unclosed quote");
    return;
  }

  //Trim surrounding quotes from any token
  for (int x = 0 ; x < tokenCount ; x++)
  {
    //Remove leading "
    if (tokens[x][0] == '"')
      tokens[x] = &tokens[x][1];

    //Remove trailing "
    if (tokens[x][strlen(tokens[x]) - 1] == '"')
      tokens[x][strlen(tokens[x]) - 1] = '\0';
  }

  //Remove escape characters from within tokens
  //https://stackoverflow.com/questions/53134028/remove-all-from-a-string-in-c
  for (int x = 0 ; x < tokenCount ; x++)
  {
    int k = 0;
    for (int i = 0; tokens[x][i] != '\0'; ++i)
      if (tokens[x][i] != '\\')
        tokens[x][k++] = tokens[x][i];
    tokens[x][k] = '\0';
  }

  //  Serial.printf("Token count: %d\r\n", tokenCount);
  //  Serial.printf("Token[0]: %s\r\n", tokens[0]);
  //  Serial.printf("Token[1]: %s\r\n", tokens[1]);
  //  Serial.println();

  //Valid commands: CMD, GET, SET, EXE,

  if (strcmp(tokens[0], "SPCMD") == 0)
  {
    commandSendOkResponse(tokens[0]);
  }
  else if (strcmp(tokens[0], "SPGET") == 0)
  {
    if (tokenCount != 2)
      commandSendErrorResponse(tokens[0], (char*)"Incorrect number of arguments");
    else
    {
      auto field = tokens[1];

      SettingValueResponse response = getSettingValue(field, valueBuffer);

      if (response == SETTING_KNOWN)
        commandSendValueResponse(tokens[0], field, valueBuffer); //Send structured response
      else if (response == SETTING_KNOWN_STRING)
        commandSendStringResponse(tokens[0], field, valueBuffer); //Wrap the string setting in quotes in the response, no OK
      else
        commandSendErrorResponse(tokens[0], (char*)"Unknown setting");
    }
  }
  else if (strcmp(tokens[0], "SPSET") == 0)
  {
    if (tokenCount != 3)
      commandSendErrorResponse(tokens[0], (char*)"Incorrect number of arguments"); //Incorrect number of arguments
    else
    {
      auto field = tokens[1];
      auto value = tokens[2];

      SettingValueResponse response = updateSettingWithValue(field, value);
      if (response == SETTING_KNOWN)
        commandSendValueOkResponse(tokens[0], field, value); //Just respond with the setting (not quotes needed)
      else if (response == SETTING_KNOWN_STRING)
        commandSendStringOkResponse(tokens[0], field, value); //Wrap the string setting in quotes in the response, add OK
      else
        commandSendErrorResponse(tokens[0], (char*)"Unknown setting");
    }
  }
  else if (strcmp(tokens[0], "SPEXE") == 0)
  {
    if (tokenCount != 2)
      commandSendErrorResponse(tokens[0], (char*)"Incorrect number of arguments"); //Incorrect number of arguments
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
        commandSendExecuteListResponse((char*)"observationSeconds", (char*)"int", (char*)"10");
        commandSendExecuteListResponse((char*)"observationPositionAccuracy", (char*)"float", (char*)"0.5");

        commandSendExecuteOkResponse(tokens[0], tokens[1]);
      }
      else
        commandSendErrorResponse(tokens[0], (char*)"Unknown command");
    }
  }
  else
  {
    commandSendErrorResponse(tokens[0], (char*)"Unknown command");
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

//Given a command, and a value, send response sentence with OK and checksum and <CR><LR>
//Ex: SPSET,ntripClientCasterUserPW,thePassword = $SPSET,ntripClientCasterUserPW,"thePassword",OK*2F
void commandSendStringOkResponse(char *command, char *settingName, char *valueBuffer)
{
  //Add escapes for any quotes in valueBuffer
  //https://stackoverflow.com/a/26114476
  const char *escapeChar = "\"";
  char escapedValueBuffer[100];
  size_t bp = 0;

  for (size_t sp = 0; valueBuffer[sp]; sp++) {
    if (strchr(escapeChar, valueBuffer[sp])) escapedValueBuffer[bp++] = '\\';
    escapedValueBuffer[bp++] = valueBuffer[sp];
  }
  escapedValueBuffer[bp] = 0;

  //Create string between $ and * for checksum calculation
  char innerBuffer[200];
  sprintf(innerBuffer, "%s,%s,\"%s\",OK", command, settingName, escapedValueBuffer);
  commandSendResponse(innerBuffer);
}

//Given a command, and a value, send response sentence with checksum and <CR><LR>
//Ex: $SPGET,ntripClientCasterUserPW*35 = $SPSET,ntripClientCasterUserPW,"thePassword",OK*2F
void commandSendStringResponse(char *command, char *settingName, char *valueBuffer)
{
  //Add escapes for any quotes in valueBuffer
  //https://stackoverflow.com/a/26114476
  const char *escapeChar = "\"";
  char escapedValueBuffer[100];
  size_t bp = 0;

  for (size_t sp = 0; valueBuffer[sp]; sp++) {
    if (strchr(escapeChar, valueBuffer[sp])) escapedValueBuffer[bp++] = '\\';
    escapedValueBuffer[bp++] = valueBuffer[sp];
  }
  escapedValueBuffer[bp] = 0;

  //Create string between $ and * for checksum calculation
  char innerBuffer[200];
  sprintf(innerBuffer, "%s,%s,\"%s\"", command, settingName, escapedValueBuffer);
  commandSendResponse(innerBuffer);
}

//Given a command, send structured ERROR response
//Ex: SPGET, 'Incorrect number of arguments' = "$SPGET,ERROR,Incorrect number of arguments*1E"
void commandSendErrorResponse(char *command, char *errorVerbose)
{
  //Create string between $ and * for checksum calculation
  char innerBuffer[200];
  snprintf(innerBuffer, sizeof(innerBuffer), "%s,ERROR,%s", command, errorVerbose);
  commandSendResponse(innerBuffer);
}

//Given an inner buffer, send response sentence with checksum and <CR><LR>
//Ex: "SPGET,0.25" = "$SPGET,0.25*33"
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
