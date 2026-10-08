#ifndef CORE_PUSH_PLAN_H
#define CORE_PUSH_PLAN_H

#include "core/canvas.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int y, x0, x1; /* x1 inclusive */
} PushRow;

typedef enum { PUSH_NONE, PUSH_ROWS, PUSH_ALL } PushKind;

/* Measured on the device with the rows pushed inside one startWrite: a call
 * costs about 4.6 us whatever its width, plus 143 ns for each pixel. A single
 * full push costs the same 143 ns per pixel and no per-row cost. */
#define PUSH_ROW_NS 4600
#define PUSH_PIXEL_NS 143

/* Turns the canvas's dirty spans into row pushes and marks every row clean.
 * `rows` has room for c->h entries. For PUSH_ROWS, `rows` holds *count entries
 * in increasing y; otherwise *count is 0. PUSH_ALL means the rows would cost
 * at least as much as pushing the whole canvas in one call. */
PushKind push_plan(Canvas *c, PushRow *rows, int *count);

typedef struct {
  void *ctx;
  void (*begin)(void *ctx);
  void (*row)(void *ctx, int y, int x0, int x1);
  void (*all)(void *ctx);
  void (*end)(void *ctx);
} PushSink;

/* Plans, then runs the plan through the sink: nothing for PUSH_NONE; begin,
 * row per row, end for PUSH_ROWS; a single all for PUSH_ALL. Returns the
 * number of rows pushed (0, the row count, or c->h). */
int push_present(Canvas *c, PushRow *scratch, const PushSink *sink);

#ifdef __cplusplus
}
#endif

#endif
