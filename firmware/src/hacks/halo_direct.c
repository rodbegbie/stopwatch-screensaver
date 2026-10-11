/* Builds the unmodified halo.c with its depth-1 pixmaps as packed bit planes
 * and the calls that draw on them replaced by hacks/halo_planes.h: the shim
 * cannot draw into a bitmap or XOR, and Halo fills a dozen window-sized circles
 * a frame, which as 16-bit pixmaps cost several million PSRAM writes. */
#include "screenhack.h"

#include "hacks/halo_planes.h"

#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)

#define XCreatePixmap halo_create_pixmap
#define XFreePixmap halo_free_pixmap
#define XFillRectangle halo_fill_rectangle
#define XFillArc halo_fill_arc
#define XCopyPlane halo_copy_plane

#include "halo/halo.c"

/* Halo asks for 100 ms between frames, which with a 35-110 ms step held it to
 * 5-12 fps; 10 ms leaves the step as the limit. The circles grow faster. */
static const char *const k_halo_overrides[] = {"*delay: 10000", NULL};

const HackEntry halo_hack = {"Halo", halo_defaults, halo_init, halo_draw,
                             halo_free, NULL, k_halo_overrides};
