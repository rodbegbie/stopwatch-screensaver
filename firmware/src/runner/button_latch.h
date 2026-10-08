#ifndef RUNNER_BUTTON_LATCH_H
#define RUNNER_BUTTON_LATCH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Remembers button presses that arrive while the main loop is busy, so a hack
 * with a long draw step cannot make a press vanish. The press function is
 * meant for a GPIO interrupt; take is meant for the main loop. */
typedef struct {
  volatile uint32_t pending;
  volatile uint32_t first_ms;
  volatile uint32_t last_ms;
  volatile uint32_t has_last;
  uint32_t debounce_ms;
} ButtonLatch;

void button_latch_init(ButtonLatch *l, uint32_t debounce_ms);

/* Records a press at now_ms. Presses within debounce_ms of the previous
 * accepted press are contact bounce and are ignored. */
void button_latch_press(ButtonLatch *l, uint32_t now_ms);

/* Presses accepted and not yet taken. */
uint32_t button_latch_pending(const ButtonLatch *l);

/* True if any press is pending. All pending presses are consumed at once, since
 * extra presses during a long step are almost always retries. When first_ms is
 * not NULL it receives the time of the earliest press taken. */
bool button_latch_take(ButtonLatch *l, uint32_t *first_ms);

#ifdef __cplusplus
}
#endif

#endif
