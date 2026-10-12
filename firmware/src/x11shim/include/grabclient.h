/* Shim replacement for xscreensaver's utils/grabclient.h. Not derived from
 * xscreensaver code.
 *
 * Upstream loads a picture of the desktop or a random image file into a
 * drawable, a little at a time. The device has neither, so loading finishes at
 * once with nothing loaded: Boxfit's optional `grab` mode would then colour its
 * boxes from the window as it stands. */
#ifndef XSHIM_GRABCLIENT_H
#define XSHIM_GRABCLIENT_H

#include "x11shim/xshim.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct async_load_state async_load_state;

/* Returns NULL (finished) at once and draws nothing; a file name asked for is
 * NULL and a geometry asked for is all zero. */
async_load_state *load_image_async_simple(async_load_state *, Screen *,
                                          Window top_level, Drawable target,
                                          char **filename_ret,
                                          XRectangle *geometry_ret);

#ifdef __cplusplus
}
#endif

#endif
