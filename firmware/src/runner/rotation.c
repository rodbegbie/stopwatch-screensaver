#include "runner/rotation.h"

void rotation_init(Rotation *r, uint32_t interval_ms, uint32_t now_ms) {
  r->interval_ms = interval_ms;
  r->started_ms = now_ms;
}

void rotation_reset(Rotation *r, uint32_t now_ms) { r->started_ms = now_ms; }

bool rotation_due(const Rotation *r, uint32_t now_ms) {
  return r->interval_ms != 0 && now_ms - r->started_ms >= r->interval_ms;
}
