#include "runner/button_latch.h"

void button_latch_init(ButtonLatch *l, uint32_t debounce_ms) {
  l->pending = 0;
  l->first_ms = 0;
  l->last_edge_ms = 0;
  l->has_edge = 0;
  l->debounce_ms = debounce_ms;
}

void button_latch_edge(ButtonLatch *l, bool pressed, uint32_t now_ms) {
  bool quiet = !l->has_edge || (uint32_t)(now_ms - l->last_edge_ms) >= l->debounce_ms;
  l->has_edge = 1;
  l->last_edge_ms = now_ms;
  if (!pressed || !quiet) return;
  if (l->pending == 0) l->first_ms = now_ms;
  __atomic_fetch_add(&l->pending, 1, __ATOMIC_RELEASE);
}

uint32_t button_latch_pending(const ButtonLatch *l) { return l->pending; }

bool button_latch_take(ButtonLatch *l, uint32_t *first_ms) {
  uint32_t taken = __atomic_exchange_n(&l->pending, 0, __ATOMIC_ACQUIRE);
  if (taken == 0) return false;
  if (first_ms) *first_ms = l->first_ms;
  return true;
}
