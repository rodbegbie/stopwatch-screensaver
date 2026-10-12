/* Builds the unmodified piecewise.c with HAVE_JWXYZ defined after the framework
 * headers. Upstream defines it where the window system double-buffers for the
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

#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)
#define HAVE_JWXYZ

#define piecewise_free piecewise_free_upstream
#include "piecewise/piecewise.c"
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
