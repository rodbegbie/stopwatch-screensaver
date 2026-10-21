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
 * The blob size must match its width and height exactly.
 *
 * A "565Z" blob (make_logo_blob.py --deflate) has the same eight-byte header,
 * then the pixels and mask as one zlib stream, which must unpack to exactly
 * that size. It is inflated into a temporary buffer while the image loads. */
#ifndef XSHIM_XIMAGE_LOADER_H
#define XSHIM_XIMAGE_LOADER_H

#include "x11shim/xshim.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Returns a colour pixmap, and writes its size and a depth-1 mask pixmap
 * through the pointers. Returns None (and writes zeros and None) when the
 * blob is not valid or memory runs out. The caller frees both pixmaps. Any of
 * the three pointers may be NULL (xmatrix passes no mask, and none is made). */
Pixmap image_data_to_pixmap(Display *, Window, const unsigned char *data,
                            unsigned long size, int *width_ret,
                            int *height_ret, Pixmap *mask_ret);

/* The image as a 32-bit XImage of RGBA bytes (alpha 255 where the mask is set,
 * 0 where not), upside down, row 0 being the bottom, as upstream gives it for
 * glTexImage2D. NULL for a bad blob or no memory; free it with XDestroyImage. */
XImage *image_data_to_ximage(Display *, Visual *, const unsigned char *data,
                             unsigned long size);

/* Upstream reads an image file. The device has no filesystem for hacks and no
 * decoder, so this always returns NULL, and a hack that loads an optional
 * picture carries on without one. */
XImage *file_to_ximage(Display *, Visual *, const char *filename);

/* The same, as a pixmap (and its mask): always None, with zero size, and an
 * optional mask pointer set to None. */
Pixmap file_to_pixmap(Display *, Window, const char *filename, int *width_ret,
                      int *height_ret, Pixmap *mask_ret);

#ifdef __cplusplus
}
#endif

#endif
