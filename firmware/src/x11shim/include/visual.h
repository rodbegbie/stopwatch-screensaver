/* Stand-in for the part of xscreensaver's utils/visual.h that hacks use. Not
 * derived from xscreensaver code. */
#ifndef XSHIM_VISUAL_H
#define XSHIM_VISUAL_H

#include "x11shim/xshim.h"

#ifdef __cplusplus
extern "C" {
#endif

/* X's visual classes. The display is always TrueColor. */
#define StaticGray 0
#define GrayScale 1
#define StaticColor 2
#define PseudoColor 3
#define TrueColor 4
#define DirectColor 5
int visual_class(Screen *, Visual *);
/* Ordinary RGB565: 0xF800, 0x07E0 and 0x001F. The canvas holds the bytes of
 * each pixel swapped, so a hack that packs pixels from these masks (analogtv)
 * must swap what it packs. */
void visual_rgb_masks(Screen *, Visual *, unsigned long *red_mask,
                      unsigned long *green_mask, unsigned long *blue_mask);
/* The display is always 16-bit RGB565, whatever visual is asked about. */
int visual_depth(Screen *, Visual *);
/* Bits per pixel of a pixmap at that depth: also 16. */
int visual_pixmap_depth(Screen *, Visual *);

#ifdef __cplusplus
}
#endif

#endif
