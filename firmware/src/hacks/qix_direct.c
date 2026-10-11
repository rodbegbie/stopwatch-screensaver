/* Builds the unmodified qix.c and frees what its qix_free forgets: every
 * segment of every qix has its own array of points (`lines[i].p`), and
 * upstream frees `lines` and the qix without them: 4 qixes of 250 segments is
 * 1,000 blocks, about 48 KB lost each time Qix stops. */
#include "screenhack.h"

#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)

#define qix_free qix_free_upstream
#include "qix/qix.c"
#undef qix_free

static void qix_free(Display *dpy, Window window, void *closure) {
  struct state *st = (struct state *)closure;
  for (int i = 0; i < st->count; i++) {
    struct qix *q = st->qixes[i];
    for (int j = 0; j < q->nlines; j++) free(q->lines[j].p);
  }
  qix_free_upstream(dpy, window, closure);
}

const HackEntry qix_hack = {"Qix", qix_defaults, qix_init, qix_draw, qix_free,
                            NULL, NULL};
