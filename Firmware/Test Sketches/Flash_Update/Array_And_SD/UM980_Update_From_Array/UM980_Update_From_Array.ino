/*
  UM980 GNSS Bootloader for ESP32

  Streams a firmware .pkg file from an array to the UM980.
  This is a baby step towards OTA.
  Use 8MB SPIFFs as the partition. Set flash size to 8MB.

  Packet structure (1028 bytes total):
    [0]    0x02 (Header)
    [1]    Packet number
    [2]    Inverse of packet number (0xFF - packet number)
    [3..1026]  1024 data bytes (final packet zero-padded if needed)
    [1027] 8-bit checksum: (sum of all preceding bytes) - 1
*/

#define PLATFORM_REDBOARD
// #define PLATFORM_TORCH

#define COMPILE_ALL_FIRMWARE // Comment this out to test with a smaller firmware blob

#include "TheData.h" //Array containing the PKG data

uint64_t blobSpot = 0; // Advances as we read through the blob

// -----------------------------------------------------------------------------
// GNSS UART
// -----------------------------------------------------------------------------

#ifdef PLATFORM_REDBOARD
int pin_UART1_TX = 13; // Redboard testing
int pin_UART1_RX = 4;
int pin_GNSS_reset = 25; // Push low to reset GNSS/DR.
#elif defined(PLATFORM_TORCH)
int pin_UART1_TX = 27; // Torch testing
int pin_UART1_RX = 26;
int pin_GNSS_reset = 22; // Push low to reset GNSS/DR.
#endif
int gnssBaud = 115200;

HardwareSerial SerialForGnss(1); // Use UART1 on the ESP32

// -----------------------------------------------------------------------------
// Packet protocol constants
// -----------------------------------------------------------------------------
static const size_t PACKET_DATA_SIZE = 1024;
static const size_t PACKET_TOTAL_SIZE = 1 + 2 + PACKET_DATA_SIZE + 1; // = 1028
static const uint8_t HEADER_BYTE = 0x02;
static const uint16_t INITIAL_INDEX = 0x01FE;
static const uint16_t INDEX_INCREMENT = 0x00FF;

// -----------------------------------------------------------------------------
// Bootload timing
// -----------------------------------------------------------------------------
unsigned long bootloadStartTime = 0;
unsigned long bootloadEndTime = 0;

static void appendAndTrim(String &buf, char c, size_t maxLen = 4096)
{
  buf += c;
  if (buf.length() > maxLen)
    buf.remove(0, buf.length() - maxLen);
}

// Wait until the `needle` string appears in the GNSS receive stream, or timeout.
// Returns true if the needle string is found.
static bool waitForString(const char *needle, unsigned long timeoutMs, bool echoToSerial = false)
{
  String accum;
  unsigned long start = millis();
  while (millis() - start < timeoutMs)
  {
    while (SerialForGnss.available())
    {
      char c = SerialForGnss.read();
      if (echoToSerial)
        Serial.write(c);
      appendAndTrim(accum, c);
      if (accum.indexOf(needle) != -1)
        return true;
    }
    delay(1);
  }
  return false;
}

// Wait until a single byte value is received, or timeout.
static bool waitForByte(uint8_t target, unsigned long timeoutMs)
{
  unsigned long start = millis();
  while (millis() - start < timeoutMs)
  {
    if (SerialForGnss.available())
    {
      uint8_t b = (uint8_t)SerialForGnss.read();
      if (b == target)
        return true;
    }
  }
  return false;
}

// Empty the GNSS RX buffer.
static void flushGnssInput()
{
  delay(50);
  while (SerialForGnss.available())
    SerialForGnss.read();
}

