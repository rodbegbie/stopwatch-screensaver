/* Builds the unmodified blaster.c with a free that is safe before the first
 * draw. The hack sets NUM_ROBOTS in init but allocates `robots` on its first
 * draw, and its free loops NUM_ROBOTS times over `robots`, so freeing it first
 * dereferences NULL. Two quick button presses can do that. */
#include "screenhack.h"

/* blaster.c registers itself at its end; this file registers it instead. */
#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)

#include "blaster/blaster.c"

static void blaster_free_safe(Display *dpy, Window window, void *closure) {
  struct state *st = (struct state *)closure;
  if (!st->robots) st->NUM_ROBOTS = 0;
  blaster_free(dpy, window, closure);
}

const HackEntry blaster_hack = {"Blaster", blaster_defaults, blaster_init,
                                blaster_draw, blaster_free_safe};
