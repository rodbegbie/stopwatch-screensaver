#ifndef CORE_PUSH_PLAN_H
#define CORE_PUSH_PLAN_H

#include "core/canvas.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int y, x0, x1;       /* x1 inclusive */
  bool end_of_group;   /* last row of a batch: the display is flushed after it */
} PushRow;

typedef enum { PUSH_NONE, PUSH_ROWS, PUSH_ALL } PushKind;

/* Measured on the device (Pyro, 22 s per shape). M5GFX keeps a framebuffer for
 * this panel: each pushImage copies pixels into it, and endWrite sends the
 * bounding box of everything written since startWrite in one flush. So rows
 * pushed in one batch cost the box around all of them, and rows far apart are
 * cheaper in separate batches (two corner pixels: 11.7 ms together, 0.1 ms
 * apart). One batch costs a flush, plus the box it covers, plus a call and a
 * copy for each row written. A single full push costs PUSH_PIXEL_NS a pixel. */
#define PUSH_PIXEL_NS 143
#define PUSH_FLUSH_NS 17000
#define PUSH_FLUSH_ROW_NS 1600
#define PUSH_FLUSH_PIXEL_NS 54
#define PUSH_CALL_NS 1500
#define PUSH_COPY_PIXEL_NS 90

/* Turns the canvas's dirty spans into row pushes and marks every row clean.
 * `rows` has room for c->h entries. For PUSH_ROWS, `rows` holds *count entries
 * in increasing y, grouped into batches (the last row of each has
 * end_of_group); otherwise *count is 0. PUSH_ALL means the rows would cost at
 * least as much as pushing the whole canvas in one call.
 * With use_disc, each span is clipped to the canvas's disc (rows left empty
 * are dropped) and the rows are split into the cheapest batches found; a
 * full frame then becomes bands rather than PUSH_ALL. Without it, whole dirty
 * spans are grouped greedily. */
PushKind push_plan(Canvas *c, PushRow *rows, int *count, bool use_disc);

/* The model's cost in ns of pushing `rows` in the batches they are marked
 * with, and the greedy grouping (which re-marks them). For tests. */
int64_t push_plan_cost(const PushRow *rows, int count);
int64_t push_group_greedy(PushRow *rows, int count);

typedef struct {
  void *ctx;
  void (*begin)(void *ctx);
  void (*row)(void *ctx, int y, int x0, int x1);
  void (*all)(void *ctx);
  void (*end)(void *ctx);
} PushSink;

/* Plans, then runs the plan through the sink: nothing for PUSH_NONE; for
 * PUSH_ROWS, begin, a row call per row, end, once per group; a single all for PUSH_ALL. Returns the
 * number of rows pushed (0, the row count, or c->h). */
int push_present(Canvas *c, PushRow *scratch, const PushSink *sink, bool use_disc);

#ifdef __cplusplus
}
#endif

#endif