// Read everything available from GNSS for `quietMs` after the last byte
// arrived (or until total `maxMs` elapsed). Returns the captured text.
static String drainGnssLine(unsigned long maxMs = 2000, unsigned long quietMs = 200)
{
  String out;
  unsigned long start = millis();
  unsigned long lastByte = millis();
  bool gotAny = false;

  while (millis() - start < maxMs)
  {
    while (SerialForGnss.available())
    {
      out += (char)SerialForGnss.read();
      lastByte = millis();
      gotAny = true;
    }

    if (gotAny && (millis() - lastByte > quietMs))
      break;

    delay(1);
  }

  return out;
}

// -----------------------------------------------------------------------------
// Bootloader steps
// -----------------------------------------------------------------------------

// Step 1: Verify communication with "version\r\n".
static bool verifyCommunication()
{
  Serial.println("\r\nVerifying communication with UM980 ('version' query)...");

  flushGnssInput();

  SerialForGnss.print("version\r\n");

  String resp = drainGnssLine(2000, 250);

  Serial.print("UM980 response: ");
  if (resp.length() == 0)
  {
    Serial.println("<no response>");
    return false;
  }
  else
  {
    Serial.println(resp);
    return true;
  }

  return false;
}

// Step 2: Send "T@" trigger every 20 ms until the bootloader menu prints
//         "efuse from uart". Starts the bootload timer when detected.
static bool triggerBootloader()
{
  Serial.println("Resetting UM980...");
  gnssResetPulse();

  Serial.println("Sending bootloader trigger 'T@T@T@T@T@T@T@T@' every 20 ms...");

  String accum;
  unsigned long lastTriggerSend = 0;

  while (true)
  {
    if (millis() - lastTriggerSend >= 20)
    {
      SerialForGnss.print("T@T@T@T@T@T@T@T@");
      lastTriggerSend = millis();
    }
    while (SerialForGnss.available())
    {
      char c = SerialForGnss.read();
      appendAndTrim(accum, c);
      if (accum.indexOf("efuse from uart") != -1)
      {
        bootloadStartTime = millis(); // start the bootload timer
        Serial.println("\r\nState: Bootloader menu reached "
                       "('efuse from uart' detected).");
        return true;
      }
    }
    delay(1);
  }
  return false;
}

// Step 3: Send "\r\n2\r\n" to enter bootload mode. Wait for "download to".
static bool enterBootloadMode()
{
  Serial.println("\r\nSending '2' to enter bootload mode...");
  SerialForGnss.print("\r\n2\r\n");
  if (!waitForString("download to", 10000))
  {
    Serial.println("Bootload mode not entered ('download to' not received)");
    return false;
  }
  Serial.println("State: Bootload mode reached ('download to' detected).");
  return true;
}

// Step 4: Wait for the 0x15 byte that signals "ready to receive".
static bool waitForReadyToReceive()
{
  Serial.println("\r\nWaiting for 0x15 (ready-to-receive)...");
  if (!waitForByte(0x15, 10000))
  {
    Serial.println("Did not receive 0x15 from UM980");
    return false;
  }
  Serial.println("State: Received 0x15 - ready to send data packets.");
  return true;
}

