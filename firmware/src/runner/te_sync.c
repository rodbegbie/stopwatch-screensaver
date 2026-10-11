#include "runner/te_sync.h"

void te_sync_init(TeSync *s, uint32_t offset_us, uint32_t timeout_us) {
  s->last_rise_us = 0;
  s->period_us = 0;
  s->rises = 0;
  s->rejects = 0;
  s->offset_us = offset_us;
  s->timeout_us = timeout_us;
}

void TE_SYNC_ISR te_sync_edge(TeSync *s, uint32_t now_us, int level) {
  if (!level) return;
  if (s->rises == 0) {
    s->last_rise_us = now_us;
    s->rises = 1;
    return;
  }
  const uint32_t gap = now_us - s->last_rise_us;
  if (gap < TE_SYNC_MIN_PERIOD_US) return; /* a glitch: keep the true edge */

  const uint32_t period = s->period_us;
  if (!period) {
    if (gap <= TE_SYNC_MAX_PERIOD_US) {
      s->period_us = gap;
      s->rises = 2;
    } else {
      s->rises = 1;
    }
    s->last_rise_us = now_us;
    return;
  }

  /* A real edge lands a whole number of periods after the last one, give or
   * take jitter. Anything else is a spurious pulse and is ignored; a run of
   * them means the lock is wrong, so it is dropped and learnt again. */
  const uint32_t k = (gap + period / 2) / period;
  const uint64_t whole = (uint64_t)k * period;
  const uint64_t off = gap > whole ? gap - whole : whole - gap;
  if (k == 0 || off > period / 16) {
    if (++s->rejects >= TE_SYNC_RELOCK_AFTER) {
      s->period_us = 0;
      s->rises = 1;
      s->rejects = 0;
      s->last_rise_us = now_us;
    }
    return;
  }
  s->rejects = 0;
  if (k == 1) {
    /* an eighth of the way to each new gap, so a slow drift is followed */
    if (gap >= period)
      s->period_us = period + (gap - period) / 8;
    else
      s->period_us = period - (period - gap) / 8;
  }
  s->rises = 2;
  s->last_rise_us = now_us;
}

uint32_t te_sync_period_us(const TeSync *s) { return s->period_us; }

uint32_t te_sync_wait_us(const TeSync *s, uint32_t now_us) {
  const uint32_t period = s->period_us;
  if (!period) return 0;
  uint32_t since = now_us - s->last_rise_us;
  if ((int32_t)since < 0) {
    /* the edge landed just after now was read; anything further ahead than a
     * period is an old edge seen across half the counter's range */
    if ((int64_t)0 - (int32_t)since >= (int64_t)period) return 0;
    since = 0;
  }
  if (since > s->timeout_us) return 0;
  const uint32_t into = since % period;
  const uint32_t offset = s->offset_us % period;
  return into <= offset ? offset - into : period - into + offset;
}
