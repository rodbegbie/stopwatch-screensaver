#ifndef HACKS_PHOSPHOR_TIMERS_H
#define HACKS_PHOSPHOR_TIMERS_H

/* A few one-shot timers, for the cursor blink Phosphor makes with Xt timers.
 * The shim's Xt timers never fire (there is no event loop), so the wrapper
 * keeps this queue, adds to it where upstream calls XtAppAddTimeOut and fires
 * what is due once a draw. Time is passed in, so a test can move it.
 *
 * A callback has Xt's shape: (closure, &id). It may add timers; one added
 * during a pt_fire waits for its own time, even if that is now. */
#define PT_SLOTS 4

typedef void (*PtProc)(void *closure, unsigned long *id);

typedef struct {
  unsigned long id; /* 0 for a free slot */
  double due;
  PtProc proc;
  void *closure;
} PtTimer;

typedef struct {
  PtTimer timer[PT_SLOTS];
  unsigned long next_id;
} PtQueue;

/* The new timer's id (never 0), or 0 if every slot is taken. */
static inline unsigned long pt_add(PtQueue *q, double now, unsigned long ms,
                                   PtProc proc, void *closure) {
  for (int i = 0; i < PT_SLOTS; i++)
    if (!q->timer[i].id) {
      q->timer[i].id = ++q->next_id;
      q->timer[i].due = now + (double)ms / 1000.0;
      q->timer[i].proc = proc;
      q->timer[i].closure = closure;
      return q->timer[i].id;
    }
  return 0;
}

/* Id 0 (no timer) matches only free slots, which stay free. */
static inline void pt_remove(PtQueue *q, unsigned long id) {
  for (int i = 0; i < PT_SLOTS; i++)
    if (q->timer[i].id == id) q->timer[i].id = 0;
}

static inline void pt_fire(PtQueue *q, double now) {
  /* Only the timers that were pending on entry: a callback's new one is not
   * looked at again in this pass, so it waits for the next. */
  PtTimer due[PT_SLOTS];
  int n = 0;
  for (int i = 0; i < PT_SLOTS; i++)
    if (q->timer[i].id && q->timer[i].due <= now) {
      due[n++] = q->timer[i];
      q->timer[i].id = 0;
    }
  for (int i = 0; i < n; i++) {
    unsigned long id = due[i].id;
    due[i].proc(due[i].closure, &id);
  }
}

#endif
