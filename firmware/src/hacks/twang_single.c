/* Builds the unmodified twang.c in single precision (the ESP32-S3's FPU is
 * single-precision only, so `double` maths runs in software, and the hack
 * rotates and zooms every pixel of every tile in double) and with its decimal
 * literals made floats at build time (tools/float_literals.py).
 *
 * Upstream takes its picture the first time its draw finds the image load
 * finished: it polls a loader that load_image_async_simple returned. The shim
 * finishes a load at once and returns no loader, so that moment never came and
 * the hack drew with no source image. The wrapper does what the moment does,
 * straight after init. */
#include <math.h>
#include <stdlib.h>

#include "screenhack.h"
#include "xshm.h"

#include "hacks/single_precision.h"
#undef M_PI
#define M_PI 3.14159265358979323846f

#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)
#define twang_init twang_upstream_init

#include "twang_floatlit.c"

#undef twang_init

static void *twang_init(Display *dpy, Window window) {
  struct state *st = (struct state *)twang_upstream_init(dpy, window);
  if (!st->img_loader) grabImage_done(st);
  return st;
}

const HackEntry twang_hack = {"Twang", twang_defaults, twang_init,
                              twang_draw, twang_free, NULL, NULL};
