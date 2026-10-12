/* Builds the unmodified piecewise.c in single precision (the ESP32-S3's FPU is
 * single-precision only, so `double` maths runs in software) with its decimal
 * literals made floats at build time (tools/float_literals.py) and frand in
 * float (single_frand.h): its sweep over the circles' intersections and its
 * arcs made a 67 ms step in double, 42 ms in float. HAVE_JWXYZ is defined
 * after the framework headers. Upstream defines it where the window system double-buffers for the
 * hack, and the hack then draws straight on the window. The shim can draw into
 * pixmaps, but two back buffers would cost 868 KB and a full-frame copy each
 * frame.
 *
 * Each circle has two fringes, and each fringe a growing array of
 * intersections (`i`). Upstream's free releases the low fringe's array and not
 * the high one's, so the wrapper frees those first: 32 blocks each time
 * Piecewise stops. */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "screenhack.h"

#include "hacks/single_precision.h"
#include "hacks/single_frand.h"
#define rint(x) rintf(x)
#define ceil(x) ceilf(x)
#define floor(x) floorf(x)
#undef M_PI
#define M_PI 3.14159265358979323846f

#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)
#define HAVE_JWXYZ

#define piecewise_free piecewise_free_upstream
#include "piecewise_floatlit.c"
#undef piecewise_free

static void piecewise_free(Display *dpy, Window window, void *closure) {
  struct state *st = (struct state *)closure;
  for (int i = 0; i < st->count; i++)
    if (st->circles[i].hi) {
      free(st->circles[i].hi->i);
      st->circles[i].hi->i = NULL;
    }
  piecewise_free_upstream(dpy, window, closure);
}

const HackEntry piecewise_hack = {"Piecewise", piecewise_defaults, piecewise_init,
                                  piecewise_draw, piecewise_free, NULL, NULL};
