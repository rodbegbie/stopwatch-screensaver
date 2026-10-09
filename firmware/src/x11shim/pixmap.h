#ifndef X11SHIM_PIXMAP_H
#define X11SHIM_PIXMAP_H

#include <stdint.h>

#include "x11shim/xshim.h"

/* A Pixmap handle is a pointer to one of these. Depth 16 pixmaps use rgb
 * (RGB565, row by row); depth 1 pixmaps use bits (rows of `stride` bytes,
 * most significant bit first). */
struct XshimPixmap {
  int w, h, depth;
  int stride;
  uint16_t *rgb;
  uint8_t *bits;
  /* Links in the display's list of pixmaps a hack holds. Only pixmaps handed
   * to a hack are on it; a GC's copy of its clip mask is not. */
  struct XshimPixmap *prev, *next;
};

/* Frees the GC's copy of its clip mask, if it has one. */
void xshim_gc_release_clip(GC gc);

#endif
