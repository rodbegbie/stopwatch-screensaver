/* Builds the unmodified binaryhorizon.c, told the display is 24 bits deep,
 * drawing straight on the window, in single precision: its line drawing
 * widened every coordinate to software double, per pixel. Its decimal
 * literals are made floats at build time (tools/float_literals.py, and
 * tools/binaryring_patch.py for the exponent literal in frand1).
 *
 * Like BinaryRing, the hack blends each hit into its own buffer of pixel
 * values at a low alpha. In its 16-bit path a hit adds less than one step of
 * a channel and the random walk in next_color rounds down, so every colour
 * drifts to black or a dim green. In its 24-bit path (0xRRGGBB) it works, so
 * the wrapper gives it that depth.
 *
 * Each pixel it writes to its XImage goes to the window instead, packed to the
 * canvas's byte-swapped RGB565, and the frame's XCopyArea to the window is
 * skipped. The image and pixmap are made one pixel big, so the XPutImage between
 * them copies nothing: 32-bit ones would be 868 KB and 434 KB, and the two
 * full-frame copies would mark every row for the push.
 *
 * Every `duration` seconds the hack starts again, which callocs a new pixel
 * buffer and never frees the old one (868 KB on the device). The wrapper frees
 * it, and clears the window, as the copy of the new black pixmap would have.
 * The hack's own check and the wrapper's must agree, so the clock is read once
 * a frame and both get that second: two reads can straddle the boundary, and
 * upstream would then replace the buffer with the wrapper's cleanup skipped. */
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include "screenhack.h"

#include "core/canvas.h"
#include "x11shim/pixmap.h"

static Canvas *binaryhorizon_window;

static int binaryhorizon_put_pixel(XImage *im, int x, int y, unsigned long c) {
  (void)im;
  const unsigned r = (c >> 16) & 0xff, g = (c >> 8) & 0xff, b = c & 0xff;
  canvas_point(binaryhorizon_window, x, y,
               px_swap((uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))));
  return 0;
}
#define XPutPixel binaryhorizon_put_pixel
#define visual_depth(screen, visual) 24

static int binaryhorizon_skip_copy(Display *dpy, Drawable src, Drawable dst, GC gc,
                                   int sx, int sy, unsigned int w, unsigned int h,
                                   int dx, int dy) {
  (void)dpy, (void)src, (void)dst, (void)gc, (void)sx, (void)sy, (void)w, (void)h,
      (void)dx, (void)dy;
  return 0;
}
#define XCopyArea binaryhorizon_skip_copy

static Pixmap binaryhorizon_tiny_pixmap(Display *dpy, Drawable d, unsigned int w,
                                        unsigned int h, unsigned int depth) {
  (void)w, (void)h;
  return XCreatePixmap(dpy, d, 1, 1, depth);
}
#define XCreatePixmap binaryhorizon_tiny_pixmap

static XImage *binaryhorizon_tiny_image(Display *dpy, Drawable d, int x, int y,
                                        unsigned int w, unsigned int h,
                                        unsigned long plane_mask, int format) {
  (void)d, (void)x, (void)y, (void)w, (void)h, (void)plane_mask, (void)format;
  XImage *im = XCreateImage(dpy, NULL, 16, ZPixmap, 0, NULL, 1, 1, 32, 0);
  if (im) im->data = (char *)calloc(1, (size_t)im->bytes_per_line);
  return im;
}
#define XGetImage binaryhorizon_tiny_image

static time_t binaryhorizon_real_clock(void) { return (time)((time_t *)0); }
static time_t (*binaryhorizon_clock)(void) = binaryhorizon_real_clock;
static time_t binaryhorizon_frame_time;
static time_t binaryhorizon_time(time_t *out) {
  const time_t now =
      binaryhorizon_frame_time ? binaryhorizon_frame_time : binaryhorizon_clock();
  if (out) *out = now;
  return now;
}
#define time(p) binaryhorizon_time(p)

#include "hacks/single_precision.h"
#include "hacks/single_constants.h"

#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)
#define binaryhorizon_init binaryhorizon_upstream_init
#define binaryhorizon_draw binaryhorizon_upstream_draw

#include "binaryhorizon_floatlit.c"

#undef binaryhorizon_init
#undef binaryhorizon_draw

static void *binaryhorizon_init(Display *dpy, Window window) {
  binaryhorizon_window = xshim_target(dpy, window);
  return binaryhorizon_upstream_init(dpy, window);
}

static unsigned long binaryhorizon_draw(Display *display, Window win,
                                        void *closure) {
  struct state *st = (struct state *)closure;
  binaryhorizon_frame_time = binaryhorizon_clock();
  if (st->duration && binaryhorizon_frame_time > st->start_time + st->duration) {
    free(st->buffer);
    st->buffer = NULL;
    canvas_clear(binaryhorizon_window, 0);
  }
  const unsigned long delay = binaryhorizon_upstream_draw(display, win, closure);
  binaryhorizon_frame_time = 0;
  return delay;
}

const HackEntry binaryhorizon_hack = {
    "BinaryHorizon", binaryhorizon_defaults, binaryhorizon_init,
    binaryhorizon_draw, binaryhorizon_free, NULL, NULL};
