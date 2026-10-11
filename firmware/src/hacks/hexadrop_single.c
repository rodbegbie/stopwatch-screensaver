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
static void hexadrop_fill_convex(Display *dpy, Window w, GC gc, XPoint *pts,
                                 int n, int shape, int mode) {
  (void)w, (void)shape, (void)mode;
  int ymin = pts[0].y, ymax = pts[0].y;
  for (int i = 1; i < n; i++) {
    if (pts[i].y < ymin) ymin = pts[i].y;
    if (pts[i].y > ymax) ymax = pts[i].y;
  }
  if (ymin < 0) ymin = 0;
  if (ymax >= dpy->canvas->h) ymax = dpy->canvas->h - 1;
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
    const int x0 = (int)ceilf(lo - 0.5f), x1 = (int)ceilf(hi - 0.5f) - 1;
    if (x1 >= x0)
      canvas_fill_rect(dpy->canvas, x0, y, x1 - x0 + 1, 1, (uint16_t)gc->foreground);
  }
}
#define XFillPolygon hexadrop_fill_convex

#include "hexadrop_floatlit.c"

const HackEntry hexadrop_hack = {"Hexadrop", hexadrop_defaults, hexadrop_init,
                                 hexadrop_draw, hexadrop_free, NULL, NULL};
