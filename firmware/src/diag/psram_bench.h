/* Build with -DPSRAM_BENCH to run a PSRAM micro-benchmark at boot instead of
 * the hacks, and log it on the serial port (`bench ...` lines). It measures
 * what the canvas's scattered span writes cost: sequential and strided writes,
 * random spans of each length, random spans confined to a working set (to find
 * the data cache's size), and the same on internal RAM for comparison.
 *
 * -DPSRAM_BENCH_VARIANT=N changes the data cache first:
 *   0 as booted (32 KB, 8 ways, 32-byte lines, no prefetch)
 *   1 64-byte lines
 *   2 prefetch on a miss, smallest size
 *   3 64-byte lines and prefetch
 * Temporary: not part of a normal build, and the cache change is a ROM call the
 * SDK says not to make from an application. */
#pragma once

#include <Arduino.h>
#include <esp32s3/rom/cache.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>

#ifndef PSRAM_BENCH_VARIANT
#define PSRAM_BENCH_VARIANT 0
#endif

namespace psram_bench {

constexpr size_t kBytes = 466 * 466 * 2;

static uint32_t rng_state = 12345;
static inline uint32_t rnd() {
  rng_state = rng_state * 1664525u + 1013904223u;
  return rng_state >> 8;
}

static void report(const char *name, double us, double count) {
  Serial.printf("bench %s total_ms=%.2f per_us=%.4f n=%.0f\n", name, us / 1000.0,
                us / count, count);
}

static void sequential(const char *name, volatile uint16_t *p, size_t n16, int passes) {
  const int64_t t0 = esp_timer_get_time();
  for (int k = 0; k < passes; k++)
    for (size_t i = 0; i < n16; i++) p[i] = (uint16_t)(i + k);
  report(name, (double)(esp_timer_get_time() - t0), (double)n16 * passes);
}

static void sequential32(const char *name, volatile uint32_t *p, size_t n32, int passes) {
  const int64_t t0 = esp_timer_get_time();
  for (int k = 0; k < passes; k++)
    for (size_t i = 0; i < n32; i++) p[i] = (uint32_t)(i + k);
  report(name, (double)(esp_timer_get_time() - t0), (double)n32 * passes);
}

/* One 16-bit store every `stride` bytes across the buffer. */
static void strided_write(const char *name, volatile uint8_t *base, size_t bytes, size_t stride) {
  size_t touches = 0;
  const int64_t t0 = esp_timer_get_time();
  while (touches < 200000) {
    for (size_t off = 0; off + 2 <= bytes; off += stride) {
      *(volatile uint16_t *)(base + off) = (uint16_t)off;
      touches++;
    }
  }
  report(name, (double)(esp_timer_get_time() - t0), (double)touches);
}

static void strided_read(const char *name, volatile uint8_t *base, size_t bytes, size_t stride) {
  size_t touches = 0;
  uint32_t sink = 0;
  const int64_t t0 = esp_timer_get_time();
  while (touches < 200000) {
    for (size_t off = 0; off + 2 <= bytes; off += stride) {
      sink += *(volatile uint16_t *)(base + off);
      touches++;
    }
  }
  report(name, (double)(esp_timer_get_time() - t0), (double)touches);
  if (sink == 0xdeadbeef) Serial.print("");
}

/* Spans of `len` 16-bit pixels at random places in the first `region` bytes,
 * 466 pixels to a row as the canvas has them. */
static void random_spans(const char *name, volatile uint16_t *px, size_t region, int len, int spans) {
  const size_t pixels = region / 2;
  const int64_t t0 = esp_timer_get_time();
  for (int s = 0; s < spans; s++) {
    const size_t at = rnd() % (pixels - len);
    volatile uint16_t *p = px + at;
    for (int i = 0; i < len; i++) p[i] = (uint16_t)s;
  }
  report(name, (double)(esp_timer_get_time() - t0), spans);
}

static void row_copy(const char *name, volatile uint16_t *dst, const uint16_t *src) {
  const int64_t t0 = esp_timer_get_time();
  for (int pass = 0; pass < 5; pass++)
    for (int y = 0; y < 466; y++) memcpy((void *)(dst + y * 466), src, 466 * 2);
  report(name, (double)(esp_timer_get_time() - t0), 5.0 * 466);
}

#ifndef PSRAM_BENCH_AUTOLOAD_TRIGGER
#define PSRAM_BENCH_AUTOLOAD_TRIGGER 1
#endif
#ifndef PSRAM_BENCH_AUTOLOAD_SIZE
#define PSRAM_BENCH_AUTOLOAD_SIZE 0
#endif

/* Switches the data cache to 64-byte lines and/or turns its prefetch on. Run
 * it first thing in setup(), before the other core does much. */
static void set_dcache(bool line64, bool prefetch) {
  noInterrupts();
  Cache_WriteBack_All();
  const uint32_t autoload = Cache_Suspend_DCache();
  if (line64) {
    Cache_Set_DCache_Mode(DCACHE_SIZE_32KB, CACHE_4WAYS_ASSOC, CACHE_LINE_SIZE_64B);
    Cache_Invalidate_DCache_All();
  }
  if (prefetch) {
    autoload_config cfg = {1, 0, PSRAM_BENCH_AUTOLOAD_TRIGGER, PSRAM_BENCH_AUTOLOAD_SIZE};
    Cache_Config_DCache_Autoload(&cfg);
  }
  Cache_Resume_DCache(prefetch ? 1 : autoload);
  interrupts();
}

static void apply_variant() {
  const bool line64 = PSRAM_BENCH_VARIANT == 1 || PSRAM_BENCH_VARIANT == 3;
  const bool prefetch = PSRAM_BENCH_VARIANT == 2 || PSRAM_BENCH_VARIANT == 3;
  Serial.printf("bench variant=%d before: dcache line=%u\n", PSRAM_BENCH_VARIANT,
                (unsigned)Cache_Get_DCache_Line_Size());
  set_dcache(line64, prefetch);
  Serial.printf("bench variant=%d after: dcache line=%u\n", PSRAM_BENCH_VARIANT,
                (unsigned)Cache_Get_DCache_Line_Size());
}

static void run() {
  Serial.println("bench begin");
  apply_variant();
  volatile uint16_t *ps = (volatile uint16_t *)heap_caps_malloc(kBytes, MALLOC_CAP_SPIRAM);
  volatile uint16_t *in =
      (volatile uint16_t *)heap_caps_malloc(64 * 1024, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  uint16_t *row = (uint16_t *)heap_caps_malloc(466 * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!ps || !in || !row) {
    Serial.println("bench alloc failed");
    return;
  }
  memset((void *)ps, 0, kBytes);
  memset((void *)in, 0, 64 * 1024);
  memset(row, 0x55, 466 * 2);

  sequential("psram_seq16", ps, kBytes / 2, 4);
  sequential32("psram_seq32", (volatile uint32_t *)ps, kBytes / 4, 4);
  sequential("sram_seq16", in, 32 * 1024, 40);
  row_copy("psram_rowcopy_466x2B", ps, row);

  static const size_t kStrides[] = {2, 4, 8, 16, 32, 64, 128, 256, 932};
  char name[48];
  for (size_t s : kStrides) {
    snprintf(name, sizeof name, "psram_write_stride%u", (unsigned)s);
    strided_write(name, (volatile uint8_t *)ps, kBytes, s);
  }
  for (size_t s : kStrides) {
    snprintf(name, sizeof name, "psram_read_stride%u", (unsigned)s);
    strided_read(name, (volatile uint8_t *)ps, kBytes, s);
  }
  for (size_t s : {2u, 32u, 64u, 932u}) {
    snprintf(name, sizeof name, "sram_write_stride%u", (unsigned)s);
    strided_write(name, (volatile uint8_t *)in, 64 * 1024, s);
  }

  static const int kLens[] = {1, 2, 4, 8, 16, 32, 64, 128, 256};
  for (int len : kLens) {
    snprintf(name, sizeof name, "psram_spans_len%d", len);
    random_spans(name, ps, kBytes, len, 20000);
  }
  for (int len : {1, 8, 64}) {
    snprintf(name, sizeof name, "sram_spans_len%d", len);
    random_spans(name, in, 64 * 1024, len, 20000);
  }
  static const size_t kRegionsKb[] = {8, 16, 24, 32, 40, 48, 64, 128, 256, 424};
  for (size_t kb : kRegionsKb) {
    snprintf(name, sizeof name, "psram_ws%uKB_len8", (unsigned)kb);
    random_spans(name, ps, kb * 1024, 8, 50000);
  }
  Serial.println("bench done");
}

}  // namespace psram_bench
