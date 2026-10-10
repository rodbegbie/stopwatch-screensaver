/* Builds the unmodified halftone.c with HAVE_JWXYZ defined after the framework
 * headers. Upstream defines it where the window system double-buffers for the
 * hack, and the hack then draws straight on the window. The shim draws only on
 * the canvas, so drawing into a pixmap and copying it over would erase the
 * picture. */
#include "screenhack.h"

#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)
#define HAVE_JWXYZ

#include "halftone/halftone.c"

const HackEntry halftone_hack = {"Halftone", halftone_defaults, halftone_init,
                                halftone_draw, halftone_free, NULL, NULL};
