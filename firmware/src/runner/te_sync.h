#ifndef RUNNER_TE_SYNC_H
#define RUNNER_TE_SYNC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Times a push to the panel's refresh. The AMOLED scans its memory top to
 * bottom 60 times a second, and a full push takes about two refreshes, so a
 * push that starts at a random moment shows two pictures at once, with a seam
 * between them: a thin line that moved a few pixels can fall in the gap and
 * vanish for a frame. The panel's TE line pulses once a refresh. Starting a
 * push at the same point in every cycle puts the seam where it is chosen, and
 * the right point keeps the write ahead of the scan so there is none.
 *
 * An interrupt feeds it the edges of TE with the microsecond they arrived; the
 * main loop asks how long to wait. Times are 32-bit microseconds that may
 * wrap. */
typedef struct {
  volatile uint32_t last_rise_us;
  volatile uint32_t period_us; /* 0 until two rising edges one period apart */
  volatile uint32_t rises;     /* rising edges accepted, saturating at 2 */
  uint32_t offset_us;
  uint32_t timeout_us;
} TeSync;

/* A rising edge this soon after the last, or later than the longest period
 * that can be a missed edge, is not a refresh. */
#define TE_SYNC_MIN_PERIOD_US 8000u
#define TE_SYNC_MAX_PERIOD_US 33000u

/* offset_us is how long after a rising edge a push should start. When no edge
 * has arrived for timeout_us the panel is taken to have stopped pulsing and
 * waiting is switched off, so a dead line cannot hang the display. */
void te_sync_init(TeSync *s, uint32_t offset_us, uint32_t timeout_us);

/* One edge of the TE line (level 1 is a rising edge) at now_us. Called from an
 * interrupt, so on the device it lives in IRAM and uses nothing in flash. */
#if defined(ESP_PLATFORM)
#include "esp_attr.h"
#define TE_SYNC_ISR IRAM_ATTR
#else
#define TE_SYNC_ISR
#endif
void TE_SYNC_ISR te_sync_edge(TeSync *s, uint32_t now_us, int level);

/* The refresh period learnt so far, or 0. */
uint32_t te_sync_period_us(const TeSync *s);

/* Microseconds from now_us until a push should start: less than one period,
 * and 0 when the period is unknown or the edges have stopped. */
uint32_t te_sync_wait_us(const TeSync *s, uint32_t now_us);

#ifdef __cplusplus
}
#endif

#endif
