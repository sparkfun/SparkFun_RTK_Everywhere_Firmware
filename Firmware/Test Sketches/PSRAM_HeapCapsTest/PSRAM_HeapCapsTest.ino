
#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

unsigned long lastHeapReport;

void setup() {
  Serial.begin(115200);
  delay(250);

  if (psramInit() == false)
    Serial.println("PSRAM failed to initialize");
  else
    Serial.println("PSRAM initialized");

  Serial.printf("PSRAM Size available (bytes): %d\r\n", ESP.getFreePsram());

  heap_caps_malloc_extmem_enable(1000); //Use PSRAM for memory requests larger than 1,000 bytes

  //Limit 10000: FreeHeap: 146348 / HeapLowestPoint: 2219887 / LargestBlock: 2064372 / Used PSRAM: 260
  //Limit 1000: FreeHeap: 172364 / HeapLowestPoint: 2219883 / LargestBlock: 2064372 / Used PSRAM: 26080
  //Limit 100: FreeHeap: 183052 / HeapLowestPoint: 2219155 / LargestBlock: 2031604 / Used PSRAM: 36196
  //No defined limit: FreeHeap: 155252 / HeapLowestPoint: 2219883 / LargestBlock: 2064372 / Used PSRAM: 9128

  Serial.print("Heap before Bluetooth: ");
  reportHeap();

  SerialBT.begin("HeapTest");

  Serial.print("Heap after Bluetooth: ");
  reportHeap();
}

void loop() {
  if (Serial.available()) {
    SerialBT.write(Serial.read());
  }
  if (SerialBT.available()) {
    Serial.write(SerialBT.read());
  }
  delay(20);

  if (millis() - lastHeapReport > 1000)
  {
    lastHeapReport = millis();
    reportHeap();
  }

  if(Serial.available()) ESP.restart();
}

void reportHeap()
{
  Serial.printf("FreeHeap: %d / HeapLowestPoint: %d / LargestBlock: %d / Used PSRAM: %d\r\n", ESP.getFreeHeap(),
                xPortGetMinimumEverFreeHeapSize(), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT), ESP.getPsramSize() - ESP.getFreePsram());
}
