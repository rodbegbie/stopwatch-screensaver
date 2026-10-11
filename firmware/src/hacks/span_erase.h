#ifndef HACKS_SPAN_ERASE_H
#define HACKS_SPAN_ERASE_H

#include <stdint.h>

#include "core/canvas.h"
#include "x11shim/xshim.h"

/* For a hack that clears the whole window and redraws it every frame, when what
 * it draws covers only part of it: clearing 466 by 466 pixels is about 15 ms of
 * PSRAM writes. The wrapper notes each polygon it fills (its bounding box, row
 * by row), and the next frame's "clear" erases only those, so the canvas ends
 * up the same as a full clear. */
#define SPAN_TRACK_ROWS 480

typedef struct {
  int16_t lo[SPAN_TRACK_ROWS], hi[SPAN_TRACK_ROWS]; /* lo > hi: nothing noted */
  int primed;                                       /* the first erase has run */
} SpanTrack;

static void span_track_reset(SpanTrack *t) {
  for (int y = 0; y < SPAN_TRACK_ROWS; y++) {
    t->lo[y] = INT16_MAX;
    t->hi[y] = -1;
  }
  t->primed = 0;
}

/* Notes the rows a polygon touches and the columns it spans there. */
static void span_track_note(SpanTrack *t, const XPoint *pts, int n) {
  if (!pts || n <= 0) return;
  int xmin = pts[0].x, xmax = pts[0].x, ymin = pts[0].y, ymax = pts[0].y;
  for (int i = 1; i < n; i++) {
    if (pts[i].x < xmin) xmin = pts[i].x;
    if (pts[i].x > xmax) xmax = pts[i].x;
    if (pts[i].y < ymin) ymin = pts[i].y;
    if (pts[i].y > ymax) ymax = pts[i].y;
  }
  if (xmin < 0) xmin = 0;
  if (ymin < 0) ymin = 0;
  if (ymax >= SPAN_TRACK_ROWS) ymax = SPAN_TRACK_ROWS - 1;
  if (xmax < xmin) return;
  for (int y = ymin; y <= ymax; y++) {
    if (xmin < t->lo[y]) t->lo[y] = (int16_t)xmin;
    if (xmax > t->hi[y]) t->hi[y] = (int16_t)xmax;
  }
}

/* The first call, and every call on a canvas taller than the tracker, clears
 * the lot; later ones erase what was noted since the last call, then forget it. */
static void span_track_erase(Canvas *c, SpanTrack *t, uint16_t colour) {
  if (!t->primed || c->h > SPAN_TRACK_ROWS) {
    canvas_fill_rect(c, 0, 0, c->w, c->h, colour);
    t->primed = c->h <= SPAN_TRACK_ROWS;
  } else {
    for (int y = 0; y < c->h; y++)
      if (t->lo[y] <= t->hi[y])
        canvas_fill_rect(c, t->lo[y], y, t->hi[y] - t->lo[y] + 1, 1, colour);
  }
  for (int y = 0; y < SPAN_TRACK_ROWS; y++) {
    t->lo[y] = INT16_MAX;
    t->hi[y] = -1;
  }
}

#endif