// Build one packet in `out`. Returns false when the file is exhausted and no
// data was read (i.e. nothing left to send).
static bool buildNextPacket(uint8_t *out, uint8_t packetIndex, uint32_t &bytesReadTotal, uint32_t fileSize)
{
  // XMODEM: out[1] = packet number (starts at 0x01), out[2] = inverse of packet number (0xFF - packet number)
  out[0] = HEADER_BYTE;
  out[1] = packetIndex;
  out[2] = (uint8_t)(0xFF - out[1]);

  // int n = firmwareFile.read(&out[3], PACKET_DATA_SIZE);

  int n = PACKET_DATA_SIZE;

  // If we are at the end of the array, reduce n appropriately
  if (blobSpot + n > sizeof(um980FirmwareBlob))
  {
    n = sizeof(um980FirmwareBlob) - blobSpot;
  }

  memcpy(&out[3], &um980FirmwareBlob[blobSpot], n);
  blobSpot += n; // Advance the counter because these bytes have been read out from the array

  if (n < 0)
  {
    Serial.println("Array read failed");
    return false;
  }

  if (n == 0)
    return false;

  bytesReadTotal += (uint32_t)n;

  // Check if this is a partial/last packet
  if ((n < PACKET_DATA_SIZE) && (bytesReadTotal == fileSize))
  {
    Serial.println("Sending last packet");

    // Add 0x1A (EOF) byte after the last data byte
    out[3 + n] = 0x1A;

    // Pad the remainder with 0x00 up to PACKET_DATA_SIZE
    if ((PACKET_DATA_SIZE - n - 1) > 0)
      memset(&out[3 + n + 1], 0, PACKET_DATA_SIZE - n - 1);
  }

  // Add 8-bit CRC: sum of all preceding bytes (including header and index), then subtract 1
  uint8_t sum = 0;
  for (size_t i = 0; i < PACKET_TOTAL_SIZE - 1; i++)
    sum += out[i];
  out[PACKET_TOTAL_SIZE - 1] = (uint8_t)(sum - 1);

  return true;
}

// Step 5: Stream the firmware in 544-byte data packets, waiting for 0x06
//         after each one.
static bool sendFirmware(uint32_t fileSize)
{
  Serial.println("\r\nStreaming firmware data packets...");

  uint8_t packet[PACKET_TOTAL_SIZE + 2];
  uint8_t packetIndex = 0x01; // XMODEM: packet number starts at 0x01
  uint32_t packetCount = 0;
  uint32_t totalBytesRead = 0;

  while (totalBytesRead < fileSize)
  {
    if (buildNextPacket(packet, packetIndex, totalBytesRead, fileSize) == false)
      return false; // Unexpected EOF before fileSize or other error

    SerialForGnss.write(packet, PACKET_TOTAL_SIZE);
    SerialForGnss.flush();

    if (!waitForByte(0x06, 5000))
    {
      Serial.print("ERROR: No ACK for packet #");
      Serial.println(packetCount);
      Serial.println("Bootload aborted (missing ACK)");
      return false;
    }

    packetCount++;
    packetIndex++; // XMODEM: increment by 1 each packet

    // Print progress
    if ((packetCount % 50) == 0)
    {
      float percent = (fileSize > 0) ? (100.0f * totalBytesRead / fileSize) : 0.0f;
      if (percent > 100.0f)
        percent = 100.0f;
      Serial.print("  Progress: ");
      Serial.print(percent, 1);
      Serial.println("% complete");
    }
  }

  Serial.print("All firmware packets sent. Total packets: ");
  Serial.println(packetCount);
  return true;
}

// Step 6: Send 0x04 (end-of-transfer) and wait through the long verification
//         phase for "backup succeed".
static bool finishTransferAndWaitBackup()
{
  Serial.println("\r\nSending 0x04 (end-of-transfer)...");
  SerialForGnss.write((uint8_t)0x04);
  SerialForGnss.flush();

  Serial.println("Waiting for 0x06 ack and 'backup succeed' "
                 "(this may take 15+ seconds)...");
  if (!waitForString("backup succeed", 90000, /*echoToSerial=*/true))
  {
    Serial.println("Did not receive 'backup succeed'");
    return false;
  }
  Serial.println("\r\nState: 'backup succeed' detected.");
  return true;
}

// Step 7: After backup succeeds the bootloader menu reprints. When we see
//         "efuse from uart" again, send "6\r\n6\r\n6\r\n" to exit.
static bool exitBootloader()
{
  Serial.println("\r\nWaiting for 'efuse from uart' (post-backup menu)...");
  if (waitForString("efuse from uart", 10000) == false)
  {
    Serial.println("Did not detect 'efuse from uart' after backup");
    // return false;
  }

  Serial.println("Sending '6\\r\\n6\\r\\n6\\r\\n' to exit bootloader...");
  SerialForGnss.print("6\r\n6\r\n6\r\n");

  return true;
}

