/* Builds the unmodified cwaves.c in single precision (the ESP32-S3's FPU is
 * single-precision only, so `double` maths runs in software) and with its
 * decimal literals made floats at build time (tools/float_literals.py), or
 * `0.5 * x` would stay software double. The hack takes 15 cosines for each of
 * 233 columns a frame, but on the board that is 4 ms of a 61 ms step.
 *
 * The rest is the fills: each column is a 2 by 466 rectangle, so every store
 * lands on a different cache line of the PSRAM canvas. The picture depends on
 * x alone, so a full-height column is written into canvas row 0 only, and the
 * draw wrapper copies that row down the canvas in sequential row writes. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "screenhack.h"

#include "core/canvas.h"
#include "hacks/single_precision.h"
#include "x11shim/pixmap.h"

static int cwaves_row_dirty;

static int cwaves_fill_column(Display *dpy, Drawable d, GC gc, int x, int y,
                              unsigned int w, unsigned int h) {
  Canvas *c = xshim_target(dpy, d);
  if (d != XSHIM_WINDOW || !c || y != 0 || h < (unsigned int)c->h || x < 0 ||
      (int64_t)x + w > c->w)
    return (XFillRectangle)(dpy, d, gc, x, y, w, h);
  for (unsigned int i = 0; i < w; i++) c->px[x + i] = (uint16_t)gc->foreground;
  cwaves_row_dirty = 1;
  return 0;
}

#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)
#define XFillRectangle cwaves_fill_column
#undef M_PI
#define M_PI 3.14159265358979323846f

#include "cwaves_floatlit.c"

#undef XFillRectangle

static unsigned long cwaves_draw_rows(Display *dpy, Window window,
                                      void *closure) {
  const unsigned long delay = cwaves_draw(dpy, window, closure);
  if (cwaves_row_dirty) {
    Canvas *c = dpy->canvas;
    for (int y = 1; y < c->h; y++)
      memcpy(c->px + (size_t)y * (size_t)c->w, c->px,
             (size_t)c->w * sizeof(uint16_t));
    canvas_mark_dirty(c, 0, 0, c->w, c->h);
    cwaves_row_dirty = 0;
  }
  return delay;
}

const HackEntry cwaves_hack = {"CWaves", cwaves_defaults, cwaves_init,
                               cwaves_draw_rows, cwaves_free, NULL, NULL};
