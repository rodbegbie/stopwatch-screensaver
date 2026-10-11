/* Stand-in for xscreensaver's utils/xshm.h. There is no X server to share
 * memory with: an "shm" image is an ordinary 16-bit XImage in the hack's own
 * memory, and putting it is XPutImage. Not derived from xscreensaver code. */
#ifndef XSHIM_XSHM_H
#define XSHIM_XSHM_H

#include "visual.h"
#include "x11shim/xshim.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Holds nothing: the image owns its memory. */
typedef struct {
  int unused;
} XShmSegmentInfo;

/* A zeroed w by h image of the given depth, or NULL for a depth XCreateImage
 * does not make or no memory. Free it with destroy_xshm_image. */
XImage *create_xshm_image(Display *, Visual *, unsigned int depth, int format,
                          XShmSegmentInfo *, unsigned int w, unsigned int h);

/* XPutImage of the region, clipped to the drawable. */
void put_xshm_image(Display *, Drawable, GC, XImage *, int src_x, int src_y,
                    int dest_x, int dest_y, unsigned int w, unsigned int h,
                    XShmSegmentInfo *);

void destroy_xshm_image(Display *, XImage *, XShmSegmentInfo *);

#ifdef __cplusplus
}
#endif

#endif
