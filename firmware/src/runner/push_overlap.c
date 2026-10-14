#include "runner/push_overlap.h"

#include <stdlib.h>
#include <string.h>

struct PushOverlap {
  PushOverlapOps ops;
  PushOverlapPush push;
  void *ctx;
  Canvas view;
  uint16_t *spare;
  uint16_t *newest; /* the buffer that holds the latest pushed frame */
  void *go, *done;
  bool busy, quit;
  int rows_result, rows_taken;
  uint32_t dur_result, last_us, credit_us;
};

static void worker(void *arg) {
  PushOverlap *p = arg;
  for (;;) {
    p->ops.sem_take(p->go);
    if (p->quit) break;
    uint32_t t0 = p->ops.now_us();
    p->rows_result = p->push(p->ctx, &p->view);
    p->dur_result = p->ops.now_us() - t0;
    p->ops.sem_give(p->done);
  }
  p->ops.sem_give(p->done);
}

static void release(PushOverlap *p) {
  if (p->ops.sem_destroy) {
    if (p->go) p->ops.sem_destroy(p->go);
    if (p->done) p->ops.sem_destroy(p->done);
  }
  free(p->view.dirty_x0);
  free(p->view.dirty_x1);
  free(p);
}

PushOverlap *push_overlap_create(const Canvas *canvas, uint16_t *spare,
                                 const PushOverlapOps *ops, PushOverlapPush push,
                                 void *push_ctx) {
  if (!spare) return NULL;
  PushOverlap *p = calloc(1, sizeof *p);
  if (!p) return NULL;
  p->ops = *ops;
  p->push = push;
  p->ctx = push_ctx;
  p->spare = spare;
  p->view = *canvas;
  p->view.dirty_x0 = malloc((size_t)canvas->h * sizeof(int16_t));
  p->view.dirty_x1 = malloc((size_t)canvas->h * sizeof(int16_t));
  p->go = ops->sem_create();
  p->done = ops->sem_create();
  if (!p->view.dirty_x0 || !p->view.dirty_x1 || !p->go || !p->done ||
      ops->spawn(worker, p) != 0) {
    release(p);
    return NULL;
  }
  return p;
}

void push_overlap_wait(PushOverlap *p) {
  if (!p->busy) return;
  p->ops.sem_take(p->done);
  p->busy = false;
  p->rows_taken += p->rows_result;
  p->last_us = p->dur_result;
}

void push_overlap_reset(PushOverlap *p) {
  push_overlap_wait(p);
  p->newest = NULL;
}

static bool any_dirty(const Canvas *c) {
  int x0, x1;
  for (int y = 0; y < c->h; y++)
    if (canvas_dirty_row(c, y, &x0, &x1)) return true;
  return false;
}

bool push_overlap_present(PushOverlap *p, Canvas *canvas, bool eligible) {
  push_overlap_wait(p);
  if (eligible && any_dirty(canvas)) {
    memcpy(p->view.dirty_x0, canvas->dirty_x0,
           (size_t)canvas->h * sizeof(int16_t));
    memcpy(p->view.dirty_x1, canvas->dirty_x1,
           (size_t)canvas->h * sizeof(int16_t));
    canvas_clear_dirty(canvas);
    p->view.px = canvas->px;
    p->newest = canvas->px;
    canvas->px = p->spare;
    p->spare = p->newest;
    p->credit_us = p->last_us;
    p->busy = true;
    p->ops.sem_give(p->go);
    return true;
  }
  if (p->newest && p->newest != canvas->px) {
    p->spare = canvas->px;
    canvas->px = p->newest;
    canvas_clear_dirty(canvas);
  }
  p->newest = NULL;
  p->credit_us = 0;
  return false;
}

uint32_t push_overlap_credit_us(const PushOverlap *p) { return p->credit_us; }

int push_overlap_take_rows(PushOverlap *p) {
  int n = p->rows_taken;
  p->rows_taken = 0;
  return n;
}

void push_overlap_destroy(PushOverlap *p) {
  if (!p) return;
  push_overlap_wait(p);
  p->quit = true;
  p->ops.sem_give(p->go);
  p->ops.sem_take(p->done);
  release(p);
}
