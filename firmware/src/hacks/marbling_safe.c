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

const HackEntry marbling_hack = {"Marbling", marbling_defaults, marbling_init,
                                 marbling_draw, marbling_free, NULL, NULL};
