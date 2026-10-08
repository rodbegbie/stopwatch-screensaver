#ifndef RUNNER_BUTTON_LATCH_H
#define RUNNER_BUTTON_LATCH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Remembers button presses that arrive while the main loop is busy, so a hack
 * with a long draw step cannot make a press vanish. A background task feeds it
 * samples of the button line every few milliseconds; the main loop takes the
 * presses. The line only counts as changed once it has held its new level for
 * the settle time, so bounce on press or release cannot produce extra presses. */
typedef struct {
  volatile uint32_t lock; /* guards pending and first_ms, which change together */
  volatile uint32_t pending;
  volatile uint32_t first_ms;
  uint32_t settle_ms;
  uint32_t candidate_since_ms;
  bool has_sample;
  bool stable_down;
  bool candidate_down;
} ButtonLatch;

void button_latch_init(ButtonLatch *l, uint32_t settle_ms);

/* One reading of the line (down is true while the button is pressed) taken at
 * now_ms. The first sample only sets the starting state, so a button held at
 * boot is not a press. */
void button_latch_sample(ButtonLatch *l, bool down, uint32_t now_ms);

/* Presses confirmed and not yet taken. */
uint32_t button_latch_pending(const ButtonLatch *l);

/* True if any press is pending. All pending presses are consumed at once, since
 * extra presses during a long step are almost always retries. When began_ms is
 * not NULL it receives the time the earliest press taken began. */
bool button_latch_take(ButtonLatch *l, uint32_t *began_ms);

#ifdef __cplusplus
}
#endif

#endif
