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

/* A closed loop of colours that shift smoothly (the last blends back into the
 * first), so cycling through them has no jumps. Same arguments and
 * conventions as make_random_colormap; the colours are ours, not xscreensaver's. */
void make_smooth_colormap(Screen *, Visual *, Colormap, XColor *colors,
                          int *ncolorsP, Bool allocate_p, Bool *writable_pP,
                          Bool verbose_p);
/* Fully saturated colours with evenly spaced hues. */
void make_uniform_colormap(Screen *, Visual *, Colormap, XColor *colors,
                           int *ncolorsP, Bool allocate_p, Bool *writable_pP,
                           Bool verbose_p);
/* Nothing to release: colours are plain RGB565 values, not shared cells. */
void free_colors(Screen *, Colormap, XColor *colors, int ncolors);

#ifdef __cplusplus
}
#endif

#endif
