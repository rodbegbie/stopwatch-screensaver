/* Stand-in for the part of xscreensaver's utils/visual.h that hacks use. Not
 * derived from xscreensaver code. */
#ifndef XSHIM_VISUAL_H
#define XSHIM_VISUAL_H

#include "x11shim/xshim.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The display is always 16-bit RGB565, whatever visual is asked about. */
int visual_depth(Screen *, Visual *);
/* Bits per pixel of a pixmap at that depth: also 16. */
int visual_pixmap_depth(Screen *, Visual *);

#ifdef __cplusplus
}
#endif

#endif
