/* Stand-in for xscreensaver's utils/thread_util.h. Not derived from
 * xscreensaver code.
 *
 * A hack hands rows of its work to a pool of threads. The device runs hacks on
 * one task, so the pool runs each "thread" in turn on the caller, as upstream
 * does on a system without POSIX threads: threadpool_run returns with the work
 * done and threadpool_wait has nothing to wait for. */
#ifndef XSHIM_THREAD_UTIL_H
#define XSHIM_THREAD_UTIL_H

#include <stddef.h>

#include "x11shim/xshim.h"

#ifdef __cplusplus
extern "C" {
#endif

/* No -threads option or resource: there is nothing to turn on. */
#define THREAD_DEFAULTS
#define THREAD_DEFAULTS_XLOCK
#define THREAD_OPTIONS

/* The struct that holds `member`, a pointer to its field `member_name`. */
#define GET_PARENT_OBJ(struct_name, member_name, member) \
  (struct_name *)((char *)(member) - offsetof(struct_name, member_name));

struct threadpool {
  unsigned count; /* the count passed to threadpool_create */
  size_t thread_size;
  void (*thread_destroy)(void *self);
  void *threads; /* count objects of thread_size bytes, one after another */
};

struct threadpool_class {
  size_t size;
  /* Returns 0, or an errno value that threadpool_create returns. */
  int (*create)(void *self, struct threadpool *pool, unsigned id);
  void (*destroy)(void *self);
};

/* Makes count thread objects, calling create on each with ids from 0. If one
 * fails, destroys those already made and returns its error (ENOMEM if the
 * memory cannot be had). */
int threadpool_create(struct threadpool *self, const struct threadpool_class *cls,
                      Display *dpy, unsigned count);
void threadpool_destroy(struct threadpool *self);
/* Calls func on each thread object in id order, then returns. */
void threadpool_run(struct threadpool *self, void (*func)(void *));
void threadpool_wait(struct threadpool *self);

/* One core runs hacks. */
unsigned hardware_concurrency(Display *dpy);
/* With no threads sharing memory, upstream's answer is sizeof(void *). */
unsigned thread_memory_alignment(Display *dpy);

#ifdef __cplusplus
}
#endif

#endif
