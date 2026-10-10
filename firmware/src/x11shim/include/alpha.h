/* Shim replacement for xscreensaver's utils/alpha.h. */
#ifndef XSHIM_ALPHA_H
#define XSHIM_ALPHA_H

#include "x11shim/xshim.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The canvas is RGB565 with no spare bits for colour planes, so this always
 * finds none: *nplanesP is set to 0 and *plane_masks to NULL. A hack that
 * asked for transparency then falls back to opaque colours. */
void allocate_alpha_colors(Screen *, Visual *, Colormap, int *nplanesP,
                           Bool additive_p, unsigned long **plane_masks,
                           unsigned long *base_pixelP);

#ifdef __cplusplus
}
#endif

#endif
