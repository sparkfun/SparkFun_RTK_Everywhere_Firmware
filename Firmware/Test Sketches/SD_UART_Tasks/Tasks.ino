int bufferOverruns = 0;

//Read bytes from UART2 into circular buffer
void F9PSerialReadTask(void *e)
{
  bool newDataLost = false;

  Serial.printf("F9PSerialReadTask started\r\n");
  while (online.logging)
  {
    //Determine the amount of microSD card logging data in the buffer
    if (serialGNSS.available())
    {
      int usedBytes = dataHead - sdTail; //Distance between head and tail
      if (usedBytes < 0)
        usedBytes += gnssHandlerBufferSize;

      int availableHandlerSpace = gnssHandlerBufferSize - usedBytes;

      //Don't fill the last byte to prevent buffer overflow
      if (availableHandlerSpace)
        availableHandlerSpace -= 1;

      //Check for buffer overruns
      //int availableUARTSpace = settings.uartReceiveBufferSize - serialGNSS.available();
      int availableUARTSpace = uartReceiveBufferSize - serialGNSS.available();
      int combinedSpaceRemaining = availableHandlerSpace + availableUARTSpace;

      //log_d("availableHandlerSpace = %d availableUARTSpace = %d bufferOverruns: %d", availableHandlerSpace, availableUARTSpace, bufferOverruns);

      if (combinedSpaceRemaining == 0)
      {
        if (newDataLost == false)
        {
          newDataLost = true;
          bufferOverruns++;
          //if (settings.enablePrintBufferOverrun)
          //Serial.sprintf("Data loss likely! availableHandlerSpace = %d availableUARTSpace = %d bufferOverruns: %d\n\r", availableHandlerSpace, availableUARTSpace, bufferOverruns);
          log_d("Data loss likely! availableHandlerSpace = %d availableUARTSpace = %d bufferOverruns: %d", availableHandlerSpace, availableUARTSpace, bufferOverruns);
        }
      }
      //else if (combinedSpaceRemaining < ( (gnssHandlerBufferSize + settings.uartReceiveBufferSize) / 16))
      else if (combinedSpaceRemaining < ( (gnssHandlerBufferSize + uartReceiveBufferSize) / 16))
      {
        //if (settings.enablePrintBufferOverrun)
        log_d("Low space: availableHandlerSpace = %d availableUARTSpace = %d bufferOverruns: %d", availableHandlerSpace, availableUARTSpace, bufferOverruns);
      }
      else
        newDataLost = false; //Reset

      //While there is free buffer space and UART2 has at least one RX byte
      while (availableHandlerSpace && serialGNSS.available())
      {
        //Fill the buffer to the end and then start at the beginning
        int availableSlice = availableHandlerSpace;
        if ((dataHead + availableHandlerSpace) >= gnssHandlerBufferSize)
          availableSlice = gnssHandlerBufferSize - dataHead;

        //Add new data into circular buffer in front of the head
        //availableHandlerSpace is already reduced to avoid buffer overflow
        int newBytesToRecord = serialGNSS.read(&rBuffer[dataHead], availableSlice);
        Serial.printf("UART --> Buffer: %d bytes\r\n", newBytesToRecord);

        //Check for negative (error)
        if (newBytesToRecord <= 0)
          break;

        //Account for the bytes read
        availableHandlerSpace -= newBytesToRecord;

        dataHead += newBytesToRecord;
        if (dataHead >= gnssHandlerBufferSize)
          dataHead -= gnssHandlerBufferSize;

        delay(1);
        taskYIELD();
      } //End Serial.available()
    }

    delay(1);
    taskYIELD();
  }
  Serial.printf("F9PSerialReadTask exiting\r\n");
  TaskHandle_t taskHandle = F9PSerialReadTaskHandle;
  F9PSerialReadTaskHandle = NULL;
  vTaskDelete(taskHandle);
}

//Log data to the SD card
void SDWriteTask(void *e)
{
  Serial.printf("SDWriteTask started\r\n");
  while (online.logging || (dataHead - sdTail))
  {
    int sdBytesToRecord = dataHead - sdTail; //Amount of buffered microSD card logging data
    if (sdBytesToRecord < 0)
      sdBytesToRecord += gnssHandlerBufferSize;

    if (sdBytesToRecord > 0)
    {
      //Reduce bytes to record to SD if we have more to send then the end of the buffer
      //We'll wrap next loop
      if ((sdTail + sdBytesToRecord) > gnssHandlerBufferSize)
        sdBytesToRecord = gnssHandlerBufferSize - sdTail;

      long startTime = millis();
      int recordedBytes = ubxFile.write(&rBuffer[sdTail], sdBytesToRecord);
      long endTime = millis();
      Serial.printf("Buffer --> File: %d bytes\r\n", recordedBytes);

      if (endTime - startTime > 150) log_d("Long Write! Delta time: %d / Recorded %d bytes", endTime - startTime, recordedBytes);

#ifdef USE_SDFAT
      fileSize = ubxFile.fileSize(); //Get updated filed size
#else
      fileSize = 0;
#endif
      if (online.psram)
        reportHeapNow();

      //Account for the sent data or dropped
      if (recordedBytes > 0)
      {
        sdTail += recordedBytes;
        if (sdTail >= gnssHandlerBufferSize)
          sdTail -= gnssHandlerBufferSize;
      }

      //Force file sync every 60s
      if (millis() - lastUBXLogSyncTime > 60000)
      {
#ifdef USE_SDFAT
        ubxFile.sync();
        if (online.psram)
          reportHeapNow();
#endif
        lastUBXLogSyncTime = millis();
      } //End sdCardSemaphore
    }

    delay(1);
    taskYIELD();
  } //End logging
  Serial.printf("SDWriteTask exiting\r\n");
  TaskHandle_t taskHandle = SDWriteTaskHandle;
  SDWriteTaskHandle = NULL;
  vTaskDelete(taskHandle);
}
