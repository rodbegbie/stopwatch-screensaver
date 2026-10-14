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
#include <esp_ipc.h>

/* The cache is shared by both cores and noInterrupts() stops only this one, so
 * the other core is parked first: in IRAM, with interrupts off, touching only
 * internal RAM, until the cache is back (the way the SDK's flash writes do). */
static volatile bool g_dcache_parked;
static volatile bool g_dcache_release;

static void IRAM_ATTR dcache_park_core(void *) {
  noInterrupts();
  g_dcache_parked = true;
  while (!g_dcache_release) {
  }
  g_dcache_parked = false;
  interrupts();
}

/* Runs `change` with the other core parked. Returns false, doing nothing, if
 * the other core could not be parked. */
template <typename F>
static bool dcache_with_other_core_parked(F change) {
  g_dcache_parked = false;
  g_dcache_release = false;
  if (esp_ipc_call(1 - xPortGetCoreID(), dcache_park_core, nullptr) != ESP_OK) return false;
  uint32_t waited = 0;
  while (!g_dcache_parked) {
    if (++waited > 2000) {
      g_dcache_release = true;
      return false;
    }
    delay(1);
  }
  noInterrupts();
  change();
  interrupts();
  g_dcache_release = true;
  while (g_dcache_parked) {
  }
  return true;
}

/* 32 or 64. Writes the cache back and invalidates it, about a millisecond. */
static inline void dcache_set_line_size(uint32_t bytes) {
  if (Cache_Get_DCache_Line_Size() == bytes) return;
  const bool done = dcache_with_other_core_parked([bytes] {
    Cache_WriteBack_All();
    const uint32_t autoload = Cache_Suspend_DCache();
    if (bytes == 64)
      Cache_Set_DCache_Mode(DCACHE_SIZE_32KB, CACHE_4WAYS_ASSOC, CACHE_LINE_SIZE_64B);
    else
      Cache_Set_DCache_Mode(DCACHE_SIZE_32KB, CACHE_8WAYS_ASSOC, CACHE_LINE_SIZE_32B);
    Cache_Invalidate_DCache_All();
    Cache_Resume_DCache(autoload);
  });
  Serial.printf("dcache line=%u%s\n", (unsigned)Cache_Get_DCache_Line_Size(),
                done ? "" : " (other core not parked, unchanged)");
}
