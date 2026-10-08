#include "runner/button_latch.h"

void button_latch_init(ButtonLatch *l, uint32_t settle_ms) {
  l->pending = 0;
  l->first_ms = 0;
  l->settle_ms = settle_ms;
  l->candidate_since_ms = 0;
  l->has_sample = false;
  l->stable_down = false;
  l->candidate_down = false;
}

void button_latch_sample(ButtonLatch *l, bool down, uint32_t now_ms) {
  if (!l->has_sample) {
    l->has_sample = true;
    l->stable_down = down;
    l->candidate_down = down;
    return;
  }
  if (down == l->stable_down) {
    l->candidate_down = down;
    return;
  }
  if (l->candidate_down != down) {
    l->candidate_down = down;
    l->candidate_since_ms = now_ms;
  }
  if ((uint32_t)(now_ms - l->candidate_since_ms) < l->settle_ms) return;
  l->stable_down = down;
  if (!down) return;
  if (l->pending == 0) l->first_ms = l->candidate_since_ms;
  __atomic_fetch_add(&l->pending, 1, __ATOMIC_RELEASE);
}

uint32_t button_latch_pending(const ButtonLatch *l) { return l->pending; }

bool button_latch_take(ButtonLatch *l, uint32_t *began_ms) {
  uint32_t taken = __atomic_exchange_n(&l->pending, 0, __ATOMIC_ACQUIRE);
  if (taken == 0) return false;
  if (began_ms) *began_ms = l->first_ms;
  return true;
}
