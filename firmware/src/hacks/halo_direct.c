/* Builds the unmodified halo.c with its depth-1 pixmaps backed by colour
 * pixmaps and XCopyPlane replaced by hacks/halo_planes.h: the shim cannot draw
 * into a bitmap or XOR. A colour pixmap costs 434 KB, and Halo makes one or
 * two. */
#include "screenhack.h"

#include "hacks/halo_planes.h"

#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)

#define XCreatePixmap(dpy, d, w, h, depth) (XCreatePixmap)(dpy, d, w, h, 16)
#define XCopyPlane halo_copy_plane

#include "halo/halo.c"

/* Halo asks for 100 ms between frames, which with a 35-110 ms step held it to
 * 5-12 fps; 10 ms leaves the step as the limit. The circles grow faster. */
static const char *const k_halo_overrides[] = {"*delay: 10000", NULL};

const HackEntry halo_hack = {"Halo", halo_defaults, halo_init, halo_draw,
                             halo_free, NULL, k_halo_overrides};
