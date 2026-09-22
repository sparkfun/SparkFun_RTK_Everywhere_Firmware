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

#include <stdarg.h>

// #define PLATFORM_REDBOARD
#define PLATFORM_TORCH

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

// Torch has a pair of hardware muxes (U12/U18) between the ESP32 and the
// UM980/LoRa radio. See RTK_Everywhere/System.ino muxSelectUm980() and
// muxDisplayConfiguration() for the full truth table:
//   pin_muxA=LOW                  -> ESP32 UART1 <-> UM980 UART3 (COM3)
//   pin_muxA=HIGH                 -> ESP32 UART1 <-> LoRa UART1
//   pin_muxB=LOW                  -> ESP32 UART0 <-> CH340/USB
//   pin_muxB=HIGH, pin_muxA=LOW   -> ESP32 UART0 <-> LoRa UART2
//   pin_muxB=HIGH, pin_muxA=HIGH  -> ESP32 UART0 <-> UM980 UART1 (COM1)
// The last row is the only hardware path to UM980's UART1/COM1, and it goes
// through ESP32 UART0 - the very same peripheral behind our USB debug Serial
// port. There is no path from ESP32 UART1 to UM980 COM1. So talking to COM1
// means temporarily giving up the USB console; see selectUm980Com1() below.
int pin_muxA = 18;      // U12/U11 select
int pin_muxB = 12;      // U18 select
int pin_usbSelect = 21; // HIGH = CH340 stays connected to the USB bus
#endif
int gnssBaud = 115200;

HardwareSerial SerialForGnss(1); // Use UART1 on the ESP32 (reaches UM980 COM3)

// -----------------------------------------------------------------------------
// Debug console vs. GNSS link
// -----------------------------------------------------------------------------
// gnssPort points at whichever HardwareSerial is currently wired to the
// UM980. Normally that's SerialForGnss (ESP32 UART1 -> UM980 COM3). When we
// borrow ESP32 UART0 to reach UM980 COM1, gnssPort becomes &Serial - at that
// point Serial IS the GNSS link, so debug text can't go out over USB. All
// debug output goes through dbgPrint*() below, which prints live when the
// console is available and buffers into pendingLog otherwise; the buffer is
// flushed once USB is reconnected.
HardwareSerial *gnssPort = &SerialForGnss;
bool consoleAvailable = true;
String pendingLog;

static void dbgPrint(const String &s)
{
  if (consoleAvailable)
    Serial.print(s);
  else
    pendingLog += s;
}

static void dbgPrintln(const String &s = String())
{
  dbgPrint(s);
  dbgPrint("\r\n");
}

static void dbgPrintf(const char *fmt, ...)
{
  char buf[320];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  dbgPrint(buf);
}

static void flushPendingLog()
{
  if (pendingLog.length() > 0)
  {
    Serial.print(pendingLog);
    pendingLog = "";
  }
}

// Briefly reconnect USB, flush whatever has been buffered so far, then go
// back to the UM980 COM1 link. This gives real-time visibility into a
// long-running COM1 attempt (otherwise nothing reaches USB until the whole
// attempt ends, which can be many minutes and makes "still working" and
// "hung" indistinguishable from the outside). Only call this between
// packets/steps - i.e. right after we've consumed an expected ACK/response
// and before sending the next thing - never while a reply from the UM980
// could be in flight, since bytes arriving during the brief mux flip are
// simply lost (they go to neither USB nor the UM980 link).
#ifdef PLATFORM_TORCH
static void checkpointFlush()
{
  if (gnssPort != &Serial)
    return; // Not currently borrowing UART0 for COM1 - nothing to do.

  digitalWrite(pin_muxA, LOW);
  digitalWrite(pin_muxB, LOW);
  delay(2); // Let the mux/line settle before we trust Serial as USB again.

  Serial.println();
  Serial.println("[CHECKPOINT] Heartbeat flush (still on UM980 UART1/COM1)...");
  flushPendingLog();
  Serial.flush();

  digitalWrite(pin_muxA, HIGH);
  digitalWrite(pin_muxB, HIGH);
  delay(2);
}
#else
static void checkpointFlush() {}
#endif

