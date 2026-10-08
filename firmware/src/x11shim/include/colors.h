/* Shim replacement for the parts of xscreensaver's utils/colors.h that
 * hacks use. */
#ifndef XSHIM_COLORS_H
#define XSHIM_COLORS_H

#include "x11shim/xshim.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Fills colors[0..*ncolorsP-1] with random colours (pixel values are RGB565).
 * *ncolorsP is left unchanged. */
void make_random_colormap(Screen *, Visual *, Colormap, XColor *colors,
                          int *ncolorsP, Bool bright_p, Bool allocate_p,
                          Bool *writable_pP, Bool verbose_p);

#ifdef __cplusplus
}
#endif

#endif
