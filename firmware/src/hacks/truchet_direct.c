/* Builds the unmodified truchet.c with its XSCREENSAVER_MODULE expanded
 * emptied, so the HackEntry can be defined here with repaints_all set: every
 * step repaints the whole canvas. */
#include "screenhack.h"

#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)

#include "truchet/truchet.c"

/* Each step repaints the whole image, so the push can overlap the next step. */
const HackEntry truchet_hack = {"Truchet", truchet_defaults, truchet_init,
                                truchet_draw, truchet_free, NULL, NULL,
                                0, 0, 1};
