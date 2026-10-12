/* The serial thread pool declared in thread_util.h. Written for this project;
 * not derived from xscreensaver's utils. */
#include "thread_util.h"

#include <errno.h>
#include <stdlib.h>

static void *thread_at(const struct threadpool *self, unsigned i) {
  return (char *)self->threads + (size_t)i * self->thread_size;
}

int threadpool_create(struct threadpool *self, const struct threadpool_class *cls,
                      Display *dpy, unsigned count) {
  (void)dpy;
  self->count = count;
  self->thread_size = cls->size;
  self->thread_destroy = cls->destroy;
  self->threads = calloc(count ? count : 1, cls->size ? cls->size : 1);
  if (!self->threads) return ENOMEM;
  for (unsigned i = 0; i < count; i++) {
    const int error = cls->create(thread_at(self, i), self, i);
    if (error) {
      for (unsigned j = 0; j < i; j++) cls->destroy(thread_at(self, j));
      free(self->threads);
      self->threads = NULL;
      return error;
    }
  }
  return 0;
}

void threadpool_destroy(struct threadpool *self) {
  for (unsigned i = 0; i < self->count; i++) self->thread_destroy(thread_at(self, i));
  free(self->threads);
  self->threads = NULL;
}

void threadpool_run(struct threadpool *self, void (*func)(void *)) {
  for (unsigned i = 0; i < self->count; i++) func(thread_at(self, i));
}

void threadpool_wait(struct threadpool *self) { (void)self; }

unsigned hardware_concurrency(Display *dpy) {
  (void)dpy;
  return 1;
}

unsigned thread_memory_alignment(Display *dpy) {
  (void)dpy;
  return sizeof(void *);
}
