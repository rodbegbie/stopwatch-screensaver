/* Builds the unmodified anemotaxis.c with HAVE_JWXYZ defined after the framework
 * headers. Upstream defines it where the window system double-buffers for the
 * hack, and the hack then draws straight on the window. The shim can draw
 * into pixmaps, but a back buffer would cost 434 KB and a full-frame copy each
 * frame, and would dirty every row. */
#include "screenhack.h"

#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)
#define HAVE_JWXYZ

#include "anemotaxis/anemotaxis.c"

const HackEntry anemotaxis_hack = {"Anemotaxis", anemotaxis_defaults, anemotaxis_init,
                                anemotaxis_draw, anemotaxis_free, NULL, NULL, 0, 0, 1};
