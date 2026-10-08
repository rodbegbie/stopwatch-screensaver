#include "runner/button_latch.h"

/* Sampling runs on one core and taking on another. The critical sections are a
 * few instructions long, so a spin lock is cheaper than anything heavier. */
static void lock(ButtonLatch *l) {
  while (__atomic_exchange_n(&l->lock, 1, __ATOMIC_ACQUIRE)) {
  }
}

static void unlock(ButtonLatch *l) { __atomic_store_n(&l->lock, 0, __ATOMIC_RELEASE); }

void button_latch_init(ButtonLatch *l, uint32_t settle_ms) {
  l->lock = 0;
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
  lock(l);
  if (l->pending == 0) l->first_ms = l->candidate_since_ms;
  l->pending++;
  unlock(l);
}

uint32_t button_latch_pending(const ButtonLatch *l) { return l->pending; }

bool button_latch_take(ButtonLatch *l, uint32_t *began_ms) {
  lock(l);
  uint32_t taken = l->pending;
  uint32_t began = l->first_ms;
  l->pending = 0;
  unlock(l);
  if (taken == 0) return false;
  if (began_ms) *began_ms = began;
  return true;
}
