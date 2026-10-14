#ifndef RUNNER_PUSH_OVERLAP_H
#define RUNNER_PUSH_OVERLAP_H

#include <stdbool.h>
#include <stdint.h>

#include "core/canvas.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Lets a hack that repaints its whole canvas every step (HackEntry
 * repaints_all) draw its next frame while a worker pushes the last one. Two
 * pixel buffers swap roles each frame: present() hands the finished one to the
 * worker and points the canvas at the other. */
typedef struct PushOverlap PushOverlap;

/* The platform's thread primitives: FreeRTOS on the device, pthreads in the
 * host tests. sem_destroy may be NULL. */
typedef struct {
  void *(*sem_create)(void); /* binary semaphore, starts empty */
  void (*sem_give)(void *sem);
  void (*sem_take)(void *sem); /* blocks */
  int (*spawn)(void (*body)(void *), void *arg); /* 0 on success */
  uint32_t (*now_us)(void);
  void (*sem_destroy)(void *sem);
} PushOverlapOps;

/* Runs on the worker: pushes the dirty rows of view, a canvas whose px is the
 * finished frame. Returns the rows sent. */
typedef int (*PushOverlapPush)(void *ctx, Canvas *view);

/* spare is a buffer the size of canvas->px that the module may swap in; the
 * caller keeps ownership. Returns NULL if spare is NULL or anything fails to
 * allocate or start. */
PushOverlap *push_overlap_create(const Canvas *canvas, uint16_t *spare,
                                 const PushOverlapOps *ops, PushOverlapPush push,
                                 void *push_ctx);

/* Waits for the worker, then, if eligible and something is dirty, hands the
 * frame to the worker, swaps canvas->px with the spare and returns true.
 * Otherwise returns false and the caller pushes synchronously: if the canvas
 * was swapped, the newest frame is swapped back in first (and canvas's dirty
 * spans cleared, since the display already shows it). */
bool push_overlap_present(PushOverlap *p, Canvas *canvas, bool eligible);
/* Blocks until the worker is idle. */
void push_overlap_wait(PushOverlap *p);
/* Waits, then forgets the newest frame without swapping: call when a new hack
 * starts, whose canvas is the one to keep. */
void push_overlap_reset(PushOverlap *p);
/* How long the push that ran beside the last step took; 0 after a synchronous
 * present. A hack's delay is spent by this much. */
uint32_t push_overlap_credit_us(const PushOverlap *p);
/* Rows sent by pushes that finished since the last call. */
int push_overlap_take_rows(PushOverlap *p);
void push_overlap_destroy(PushOverlap *p);

#ifdef __cplusplus
}
#endif

#endif
