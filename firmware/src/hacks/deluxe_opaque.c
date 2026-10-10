/* Builds the unmodified deluxe.c with resources that suit the shim. Its
 * `transparent` default draws through colour-plane masks, which an RGB565
 * canvas cannot do, and its `doubleBuffer` default draws into pixmaps, but
 * the shim only draws on the canvas, so the copy to the window would erase
 * the picture. */
#include "screenhack.h"

#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)

#include "deluxe/deluxe.c"

static const char *const kDeluxeOverrides[] = {"*transparent: False",
                                               "*doubleBuffer: False", NULL};

const HackEntry deluxe_hack = {"Deluxe", deluxe_defaults, deluxe_init,
                               deluxe_draw, deluxe_free, NULL,
                               kDeluxeOverrides};
