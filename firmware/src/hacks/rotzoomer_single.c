/* Builds the unmodified rotzoomer.c in single precision (the ESP32-S3's FPU is
 * single-precision only, so `double` maths runs in software, and the hack
 * calls sin and cos in double for every pixel it moves) and with its decimal
 * literals made floats at build time (tools/float_literals.py).
 *
 * Upstream starts its picture and zoom boxes the first time its draw finds
 * the image load finished: it polls a loader that load_image_async_simple
 * returned. The shim finishes a load at once and returns no loader, so that
 * moment never came and the first draw used a NULL zoom box. The wrapper does
 * what the moment does, straight after init. */
#include <math.h>
#include <stdlib.h>

#include "screenhack.h"
#include "xshm.h"

#include "hacks/single_precision.h"
#undef M_PI
#define M_PI 3.14159265358979323846f

#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)
#define rotzoomer_init rotzoomer_upstream_init

#include "rotzoomer_floatlit.c"

#undef rotzoomer_init

static void *rotzoomer_init(Display *dpy, Window window) {
  struct state *st = (struct state *)rotzoomer_upstream_init(dpy, window);
  if (!st->img_loader) {
    st->orig_map = XGetImage(st->dpy, st->pm, 0, 0, st->width, st->height, ~0L, ZPixmap);
    init_hack(st);
  }
  return st;
}

const HackEntry rotzoomer_hack = {"RotZoomer", rotzoomer_defaults, rotzoomer_init,
                                  rotzoomer_draw, rotzoomer_free, NULL, NULL};
