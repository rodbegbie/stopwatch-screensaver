#include "runner/button_latch.h"

void button_latch_init(ButtonLatch *l, uint32_t debounce_ms) {
  l->pending = 0;
  l->first_ms = 0;
  l->last_ms = 0;
  l->has_last = 0;
  l->debounce_ms = debounce_ms;
}

void button_latch_press(ButtonLatch *l, uint32_t now_ms) {
  if (l->has_last && (uint32_t)(now_ms - l->last_ms) < l->debounce_ms) return;
  l->has_last = 1;
  l->last_ms = now_ms;
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