// Human-readable label for whichever physical link gnssPort currently is.
static String gnssPortLabel()
{
  char buf[96];
  if (gnssPort == &SerialForGnss)
    snprintf(buf, sizeof(buf), "ESP32 UART1 (GPIO%d TX/GPIO%d RX) -> UM980 UART3 (COM3)",
             pin_UART1_TX, pin_UART1_RX);
  else
    snprintf(buf, sizeof(buf), "ESP32 UART0 (borrowed from USB) -> UM980 UART1 (COM1)");
  return String(buf);
}

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

// Print a String as hex bytes (capped) so non-printable / garbage responses
// (e.g. noise from a mis-routed mux) are still visible.
static void printHexDump(const char *label, const String &s, size_t maxBytes = 64)
{
  dbgPrintf("%s (%u bytes): ", label, (unsigned)s.length());
  size_t n = s.length();
  if (n > maxBytes)
    n = maxBytes;
  for (size_t i = 0; i < n; i++)
    dbgPrintf("%02X ", (uint8_t)s[i]);
  if (s.length() > maxBytes)
    dbgPrint("...");
  dbgPrintln();
}

// Wait until the `needle` string appears in the GNSS receive stream, or timeout.
// Returns true if the needle string is found. Prints progress every second and
// a full dump of what was received (if anything) on timeout, so a hang here is
// distinguishable from silence on the wire.
static bool waitForString(const char *needle, unsigned long timeoutMs, bool echoToSerial = false)
{
  String accum;
  uint32_t byteCount = 0;
  unsigned long start = millis();
  unsigned long lastProgressPrint = start;
  while (millis() - start < timeoutMs)
  {
    while (gnssPort->available())
    {
      char c = gnssPort->read();
      byteCount++;
      if (echoToSerial)
        dbgPrint(String(c));
      appendAndTrim(accum, c);
      if (accum.indexOf(needle) != -1)
      {
        dbgPrintf("[RX] Found \"%s\" after %lu ms (%lu bytes total)\r\n",
                  needle, millis() - start, (unsigned long)byteCount);
        return true;
      }
    }
    if (millis() - lastProgressPrint > 1000)
    {
      dbgPrintf("[RX] Still waiting for \"%s\"... %lu ms elapsed, %lu bytes seen so far\r\n",
                needle, millis() - start, (unsigned long)byteCount);
      lastProgressPrint = millis();
    }
    delay(1);
  }
  dbgPrintf("[RX] TIMEOUT waiting for \"%s\" after %lu ms. %lu bytes were received:\r\n",
            needle, timeoutMs, (unsigned long)byteCount);
  if (accum.length() > 0)
  {
    dbgPrint("  Last data seen: ");
    dbgPrintln(accum);
    printHexDump("  Hex", accum);
  }
  else
  {
    dbgPrintf("  <nothing at all received on %s - check mux/wiring/power>\r\n", gnssPortLabel().c_str());
  }
  return false;
}

// Wait until a single byte value is received, or timeout. Any other bytes
// seen while waiting are logged (in hex) since they can hint at what state
// the UM980 is actually in.
// `quiet` suppresses the per-success log line (used in the per-packet ACK
// loop, which would otherwise add ~1 line per packet - thousands of lines
// for a full firmware transfer - to the buffered log while UART0 is
// borrowed for UM980 COM1). Unexpected bytes and timeouts are always logged.
static bool waitForByte(uint8_t target, unsigned long timeoutMs, bool quiet = false)
{
  unsigned long start = millis();
  uint32_t otherByteCount = 0;
  while (millis() - start < timeoutMs)
  {
    if (gnssPort->available())
    {
      uint8_t b = (uint8_t)gnssPort->read();
      if (b == target)
      {
        if (!quiet)
          dbgPrintf("[RX] Got expected byte 0x%02X after %lu ms\r\n", target, millis() - start);
        return true;
      }
      else
      {
        otherByteCount++;
        dbgPrintf("[RX] Unexpected byte while waiting for 0x%02X: 0x%02X ('%c')\r\n",
                  target, b, (b >= 0x20 && b < 0x7F) ? (char)b : '.');
      }
    }
  }
  dbgPrintf("[RX] TIMEOUT waiting for 0x%02X after %lu ms (%lu other bytes seen)\r\n",
            target, timeoutMs, (unsigned long)otherByteCount);
  return false;
}

