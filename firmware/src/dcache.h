/* Sets the ESP32-S3 data cache's line size at run time. Every PSRAM access goes
 * through that cache, and a miss costs about 0.92 us whatever its size (device
 * bench, `diag/psram_bench.h`): 64-byte lines halve the misses of a sequential
 * pass, which the display push and every fill or copy is, and cost more for a
 * lone short write (1.3 us a miss). The SDK boots with 32-byte lines. The ROM
 * calls are ones its header says an application should not make, so only
 * main.cpp's hackStarted() uses this, when a hack has just started. */
#pragma once

#include <Arduino.h>
#include <esp32s3/rom/cache.h>

/* 32 or 64. Writes the cache back and invalidates it, about a millisecond. */
static inline void dcache_set_line_size(uint32_t bytes) {
  if (Cache_Get_DCache_Line_Size() == bytes) return;
  noInterrupts();
  Cache_WriteBack_All();
  const uint32_t autoload = Cache_Suspend_DCache();
  if (bytes == 64)
    Cache_Set_DCache_Mode(DCACHE_SIZE_32KB, CACHE_4WAYS_ASSOC, CACHE_LINE_SIZE_64B);
  else
    Cache_Set_DCache_Mode(DCACHE_SIZE_32KB, CACHE_8WAYS_ASSOC, CACHE_LINE_SIZE_32B);
  Cache_Invalidate_DCache_All();
  Cache_Resume_DCache(autoload);
  interrupts();
  Serial.printf("dcache line=%u\n", (unsigned)Cache_Get_DCache_Line_Size());
}
