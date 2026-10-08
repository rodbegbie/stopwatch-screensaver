#ifndef RUNNER_HACK_RUNNER_H
#define RUNNER_HACK_RUNNER_H

#include "core/canvas.h"
#include "runner/hack_entry.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct HackRunner HackRunner;

#define RUNNER_MIN_DELAY_US 1000UL
#define RUNNER_MAX_DELAY_US 1000000UL

HackRunner *runner_create(Canvas *canvas); /* uses g_hacks */
HackRunner *runner_create_with(Canvas *canvas, const HackEntry *const *hacks,
                               int count);
void runner_destroy(HackRunner *r);

/* Stops any running hack, clears the canvas, loads defaults, runs init.
 * Returns 0, or -1 for a bad index. */
int runner_start(HackRunner *r, int index);
/* One draw call; returns the hack's delay in microseconds, clamped to
 * [RUNNER_MIN_DELAY_US, RUNNER_MAX_DELAY_US]. */
unsigned long runner_step(HackRunner *r);
/* How much of the hack's delay is still left to wait once spent_us has
 * already gone on drawing and pushing the frame. Never negative. */
unsigned long runner_remaining_delay_us(unsigned long requested_us,
                                        unsigned long spent_us);
/* Switch hacks, wrapping around. Return the new index. */
int runner_next(HackRunner *r);
int runner_prev(HackRunner *r);
int runner_index(const HackRunner *r);

#ifdef __cplusplus
}
#endif

#endif
