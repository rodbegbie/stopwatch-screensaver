#ifndef RUNNER_ROTATION_H
#define RUNNER_ROTATION_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Says when it is time to move on to the next hack. Times are millisecond
 * counter values that may wrap. An interval of 0 turns rotation off. */
typedef struct {
  uint32_t interval_ms;
  uint32_t started_ms;
} Rotation;

void rotation_init(Rotation *r, uint32_t interval_ms, uint32_t now_ms);

/* Restart the countdown: call whenever a hack starts, however it was chosen. */
void rotation_reset(Rotation *r, uint32_t now_ms);

bool rotation_due(const Rotation *r, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif
