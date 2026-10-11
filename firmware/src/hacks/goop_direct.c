/* Builds the unmodified goop.c with HAVE_JWXYZ defined after the framework
 * headers, which selects the code that draws each layer as filled polygons in
 * order. The other path needs X colour planes and GXclear with a plane mask,
 * which the canvas (RGB565, no spare bits) cannot give.
 *
 * Single precision (the S3's FPU is single-precision only, so `double` maths
 * runs in software) with decimal literals made floats at build time, and the
 * blobs' polygons filled in float (hacks/float_polygon.h): the shim's fill
 * divides in double for every edge on every row.
 *
 * Upstream draws each frame into a window-sized pixmap and copies it to the
 * window. The canvas is only pushed once the step is done, so nothing can tear:
 * the pixmap is the window itself, and the copy is skipped (a full-frame
 * PSRAM copy and 434 KB less memory). A one-bit pixmap (its xor mode) stays a
 * real one. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "screenhack.h"

/* spline.h must come in before `double` becomes `float`: the hack includes it
 * too, but the include guard then keeps this copy, and spline.c is built with
 * doubles. Seen through the macro, the control arrays are float*, spline.c
 * reads them as double*, and every blob becomes garbage. */
#include "spline.h"
#include "hacks/float_polygon.h"
#include "hacks/span_erase.h"
#include "hacks/single_precision.h"
#include "hacks/single_frand.h"

static Pixmap goop_create_pixmap(Display *dpy, Drawable d, unsigned int w,
                                 unsigned int h, unsigned int depth) {
  if (depth == 1) return (XCreatePixmap)(dpy, d, w, h, depth);
  return (Pixmap)XSHIM_WINDOW;
}

static int goop_free_pixmap(Display *dpy, Pixmap p) {
  return p == (Pixmap)XSHIM_WINDOW ? 0 : (XFreePixmap)(dpy, p);
}

static int goop_copy_area(Display *dpy, Drawable src, Drawable dst, GC gc,
                          int sx, int sy, unsigned int w, unsigned int h,
                          int dx, int dy) {
  if (src == dst) return 0;
  return (XCopyArea)(dpy, src, dst, gc, sx, sy, w, h, dx, dy);
}

#define XCreatePixmap goop_create_pixmap
#define XFreePixmap goop_free_pixmap
#define XCopyArea goop_copy_area

#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)
#define HAVE_JWXYZ

/* Upstream's jwxyz path gives each layer's colour an alpha of 0xBB (73 percent),
 * so blobs that overlap show through each other. The shim has no per-pixel
 * alpha, so every blob fills with that weight: 23 parts in 32. 32 is opaque. */
#ifndef GOOP_ALPHA
#define GOOP_ALPHA 23
#endif

/* Each frame clears the window and fills the blobs again, but the blobs cover
 * a fraction of it: erase only where last frame's blobs were (hacks/
 * span_erase.h), for the same picture and 15 ms less of PSRAM writes. */
static SpanTrack goop_spans;

static int goop_fill_blob(Display *dpy, Drawable d, GC gc, XPoint *pts, int n,
                          int shape, int mode) {
  (void)shape, (void)mode;
  Canvas *c = xshim_target(dpy, d);
  if (c) span_track_note(&goop_spans, pts, n);
  if (c) float_fill_polygon_alpha(c, pts, n, (uint16_t)gc->foreground, GOOP_ALPHA);
  return 0;
}

#define XFillPolygon goop_fill_blob

static int goop_fill_rectangle(Display *dpy, Drawable d, GC gc, int x, int y,
                               unsigned int w, unsigned int h) {
  Canvas *c = xshim_target(dpy, d);
  if (c && d == XSHIM_WINDOW && x == 0 && y == 0 && w >= (unsigned int)c->w &&
      h >= (unsigned int)c->h) {
    span_track_erase(c, &goop_spans, (uint16_t)gc->foreground);
    return 0;
  }
  return (XFillRectangle)(dpy, d, gc, x, y, w, h);
}

#define XFillRectangle goop_fill_rectangle
#undef M_PI
#define M_PI 3.14159265358979323846f

#define goop_init goop_init_upstream
#include "goop_floatlit.c"
#undef goop_init

static void *goop_init(Display *dpy, Window window) {
  span_track_reset(&goop_spans);
  return goop_init_upstream(dpy, window);
}

const HackEntry goop_hack = {"Goop", goop_defaults, goop_init, goop_draw,
                             goop_free, NULL, NULL};