// Step 8: Confirm the chip is rebooting.
static bool waitForReset()
{
  Serial.println("\r\nWaiting for 'resetting the cpu...'...");
  if (!waitForString("resetting the cpu", 10000))
  {
    Serial.println("Did not detect 'resetting the cpu'");
    return false;
  }
  Serial.println("State: 'resetting the cpu' detected.");
  return true;
}

// Step 9: Wait up to 5 seconds for the post-reset banner.
static bool waitForDeviceBanner()
{
  // At this point, the device resets to 115200 so we will need to switch baud rates.
  gnssBaud = 115200;

  Serial.printf("Setting ESP32 to %d baud...", gnssBaud);
  flushGnssInput();

  SerialForGnss.end();
  SerialForGnss.begin(gnssBaud, SERIAL_8N1, pin_UART1_RX, pin_UART1_TX); // Restart UART at higher baud rate
  delay(100);

  Serial.println("\r\nWaiting for '$devicename,COM' (up to 5 s)...");
  bool ok = waitForString("$devicename,COM", 5000);
  if (ok)
  {
    Serial.println("State: '$devicename,COM' detected. UM980 is back online.");
  }
  else
  {
    Serial.println("WARNING: '$devicename,COM' not seen within 5 seconds.");
  }
  return ok;
}

void gnssResetPulse()
{
  digitalWrite(pin_GNSS_reset, LOW);
  delay(500);
  digitalWrite(pin_GNSS_reset, HIGH);
  delay(100);
}

void setup()
{
  Serial.begin(115200);
  delay(100);

  Serial.println("\r\n UM980 Bootloader testing");

  // Bring up GNSS UART
  SerialForGnss.begin(gnssBaud, SERIAL_8N1, pin_UART1_RX, pin_UART1_TX);
  delay(100);

  pinMode(pin_GNSS_reset, OUTPUT);
  digitalWrite(pin_GNSS_reset, HIGH); // Keep out of reset

    displayMenu();
}

void displayMenu()
{
    Serial.println();
    Serial.println("Menu:");
    Serial.println("r) Reset");
    Serial.println("u) Update Firmware");
    Serial.print("Make selection: ");
}

void loop()
{
  if (Serial.available())
  {
    byte incoming = Serial.read();
    Serial.printf("%c\r\n", incoming);
    if (incoming == 'r')
    {
      ESP.restart();
    }
    else if (incoming == 'u')
    {
      runUpdate();
    }
    displayMenu();
  }
}

