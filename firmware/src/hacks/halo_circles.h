#ifndef HACKS_HALO_CIRCLES_H
#define HACKS_HALO_CIRCLES_H

#include <math.h>
#include <stdint.h>

#include "core/canvas.h"
#include "x11shim/pixmap.h"
#include "x11shim/xshim.h"

/* The row ends of a whole circle or ellipse, in float. The shim's
 * canvas_fill_ellipse takes the same ends in double, which the S3 does in
 * software. Same pixels except where an end lands within float rounding of a
 * half-way point. Returns 0 for a row with no pixels. */
static int halo_circle_row(int x, int y, unsigned int w, unsigned int h,
                           int64_t row, int64_t *xa, int64_t *xb) {
  const float cx = (float)x + ((float)w - 1.0f) / 2.0f;
  const float cy = (float)y + ((float)h - 1.0f) / 2.0f;
  const float rx = (float)w / 2.0f, ry = (float)h / 2.0f;
  const float ny = ((float)row - cy) / ry;
  const float t = 1.0f - ny * ny;
  if (t < 0.0f) return 0;
  const float half = rx * sqrtf(t);
  *xa = (int64_t)ceilf(cx - half);
  *xb = (int64_t)floorf(cx + half);
  return *xb >= *xa;
}

/* A whole circle on the window, as XFillArc does it. Used when Halo draws
 * straight onto the window (its ramp mode). */
static void halo_fill_circle_on_canvas(Canvas *c, GC gc, int x, int y,
                                       unsigned int w, unsigned int h) {
  if (w == 0 || h == 0 || w > 0x7FFFFFFFu || h > 0x7FFFFFFFu) return;
  int64_t y0 = y < 0 ? 0 : y, y1 = (int64_t)y + (int64_t)h - 1;
  if (y1 >= c->h) y1 = c->h - 1;
  for (int64_t yy = y0; yy <= y1; yy++) {
    int64_t xa, xb;
    if (!halo_circle_row(x, y, w, h, yy, &xa, &xb)) continue;
    canvas_fill_rect(c, (int)xa, (int)yy, (int)(xb - xa + 1), 1,
                     (uint16_t)gc->foreground);
  }
}

#endif
