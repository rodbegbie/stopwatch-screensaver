/* Builds the unmodified binaryring.c, told the display is 24 bits deep, in
 * single precision (the ESP32-S3's FPU is single-precision only, so `double`
 * maths runs in software) and with its decimal literals made floats at build
 * time (tools/float_literals.py, and tools/binaryring_patch.py for the
 * exponent literal in frand1).
 *
 * The hack blends each anti-aliased hit into its own buffer of pixel values
 * at an alpha of 0.15 times the coverage. In its 16-bit path a hit adds less
 * than one 5-bit step of red or blue, which is dropped, so only green ever
 * builds up and the picture stays a dim green. In its 24-bit path (0xRRGGBB,
 * as on most X displays) the hits accumulate, so the wrapper gives it that
 * depth.
 *
 * Each pixel it writes to its XImage goes straight to the window instead,
 * packed to the canvas's byte-swapped RGB565, and the frame's XPutImage into a
 * pixmap and XCopyArea to the window are skipped: the window then holds what
 * the copy would have put there. Those two full-frame copies took 130 of a
 * 240 ms step on the board (the XImage is 32 bits a pixel, which XPutImage
 * copies a pixel at a time), and they marked every row for the push. */
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include "screenhack.h"

#include "core/canvas.h"
#include "x11shim/pixmap.h"

static Canvas *binaryring_window;

static int binaryring_put_pixel(XImage *im, int x, int y, unsigned long c) {
  (void)im;
  const unsigned r = (c >> 16) & 0xff, g = (c >> 8) & 0xff, b = c & 0xff;
  canvas_point(binaryring_window, x, y,
               px_swap((uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))));
  return 0;
}
#define XPutPixel binaryring_put_pixel
#define visual_depth(screen, visual) 24

static int binaryring_skip_put(Display *dpy, Drawable d, GC gc, XImage *im, int sx,
                               int sy, int dx, int dy, unsigned int w, unsigned int h) {
  (void)dpy, (void)d, (void)gc, (void)im, (void)sx, (void)sy, (void)dx, (void)dy, (void)w,
      (void)h;
  return 0;
}
static int binaryring_skip_copy(Display *dpy, Drawable src, Drawable dst, GC gc, int sx,
                                int sy, unsigned int w, unsigned int h, int dx, int dy) {
  (void)dpy, (void)src, (void)dst, (void)gc, (void)sx, (void)sy, (void)w, (void)h, (void)dx,
      (void)dy;
  return 0;
}
#define XPutImage binaryring_skip_put
#define XCopyArea binaryring_skip_copy

#include "hacks/single_precision.h"
#undef M_PI
#define M_PI 3.14159265358979323846f

#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)

#include "binaryring_floatlit.c"

static void *binaryring_init_on_window(Display *dpy, Window window) {
  binaryring_window = xshim_target(dpy, window);
  return binaryring_init(dpy, window);
}

const HackEntry binaryring_hack = {"BinaryRing", binaryring_defaults, binaryring_init_on_window,
                                   binaryring_draw, binaryring_free, NULL, NULL};
