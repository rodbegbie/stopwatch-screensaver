/* Builds the unmodified anemone.c in single precision (the ESP32-S3's FPU is
 * single-precision only, so `double` maths runs in software) and
 * with its decimal literals made floats at build time
 * (tools/float_literals.py), or `0.5 * x` would stay software double, with HAVE_JWXYZ
 * defined after the framework headers. Upstream defines it where the window
 * system double-buffers for the hack, and the hack then draws straight on the
 * window. The shim can draw into pixmaps, but a back buffer would cost 434 KB
 * and a full-frame copy each frame, and would dirty every row. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "screenhack.h"

#include "hacks/single_precision.h"
#include "hacks/span_erase.h"
#include "x11shim/pixmap.h"

#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)
#define HAVE_JWXYZ
#undef M_PI
#define M_PI 3.14159265358979323846f

/* Each frame fills the whole window black and draws the arms again with about
 * 740 short round-capped lines. They cover a fraction of the window, and the
 * clear alone is a full frame of PSRAM writes (about 25 ms of the step): erase
 * only where last frame's lines were (hacks/span_erase.h), for the same
 * picture. Each line notes its box grown by its round caps. */
static SpanTrack anemone_spans;

static short anemone_clamp(int v) { return (short)(v < -30000 ? -30000 : v > 30000 ? 30000 : v); }

static int anemone_draw_line(Display *dpy, Drawable d, GC gc, int x1, int y1, int x2, int y2) {
  const int reach = gc->line_width / 2 + 2;
  const XPoint box[2] = {
      {anemone_clamp((x1 < x2 ? x1 : x2) - reach), anemone_clamp((y1 < y2 ? y1 : y2) - reach)},
      {anemone_clamp((x1 > x2 ? x1 : x2) + reach), anemone_clamp((y1 > y2 ? y1 : y2) + reach)}};
  span_track_note(&anemone_spans, box, 2);
  return (XDrawLine)(dpy, d, gc, x1, y1, x2, y2);
}

static int anemone_fill_rectangle(Display *dpy, Drawable d, GC gc, int x, int y,
                                  unsigned int w, unsigned int h) {
  Canvas *c = xshim_target(dpy, d);
  if (c && d == XSHIM_WINDOW && x == 0 && y == 0 && w >= (unsigned int)c->w &&
      h >= (unsigned int)c->h) {
    span_track_erase(c, &anemone_spans, (uint16_t)gc->foreground);
    return 0;
  }
  return (XFillRectangle)(dpy, d, gc, x, y, w, h);
}

#define XDrawLine anemone_draw_line
#define XFillRectangle anemone_fill_rectangle

#define anemone_init anemone_init_upstream
#include "anemone_floatlit.c"
#undef anemone_init

static void *anemone_init(Display *dpy, Window window) {
  span_track_reset(&anemone_spans);
  return anemone_init_upstream(dpy, window);
}

/* 128 arms of up to 64 wide segments each took 49 ms a frame in single
 * precision; half the arms halve the drawing. */
static const char *const k_anemone_overrides[] = {"*arms: 64", "*delay: 10000", NULL};

const HackEntry anemone_hack = {"Anemone", anemone_defaults, anemone_init,
                                anemone_draw, anemone_free, NULL, k_anemone_overrides};
