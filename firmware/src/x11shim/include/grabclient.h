/* Shim replacement for xscreensaver's utils/grabclient.h. Not derived from
 * xscreensaver code.
 *
 * Upstream loads a picture of the desktop or a random image file into a
 * drawable, a little at a time. The device has neither, so loading finishes at
 * once. With no picture set (xshim_set_grab_image) nothing is loaded and
 * Boxfit's optional `grab` mode would colour its boxes from the window as it
 * stands; with one, the picture is painted over the target first. */
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

/* Gives loading a picture to put up: an image blob as described in
 * ximage-loader.h, which must outlive its use. The next load that starts
 * (state NULL) paints it over its target, scaled to cover and cropped evenly
 * at the sides, as a grabbed screen would be; NULL (the default) goes back to
 * loading nothing. */
void xshim_set_grab_image(const unsigned char *blob, unsigned long size);

#ifdef __cplusplus
}
#endif

#endif
