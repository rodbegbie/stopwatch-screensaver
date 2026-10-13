/* Builds the unmodified marbling.c and frees what its free forgets: the colour
 * table that marbling_recolor callocs (5 KB each start). The restart leak test
 * found it. */
#include <stdlib.h>

#include "screenhack.h"

#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)
#define marbling_free marbling_upstream_free

#include "marbling/marbling.c"

#undef marbling_free

static void marbling_free(Display *dpy, Window window, void *closure) {
  XColor *colors = ((struct state *)closure)->colors;
  marbling_upstream_free(dpy, window, closure);
  free(colors);
}

/* Upstream's gridsize 2 and 5 noise iterations took 1,785 ms a frame on the
 * board (0.6 fps). The cost goes with 1 / gridsize squared and with the
 * iterations: 6 and 3 are cells of 6 pixels. */
static const char *const marbling_overrides[] = {"*gridsize: 6", "*iterations: 3", NULL};

const HackEntry marbling_hack = {"Marbling", marbling_defaults, marbling_init,
                                 marbling_draw, marbling_free, NULL,
                                 marbling_overrides};
