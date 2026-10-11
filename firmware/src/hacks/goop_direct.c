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

#include "hacks/float_polygon.h"
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
#define XFillPolygon float_x_fill_polygon
#undef M_PI
#define M_PI 3.14159265358979323846f

#include "goop_floatlit.c"

const HackEntry goop_hack = {"Goop", goop_defaults, goop_init, goop_draw,
                             goop_free, NULL, NULL};
