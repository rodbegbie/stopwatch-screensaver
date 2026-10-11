/* Builds the unmodified moire2.c with its depth-1 pixmaps as packed bit planes
 * and the calls that draw on them replaced by hacks/moire2_planes.h. The shim
 * draws nothing into a bitmap and ignores a GC's function, and Moire2 draws
 * outline circles into four, merges three with OR or XOR into a fifth and
 * expands that onto the window. Planes are 27 KB (window size) or 109 KB
 * (twice the width and height) apiece. */
#include "screenhack.h"

#include "hacks/moire2_planes.h"

#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)

#define XCreatePixmap halo_create_pixmap
#define XFreePixmap halo_free_pixmap
#define XFillRectangle moire2_fill_rectangle
#define XDrawArc moire2_draw_arc
#define XCopyArea moire2_copy_area
#define XCopyPlane moire2_copy_plane

#include "moire2/moire2.c"

/* Upstream's free leaves its pixmaps to the X server. The runner only frees
 * the shim's own pixmaps, so the planes are freed here. */
static void moire2_free_with_planes(Display *dpy, Window window,
                                    void *closure) {
  struct state *st = (struct state *)closure;
  halo_free_pixmap(dpy, st->p0);
  halo_free_pixmap(dpy, st->p1);
  halo_free_pixmap(dpy, st->p2);
  halo_free_pixmap(dpy, st->p3);
  moire2_free(dpy, window, closure);
}

/* Moire2 asks for 50 ms between frames, which the board spends waiting for
 * all but its 28 ms step: 12.6 fps against 16.6 with the wait gone. Upstream's
 * pace was 20 fps at most, so this is closer to it, not further. */
static const char *const k_moire2_overrides[] = {"*delay: 10000", NULL};

const HackEntry moire2_hack = {"Moire2", moire2_defaults, moire2_init,
                               moire2_draw, moire2_free_with_planes, NULL,
                               k_moire2_overrides};
