/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
JSON_Helpers.h

  ArduinoJson support - PSRAM-backed allocator for JsonDocument
=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-*/

#ifndef __JSON_HELPERS_H__
#define __JSON_HELPERS_H__

// Defined in System.ino
void *rtkMalloc(size_t sizeInBytes, const char *text);
void rtkFree(void *data, const char *text);

// Routes a JsonDocument's internal memory-pool allocations through rtkMalloc()/rtkFree()
// (PSRAM when available) instead of ArduinoJson's default allocator, which calls
// malloc()/realloc()/free() directly and is therefore subject to the same "small
// allocations stay internal" size threshold (psramMallocLevel) as every other unmanaged
// malloc() call. Pass &jsonPsramAllocator to any JsonDocument that may hold more than a
// trivial amount of data (ZTP responses, settings CSV lists, etc): JsonDocument doc(&jsonPsramAllocator);
struct PsramJsonAllocator : ArduinoJson::Allocator
{
    void *allocate(size_t size) override
    {
        return rtkMalloc(size, "ArduinoJson");
    }
    void deallocate(void *pointer) override
    {
        rtkFree(pointer, "ArduinoJson");
    }
    void *reallocate(void *ptr, size_t new_size) override
    {
        if (online.psram)
        {
            void *newPtr = heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (newPtr != nullptr || new_size == 0)
                return newPtr;
            // PSRAM exhausted - fall through to internal RAM rather than fail outright
        }
        return realloc(ptr, new_size);
    }
};
static PsramJsonAllocator jsonPsramAllocator;

#endif // __JSON_HELPERS_H__
