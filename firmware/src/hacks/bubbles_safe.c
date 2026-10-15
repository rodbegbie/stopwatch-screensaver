/* Builds the unmodified bubbles.c (which leaks live Bubble lists and one
 * extra Bubble_Step) and reclaims them in a port-side free. */
#include <stdlib.h>

#include "screenhack.h"

#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)
#define bubbles_free bubbles_upstream_free

#include "bubbles/bubbles.c"
#include "bubbles/bubbles-default.c"

#undef bubbles_free

static void bubbles_free(Display *dpy, Window window, void *closure) {
  struct state *st = (struct state *)closure;
  int i;
  if (st->mesh) {
    for (i = 0; i < st->mesh_cells; i++) {
      Bubble *b = st->mesh[i];
      while (b) {
        Bubble *next = b->next;
        free(b);
        b = next;
      }
    }
  }
  if (st->step_pixmaps && st->step_pixmaps[st->num_bubble_pixmaps]) {
    free(st->step_pixmaps[st->num_bubble_pixmaps]);
  }
  bubbles_upstream_free(dpy, window, closure);
}

const HackEntry bubbles_hack = {"Bubbles", bubbles_defaults, bubbles_init,
                                bubbles_draw, bubbles_free, NULL, NULL, 0, 0, 0};
