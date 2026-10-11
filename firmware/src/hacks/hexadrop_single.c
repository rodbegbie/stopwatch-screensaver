/* Builds the unmodified hexadrop.c in single precision (the ESP32-S3's FPU is
 * single-precision only, so `double` maths runs in software), with its decimal
 * literals made floats at build time (tools/float_literals.py), or `0.5 * x`
 * would stay software double. Every cell takes a cos and a sin per corner, per
 * frame. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "screenhack.h"

#include "hacks/single_precision.h"

#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)
#undef M_PI
#define M_PI 3.14159265358979323846f

/* Hexadrop only fills convex polygons on the window. The shim's general fill
 * divides in double for every edge on every row, which is software maths here
 * and most of a frame. This takes the same pixels (same rounding rule) from a
 * float walk of each convex polygon's left and right edges. */
/* Two pixels a store: the canvas lives in PSRAM, where the 16-bit stores of
 * a span are the cost of a frame. */
static void hexadrop_span(uint16_t *row, int x0, int n, uint16_t v) {
  if (x0 & 1) {
    row[x0++] = v;
    n--;
  }
  const uint32_t v2 = (uint32_t)v | ((uint32_t)v << 16);
  uint32_t *p = (uint32_t *)(row + x0);
  for (; n >= 2; n -= 2) *p++ = v2;
  if (n) *(uint16_t *)p = v;
}

static void hexadrop_fill_convex(Display *dpy, Window w, GC gc, XPoint *pts,
                                 int n, int shape, int mode) {
  (void)w, (void)shape, (void)mode;
  Canvas *const c = dpy->canvas;
  int ymin = pts[0].y, ymax = pts[0].y;
  for (int i = 1; i < n; i++) {
    if (pts[i].y < ymin) ymin = pts[i].y;
    if (pts[i].y > ymax) ymax = pts[i].y;
  }
  if (ymin < 0) ymin = 0;
  if (ymax >= c->h) ymax = c->h - 1;
  int dirty_x0 = c->w, dirty_x1 = -1;
  for (int y = ymin; y <= ymax; y++) {
    const float sy = (float)y + 0.5f;
    float lo = 1e9f, hi = -1e9f;
    for (int i = 0, j = n - 1; i < n; j = i++) {
      const float yi = pts[i].y, yj = pts[j].y;
      if ((yi <= sy && yj > sy) || (yj <= sy && yi > sy)) {
        const float x = pts[i].x + (sy - yi) / (yj - yi) * (pts[j].x - pts[i].x);
        if (x < lo) lo = x;
        if (x > hi) hi = x;
      }
    }
    if (lo > hi) continue;
    int x0 = (int)ceilf(lo - 0.5f), x1 = (int)ceilf(hi - 0.5f) - 1;
    if (x0 < 0) x0 = 0;
    if (x1 >= c->w) x1 = c->w - 1;
    if (x1 < x0) continue;
    hexadrop_span(c->px + (size_t)y * (size_t)c->w, x0, x1 - x0 + 1,
                  (uint16_t)gc->foreground);
    if (x0 < dirty_x0) dirty_x0 = x0;
    if (x1 > dirty_x1) dirty_x1 = x1;
  }
  if (dirty_x1 >= dirty_x0)
    canvas_mark_dirty(c, dirty_x0, ymin, dirty_x1 - dirty_x0 + 1, ymax - ymin + 1);
}
#define XFillPolygon hexadrop_fill_convex

#include "hexadrop_floatlit.c"

const HackEntry hexadrop_hack = {"Hexadrop", hexadrop_defaults, hexadrop_init,
                                 hexadrop_draw, hexadrop_free, NULL, NULL};