void runUpdate()
{
  Serial.println("Starting UM980 bootload sequence");

  gnssResetPulse(); // The UM980 may be stuck in bootloader limbo.

  // Wait for any serial activity at 115200 after reset
  // SerialForGnss.updateBaudRate(9600);
  //      SerialForGnss.updateBaudRate(19200);
  //      SerialForGnss.updateBaudRate(38400);
  // SerialForGnss.updateBaudRate(57600);
  SerialForGnss.updateBaudRate(115200);
  // SerialForGnss.updateBaudRate(115200 * 2); //230400
  //       SerialForGnss.updateBaudRate(115200 * 4); //460800
  flushGnssInput();

  //  const int baudRates[] = {115200, 230400, 460800, 921600, 9600, 19200, 38400, 57600};

  Serial.println("Waiting for any serial activity at 115200 after reset...");

  unsigned long startWait = millis();
  bool gotSerial = false;

  // Wait up to 3 seconds
  while (millis() - startWait < 3000)
  {
    if (SerialForGnss.available())
    {
      SerialForGnss.read();
      gotSerial = true;
      Serial.println("Serial activity detected after reset.");
      break;
    }
    delay(1);
  }
  if (gotSerial == false)
  {
    Serial.println("No serial activity detected after reset!");
    // return;
  }

  // Identify the interface rate the UM980 is using
  if (huntAndSetBaud() == false)
    return;

  // Switch both to a higher baud rate for faster firmware transfer.
  if (setBothToHighBaud() == false)
    return;

  //      uint32_t fileSize = firmwareFile.fileSize();
  uint32_t fileSize = sizeof(um980FirmwareBlob);
  Serial.print("Firmware file size: ");
  Serial.print(fileSize);
  Serial.println(" bytes");

  // 1. Sanity - check the link
  if (verifyCommunication() == false)
    return;

  // delay(2000); // Wait for GNSS to come out of reset

  // 2. Trigger the bootloader
  if (triggerBootloader() == false)
    return;

  // 3. Enter bootload mode
  if (enterBootloadMode() == false)
    return;

  // 4. Wait for the receiver to say "send me data"
  if (waitForReadyToReceive() == false)
    return;

  // 5. Stream the firmware
  if (sendFirmware(fileSize) == false)
  {
    // firmwareFile.close();
    return;
  }
  // firmwareFile.close();

  // 6. Tell the receiver we're done; wait for "backup succeed"
  if (finishTransferAndWaitBackup() == false)
    return;

  // 7. Exit the bootloader menu
  if (exitBootloader() == false)
    return;

  // 8. Confirm reboot
  if (waitForReset() == false)
    return;

  // 9. Wait for the device banner (ends the bootload timer)
  waitForDeviceBanner();

  bootloadEndTime = millis();
  unsigned long elapsedMs = bootloadEndTime - bootloadStartTime;
  Serial.println();
  Serial.print("=== Bootloading complete in ");
  Serial.print(elapsedMs);
  Serial.print(" ms (");
  Serial.print(elapsedMs / 1000.0, 2);
  Serial.println(" s) ===");
}

// Try the most common baud rates, send 'version', and return true if the UM980 responds. Sets gnssBaud to found rate.
static bool huntAndSetBaud()
{
  const int baudRates[] = {115200, 230400, 460800, 921600, 9600, 19200, 38400, 57600};
  const int numRates = sizeof(baudRates) / sizeof(baudRates[0]);
  for (int i = 0; i < numRates; ++i)
  {
    SerialForGnss.updateBaudRate(baudRates[i]);
    delay(100);
    flushGnssInput();
    SerialForGnss.print("version\r\n");
    String resp = drainGnssLine(1000, 200);
    if (resp.length() > 0 && resp.indexOf("UM980") != -1)
    {
      gnssBaud = baudRates[i];
      Serial.print("UM980 responded at baud: ");
      Serial.println(gnssBaud);
      return true;
    }
  }
  Serial.println("Could not detect UM980 at any baud rate!");
  return false;
}

// Change UM980 and ESP32 to 921600 baud
static bool setBothToHighBaud()
{
  gnssBaud = 921600;
  // gnssBaud = 921600 / 2;

  Serial.print("Setting UM980 and ESP32 to ");
  Serial.print(gnssBaud);
  Serial.println(" baud...");
  flushGnssInput();

#ifdef PLATFORM_REDBOARD
  SerialForGnss.printf("CONFIG COM2 %d\r\n", gnssBaud); // Redboard is connected to COM2 of UM980
#elif defined(PLATFORM_TORCH)
  SerialForGnss.printf("CONFIG COM3 %d\r\n", gnssBaud); // Torch is connected to COM3 of UM980
#endif

  delay(200); // Give the UM980 time to switch

  SerialForGnss.end();
  SerialForGnss.begin(gnssBaud, SERIAL_8N1, pin_UART1_RX, pin_UART1_TX); // Restart UART at higher baud rate
  delay(100);

  // Confirm with a 'version' query
  flushGnssInput();

  SerialForGnss.print("version\r\n");
  String resp = drainGnssLine(1000, 200);

  if (resp.length() > 0 && resp.indexOf("UM980") != -1)
  {
    Serial.println("Baud rate change confirmed.");
    return true;
  }

  Serial.println("Failed to confirm baud rate change!");

  return false;
}
