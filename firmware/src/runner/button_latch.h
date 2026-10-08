#ifndef RUNNER_BUTTON_LATCH_H
#define RUNNER_BUTTON_LATCH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Remembers button presses that arrive while the main loop is busy, so a hack
 * with a long draw step cannot make a press vanish. The edge function is meant
 * for a GPIO interrupt on both edges; take is meant for the main loop. */
typedef struct {
  volatile uint32_t pending;
  volatile uint32_t first_ms;
  volatile uint32_t last_edge_ms;
  volatile uint32_t has_edge;
  uint32_t debounce_ms;
} ButtonLatch;

void button_latch_init(ButtonLatch *l, uint32_t debounce_ms);

/* Reports a change of the button line at now_ms (pressed is true when the
 * button went down). A press counts only if no edge of either kind came in the
 * previous debounce_ms, so bounce on release cannot look like a second press.
 * Every edge restarts the quiet window, whether or not it counted. */
void button_latch_edge(ButtonLatch *l, bool pressed, uint32_t now_ms);

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