// Empty the GNSS RX buffer.
static void flushGnssInput()
{
  delay(50);
  while (gnssPort->available())
    gnssPort->read();
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
    while (gnssPort->available())
    {
      out += (char)gnssPort->read();
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

// Step 1: Send "T@" trigger every 20 ms until the bootloader menu prints
//         "efuse from uart". Starts the bootload timer when detected.
// Bounded by a timeout so a mis-routed mux / dead UART shows an error
// instead of hanging forever.
static bool triggerBootloader()
{
  dbgPrintln("Resetting UM980...");
  gnssResetPulse();

  dbgPrintf("Sending bootloader trigger 'T@T@T@T@T@T@T@T@' every 20 ms on %s...\r\n",
            gnssPortLabel().c_str());

  const unsigned long triggerTimeoutMs = 15000;
  String accum;
  unsigned long start = millis();
  unsigned long lastTriggerSend = 0;
  unsigned long lastProgressPrint = start;
  uint32_t byteCount = 0;
  uint32_t triggerSendCount = 0;

  while (millis() - start < triggerTimeoutMs)
  {
    if (millis() - lastTriggerSend >= 20)
    {
      gnssPort->print("T@T@T@T@T@T@T@T@");
      lastTriggerSend = millis();
      triggerSendCount++;
    }
    while (gnssPort->available())
    {
      char c = gnssPort->read();
      byteCount++;
      appendAndTrim(accum, c);
      if (accum.indexOf("efuse from uart") != -1)
      {
        bootloadStartTime = millis(); // start the bootload timer
        dbgPrintf("\r\nState: Bootloader menu reached ('efuse from uart' detected) after "
                  "%lu ms, %lu trigger bursts sent, %lu bytes received.\r\n",
                  millis() - start, (unsigned long)triggerSendCount, (unsigned long)byteCount);
        checkpointFlush();
        return true;
      }
    }
    if (millis() - lastProgressPrint > 1000)
    {
      dbgPrintf("[TX/RX] Still triggering bootloader... %lu ms elapsed, %lu bursts sent, "
                "%lu bytes received\r\n",
                millis() - start, (unsigned long)triggerSendCount, (unsigned long)byteCount);
      lastProgressPrint = millis();
    }
    delay(1);
  }

  dbgPrintf("TIMEOUT: 'efuse from uart' not seen after %lu ms (%lu bytes received total).\r\n",
            triggerTimeoutMs, (unsigned long)byteCount);
  if (accum.length() > 0)
  {
    dbgPrint("  Last data seen: ");
    dbgPrintln(accum);
    printHexDump("  Hex", accum);
  }
  else
  {
    dbgPrintf("  <nothing received on %s - check mux/wiring/power>\r\n", gnssPortLabel().c_str());
  }
  return false;
}

// Step 2: Send "\r\n2\r\n" to enter bootload mode. Wait for "download to".
static bool enterBootloadMode()
{
  dbgPrintln("\r\nSending '2' to enter bootload mode...");
  gnssPort->print("\r\n2\r\n");
  if (!waitForString("download to", 10000))
  {
    dbgPrintln("Bootload mode not entered ('download to' not received)");
    return false;
  }
  dbgPrintln("State: Bootload mode reached ('download to' detected).");
  checkpointFlush();
  return true;
}

// Step 3: Wait for the 0x15 byte that signals "ready to receive".
static bool waitForReadyToReceive()
{
  dbgPrintln("\r\nWaiting for 0x15 (ready-to-receive)...");
  if (!waitForByte(0x15, 10000))
  {
    dbgPrintln("Did not receive 0x15 from UM980");
    return false;
  }
  dbgPrintln("State: Received 0x15 - ready to send data packets.");
  checkpointFlush();
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
    dbgPrintln("Array read failed");
    return false;
  }

  if (n == 0)
    return false;

  bytesReadTotal += (uint32_t)n;

  // Check if this is a partial/last packet
  if ((n < PACKET_DATA_SIZE) && (bytesReadTotal == fileSize))
  {
    dbgPrintln("Sending last packet");

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

// Step 4: Stream the firmware in 544-byte data packets, waiting for 0x06
//         after each one.
static bool sendFirmware(uint32_t fileSize)
{
  dbgPrintln("\r\nStreaming firmware data packets...");

  uint8_t packet[PACKET_TOTAL_SIZE + 2];
  uint8_t packetIndex = 0x01; // XMODEM: packet number starts at 0x01
  uint32_t packetCount = 0;
  uint32_t totalBytesRead = 0;

  while (totalBytesRead < fileSize)
  {
    if (buildNextPacket(packet, packetIndex, totalBytesRead, fileSize) == false)
      return false; // Unexpected EOF before fileSize or other error

    gnssPort->write(packet, PACKET_TOTAL_SIZE);
    gnssPort->flush();

    // 30s: observed ACK latency spikes into the multi-second range (likely
    // a flash-sector erase on the UM980 side), well past a 5s timeout.
    if (!waitForByte(0x06, 30000, /*quiet=*/true))
    {
      dbgPrintf("ERROR: No ACK for packet #%lu\r\n", (unsigned long)packetCount);
      dbgPrintln("Bootload aborted (missing ACK)");
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
      dbgPrintf("  Progress: %.1f%% complete (packet #%lu)\r\n", percent, (unsigned long)packetCount);
    }

    // Safe here: we just consumed this packet's ACK and haven't sent the
    // next packet yet, so the UM980 is idle waiting on us - nothing of ours
    // is in flight to lose during the brief mux flip.
    if ((packetCount % 200) == 0)
      checkpointFlush();
  }

  dbgPrintf("All firmware packets sent. Total packets: %lu\r\n", (unsigned long)packetCount);
  checkpointFlush();
  return true;
}

// Step 5: Send 0x04 (end-of-transfer) and wait through the long verification
//         phase for "backup succeed".
static bool finishTransferAndWaitBackup()
{
  dbgPrintln("\r\nSending 0x04 (end-of-transfer)...");
  gnssPort->write((uint8_t)0x04);
  gnssPort->flush();

  dbgPrintln("Waiting for 0x06 ack and 'backup succeed' (this may take 15+ seconds)...");
  if (!waitForString("backup succeed", 90000, /*echoToSerial=*/true))
  {
    dbgPrintln("Did not receive 'backup succeed'");
    return false;
  }
  dbgPrintln("\r\nState: 'backup succeed' detected.");
  checkpointFlush();
  return true;
}

// Step 6: After backup succeeds the bootloader menu reprints. When we see
//         "efuse from uart" again, send "6\r\n6\r\n6\r\n" to exit.
static bool exitBootloader()
{
  dbgPrintln("\r\nWaiting for 'efuse from uart' (post-backup menu)...");
  if (waitForString("efuse from uart", 10000) == false)
  {
    dbgPrintln("Did not detect 'efuse from uart' after backup");
    // return false;
  }

  dbgPrintln("Sending '6\\r\\n6\\r\\n6\\r\\n' to exit bootloader...");
  gnssPort->print("6\r\n6\r\n6\r\n");

  return true;
}

// Step 7: Confirm the chip is rebooting.
static bool waitForReset()
{
  dbgPrintln("\r\nWaiting for 'resetting the cpu...'...");
  if (!waitForString("resetting the cpu", 10000))
  {
    dbgPrintln("Did not detect 'resetting the cpu'");
    return false;
  }
  dbgPrintln("State: 'resetting the cpu' detected.");
  return true;
}

// Restart gnssPort at `baud`, using the right begin() call depending on which
// physical UART is currently active. UART0 (Serial) must NOT be given
// explicit pins - it's already wired to the UM980 via the mux on its default
// pins, and passing pin_UART1_RX/TX would try to remap it onto the wrong GPIOs.
static void restartGnssPortAtBaud(uint32_t baud)
{
  gnssPort->end();
  delay(10);
  if (gnssPort == &SerialForGnss)
    SerialForGnss.begin(baud, SERIAL_8N1, pin_UART1_RX, pin_UART1_TX);
  else
    Serial.begin(baud);
  delay(100);
}

// Step 8: Wait up to 5 seconds for the post-reset banner.
static bool waitForDeviceBanner()
{
  // At this point, the device resets to 115200 so we will need to switch baud rates.
  gnssBaud = 115200;

  dbgPrintf("Setting %s to %d baud...\r\n", gnssPortLabel().c_str(), gnssBaud);
  flushGnssInput();

  restartGnssPortAtBaud(gnssBaud);

  dbgPrintln("\r\nWaiting for '$devicename,COM' (up to 5 s)...");
  bool ok = waitForString("$devicename,COM", 5000);
  if (ok)
  {
    dbgPrintln("State: '$devicename,COM' detected. UM980 is back online.");
  }
  else
  {
    dbgPrintln("WARNING: '$devicename,COM' not seen within 5 seconds.");
  }
  return ok;
}

void gnssResetPulse()
{
  dbgPrintln("[MUX] Pulling GNSS reset pin low for 500 ms...");
  digitalWrite(pin_GNSS_reset, LOW);
  delay(500);
  digitalWrite(pin_GNSS_reset, HIGH);
  dbgPrintln("[MUX] GNSS reset pin released (high).");
}

#ifdef PLATFORM_TORCH
// Route ESP32 UART1 <-> UM980 UART3 (COM3), and ESP32 UART0 <-> CH340/USB so
// our debug Serial output over USB works normally. This is the "legacy" path
// - COM3 accepts the UM980's normal command set but has NOT been observed to
// respond to the T@ bootloader trigger.
static void selectUm980Com3()
{
  Serial.println("[MUX] Configuring Torch hardware muxes (U12/U18) for UM980 COM3...");

  pinMode(pin_muxA, OUTPUT);
  pinMode(pin_muxB, OUTPUT);
  pinMode(pin_usbSelect, OUTPUT);

  digitalWrite(pin_muxA, LOW); // U12: ESP32 UART1 <-> UM980 UART3
  digitalWrite(pin_muxB, LOW); // U18: ESP32 UART0 <-> CH340 <-> USB serial
  digitalWrite(pin_usbSelect, HIGH); // Keep CH340 connected to the USB bus

  gnssPort = &SerialForGnss;
  consoleAvailable = true;

  Serial.printf("[MUX]   pin_muxA (GPIO%d) = LOW  -> ESP32 UART1 routed to UM980 UART3 (COM3)\r\n", pin_muxA);
  Serial.printf("[MUX]   pin_muxB (GPIO%d) = LOW  -> ESP32 UART0 routed to CH340/USB (debug Serial)\r\n", pin_muxB);
  Serial.printf("[MUX]   pin_usbSelect (GPIO%d) = HIGH -> CH340 stays connected to USB bus\r\n", pin_usbSelect);
}

// Route ESP32 UART0 <-> UM980 UART1 (COM1). This necessarily disconnects the
// USB console (CH340) from ESP32 UART0, since they share the same mux input.
// From this point on Serial IS the GNSS link: further debug text is buffered
// (see dbgPrint*()/pendingLog above) instead of printed, until
// restoreUsbConsole() reconnects USB and flushes it.
static void selectUm980Com1()
{
  Serial.println("[MUX] Switching ESP32 UART0 from USB to UM980 UART1 (COM1).");
  Serial.println("[MUX] Console output will be buffered and printed after this attempt completes.");
  Serial.flush();

  pinMode(pin_muxA, OUTPUT);
  pinMode(pin_muxB, OUTPUT);

  digitalWrite(pin_muxA, HIGH); // U12 -> LoRa UART1 (UART1/COM3 path disconnected); U11 branch -> UM980 UART1
  digitalWrite(pin_muxB, HIGH); // U18 -> U11 -> UM980 UART1 (COM1), instead of CH340/USB

  gnssPort = &Serial;
  consoleAvailable = false;
}

// Switch ESP32 UART0 back to the USB/CH340 path and flush anything that was
// buffered while UART0 was borrowed for the UM980 COM1 attempt.
static void restoreUsbConsole()
{
  bool wasBuffering = !consoleAvailable;

  digitalWrite(pin_muxA, LOW);
  digitalWrite(pin_muxB, LOW);
  gnssPort = &SerialForGnss;
  consoleAvailable = true;

  if (wasBuffering)
  {
    Serial.println("[MUX] ESP32 UART0 reconnected to USB.");
    Serial.println("----- Buffered log from the UM980 UART1 (COM1) attempt -----");
    flushPendingLog();
    Serial.println("----- End of buffered log -----");
  }
}
#endif

void setup()
{
  Serial.begin(115200);
  delay(100);

  Serial.println("\r\n UM980 Bootloader testing");

#ifdef PLATFORM_TORCH
  selectUm980Com3();
#endif

  // Bring up GNSS UART
  Serial.printf("[COM] Bringing up ESP32 UART1 (GPIO%d TX / GPIO%d RX) at %d baud for UM980 communication...\r\n",
                pin_UART1_TX, pin_UART1_RX, gnssBaud);
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
  Serial.println("u) Update Firmware (via UM980 UART1 / COM1)");
  Serial.println("l) Update Firmware (via UM980 UART3 / COM3 - legacy, live console)");
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
#ifdef PLATFORM_TORCH
      runUpdateViaCom1();
#else
      runUpdateViaCom3();
#endif
    }
    else if (incoming == 'l')
    {
      runUpdateViaCom3();
    }
    displayMenu();
  }
}

// Shared bootload sequence. Assumes gnssPort/consoleAvailable have already
// been pointed at the link to attempt (see selectUm980Com1/3() above).
static bool runUpdateCore(uint32_t fileSize)
{
  gnssResetPulse(); // The UM980 may be stuck in bootloader limbo.

  dbgPrintf("[COM] %s -> updating to 115200 baud\r\n", gnssPortLabel().c_str());
  gnssPort->updateBaudRate(115200);
  flushGnssInput();

  dbgPrintln("Waiting for any serial activity at 115200 after reset...");

  unsigned long startWait = millis();
  bool gotSerial = false;
  uint32_t byteCount = 0;

  // Wait up to 3 seconds, and print every byte we see (helps distinguish
  // real UM980 boot chatter from line noise / a mis-routed mux).
  while (millis() - startWait < 3000)
  {
    if (gnssPort->available())
    {
      uint8_t b = (uint8_t)gnssPort->read();
      byteCount++;
      if (!gotSerial)
      {
        dbgPrintf("Serial activity detected after reset. First byte: 0x%02X ('%c')\r\n",
                  b, (b >= 0x20 && b < 0x7F) ? (char)b : '.');
      }
      gotSerial = true;
    }
    delay(1);
  }
  if (gotSerial == false)
  {
    dbgPrintf("No serial activity detected after reset on %s! This may be normal for a quiet "
              "config port, or may mean the mux is mis-routed / GNSS is unpowered.\r\n",
              gnssPortLabel().c_str());
  }
  else
  {
    dbgPrintf("Total bytes seen during reset-activity window: %lu (liveness check only - does NOT "
              "confirm the UM980 will respond to commands)\r\n",
              (unsigned long)byteCount);
  }

  // NOTE: We intentionally do NOT probe baud rates or send 'version' here
  // and gate on a response. If the UM980's application firmware is
  // unresponsive/ignoring commands, waiting around for a command-mode reply
  // will never succeed, and every attempt burns time without ever reaching
  // the one thing that actually works: resetting the module and catching
  // the bootloader's trigger window in the first tens of ms after reset.
  // So we go straight for that, with a few retries since the exact timing
  // of that window can be a little flaky.
  dbgPrintf("Firmware file size: %lu bytes\r\n", (unsigned long)fileSize);

  const int maxTriggerAttempts = 3;
  bool triggered = false;
  for (int attempt = 1; attempt <= maxTriggerAttempts && !triggered; attempt++)
  {
    dbgPrintf("\r\n[TRIGGER] Attempt %d of %d: resetting UM980 and racing for the "
              "bootloader window...\r\n",
              attempt, maxTriggerAttempts);
    triggered = triggerBootloader();
    if (!triggered && attempt < maxTriggerAttempts)
      dbgPrintln("[TRIGGER] Missed the window (or UM980 isn't answering) - retrying...");
  }
  if (!triggered)
  {
    dbgPrintln("[TRIGGER] Could not enter the bootloader after all retries.");
    return false;
  }

  // 2. Enter bootload mode
  if (enterBootloadMode() == false)
    return false;

  // 3. Wait for the receiver to say "send me data"
  if (waitForReadyToReceive() == false)
    return false;

  // 4. Stream the firmware
  if (sendFirmware(fileSize) == false)
    return false;

  // 5. Tell the receiver we're done; wait for "backup succeed"
  if (finishTransferAndWaitBackup() == false)
    return false;

  // 6. Exit the bootloader menu
  if (exitBootloader() == false)
    return false;

  // 7. Confirm reboot
  if (waitForReset() == false)
    return false;

  // 8. Wait for the device banner (ends the bootload timer)
  waitForDeviceBanner();

  bootloadEndTime = millis();
  unsigned long elapsedMs = bootloadEndTime - bootloadStartTime;
  dbgPrintln();
  dbgPrintf("=== Bootloading complete in %lu ms (%.2f s) ===\r\n", elapsedMs, elapsedMs / 1000.0);
  return true;
}

// Attempt bootload over UM980's UART1/COM1. This is the recommended path:
// COM3 has been observed to accept normal NMEA/command traffic but NOT the
// T@ bootloader trigger, so we try UM980's other UART instead. Reaching it
// requires borrowing ESP32 UART0 from the USB console (see selectUm980Com1()),
// so all diagnostic output is buffered and printed after the attempt ends.
void runUpdateViaCom1()
{
  Serial.println("Starting UM980 bootload sequence via UM980 UART1 (COM1)");
  uint32_t fileSize = sizeof(um980FirmwareBlob);

#ifdef PLATFORM_TORCH
  selectUm980Com1();
  runUpdateCore(fileSize);
  restoreUsbConsole();
#else
  Serial.println("COM1 path requires the Torch mux (pin_muxA/pin_muxB) - not available on this platform.");
#endif
}

// Legacy path: attempt bootload over UM980's UART3/COM3. Kept for comparison
// / regression testing. USB console stays live throughout since this uses
// ESP32 UART1, which never shares a mux input with the USB console.
void runUpdateViaCom3()
{
  Serial.println("Starting UM980 bootload sequence via UM980 UART3 (COM3, legacy path)");
  uint32_t fileSize = sizeof(um980FirmwareBlob);

#ifdef PLATFORM_TORCH
  selectUm980Com3();
#endif
  runUpdateCore(fileSize);
}
