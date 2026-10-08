/* Shim replacement for xscreensaver's utils/ximage-loader.h. Not derived from
 * xscreensaver code.
 *
 * Upstream decodes a PNG. The device has no decoder, so the "image data" here
 * is a raw blob made on the Mac by tools/make_logo_blob.py:
 *
 *   bytes 0-3          the characters "565M"
 *   bytes 4-5          width, unsigned 16-bit little-endian
 *   bytes 6-7          height, unsigned 16-bit little-endian
 *   next width*height  pixels, row by row, each RGB565 as 16-bit little-endian
 *   remainder          mask, row by row, each row padded to whole bytes,
 *                      most significant bit first; 1 = draw the pixel
 *
 * The blob size must match its width and height exactly. */
#ifndef XSHIM_XIMAGE_LOADER_H
#define XSHIM_XIMAGE_LOADER_H

#include "x11shim/xshim.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Returns a colour pixmap, and writes its size and a depth-1 mask pixmap
 * through the pointers. Returns None (and writes zeros and None) when the
 * blob is not valid or memory runs out. The caller frees both pixmaps. */
Pixmap image_data_to_pixmap(Display *, Window, const unsigned char *data,
                            unsigned long size, int *width_ret,
                            int *height_ret, Pixmap *mask_ret);

#ifdef __cplusplus
}
#endif

#endif
