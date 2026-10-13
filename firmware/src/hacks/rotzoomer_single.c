/* Builds the unmodified rotzoomer.c in single precision (the ESP32-S3's FPU is
 * single-precision only, so `double` maths runs in software, and the hack
 * calls sin and cos in double for every pixel it moves) and with its decimal
 * literals made floats at build time (tools/float_literals.py).
 *
 * Upstream starts its picture and zoom boxes the first time its draw finds
 * the image load finished: it polls a loader that load_image_async_simple
 * returned. The shim finishes a load at once and returns no loader, so that
 * moment never came and the first draw used a NULL zoom box. The wrapper does
 * what the moment does, straight after init, and again after each timed
 * reload (the shim finishes that load at once too).
 *
 * XGetImage hands back a 32-bit image, but upstream's buffer is 16 bits and
 * init_hack copies one into the other as raw bytes, which alternates the
 * picture with black. Every XGetImage in the hack is given a 16-bit copy. */
#include <math.h>
#include <stdlib.h>

#include "screenhack.h"
#include "xshm.h"

#include "hacks/single_precision.h"
#undef M_PI
#define M_PI 3.14159265358979323846f

static XImage *rotzoomer_get_image16(Display *dpy, Drawable d, int x, int y,
                                     unsigned int w, unsigned int h,
                                     unsigned long plane_mask, int format) {
  XImage *wide = XGetImage(dpy, d, x, y, w, h, plane_mask, format);
  if (!wide) return NULL;
  XImage *im = create_xshm_image(dpy, NULL, 16, ZPixmap, NULL, w, h);
  if (im)
    for (unsigned int j = 0; j < h; j++)
      for (unsigned int i = 0; i < w; i++)
        XPutPixel(im, (int)i, (int)j, XGetPixel(wide, (int)i, (int)j));
  XDestroyImage(wide);
  return im;
}
#define XGetImage rotzoomer_get_image16

#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)
#define rotzoomer_init rotzoomer_upstream_init
#define rotzoomer_draw rotzoomer_upstream_draw

#include "rotzoomer_floatlit.c"

#undef rotzoomer_init
#undef rotzoomer_draw

static void rotzoomer_finish_load(struct state *st) {
  if (st->orig_map) XDestroyImage(st->orig_map);
  st->orig_map = XGetImage(st->dpy, st->pm, 0, 0, st->width, st->height, ~0L, ZPixmap);
  init_hack(st);
}

static void *rotzoomer_init(Display *dpy, Window window) {
  struct state *st = (struct state *)rotzoomer_upstream_init(dpy, window);
  if (!st->img_loader) rotzoomer_finish_load(st);
  return st;
}

static unsigned long rotzoomer_draw(Display *dpy, Window window, void *closure) {
  struct state *st = (struct state *)closure;
  const time_t started = st->start_time;
  const int was_loading = st->img_loader != NULL;
  const unsigned long delay = rotzoomer_upstream_draw(dpy, window, closure);
  if (!was_loading && !st->img_loader && st->start_time != started)
    rotzoomer_finish_load(st);
  return delay;
}

const HackEntry rotzoomer_hack = {"RotZoomer", rotzoomer_defaults, rotzoomer_init,
                                  rotzoomer_draw, rotzoomer_free, NULL, NULL};
