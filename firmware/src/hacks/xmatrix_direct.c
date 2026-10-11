/* Builds the unmodified xmatrix.c and registers it with `trace` off. With it
 * on, xmatrix opens with a typed "connecting" sequence (a modem trace) before
 * the rain starts; a screensaver on a badge wants the rain at once. */
#include "screenhack.h"

#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)

#include "xmatrix/xmatrix.c"

static const char *const k_xmatrix_overrides[] = {"*trace: False", NULL};

const HackEntry xmatrix_hack = {"XMatrix", xmatrix_defaults, xmatrix_init,
                                xmatrix_draw, xmatrix_free, NULL,
                                k_xmatrix_overrides};
