/* Shim replacement for xscreensaver's utils/grab-ximage.h. Not derived from
 * xscreensaver code.
 *
 * Upstream grabs a picture of the desktop or a random image to use as a
 * texture. The device has neither, so nothing is declared: this header exists
 * so a hack that includes it for an optional picture still compiles. */
#ifndef XSHIM_GRAB_XIMAGE_H
#define XSHIM_GRAB_XIMAGE_H

#include "x11shim/xshim.h"

#endif
