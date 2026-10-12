#ifndef X11SHIM_PIXMAP_H
#define X11SHIM_PIXMAP_H

#include <stdint.h>

#include "x11shim/xshim.h"

/* A Pixmap handle is a pointer to one of these. Depth 16 pixmaps are a
 * canvas, so the drawing primitives can draw into them; depth 1 pixmaps use
 * bits (rows of `stride` bytes, most significant bit first). */
struct XshimPixmap {
  int w, h, depth;
  int stride;
  Canvas canvas;
  uint8_t *bits;
  /* Links in the display's list of pixmaps a hack holds. Only pixmaps handed
   * to a hack are on it; a GC's copy of its clip mask is not. */
  struct XshimPixmap *prev, *next;
};

/* Frees the GC's copy of its clip mask, if it has one. */
void xshim_gc_release_clip(GC gc);

/* The canvas a primitive draws on: the display's for the window, the
 * pixmap's for a colour pixmap, NULL (draw nothing) for a bitmap or None. */
Canvas *xshim_target(Display *dpy, Drawable d);

/* Fills on a bitmap (a depth 1 pixmap), setting bits where the GC's
 * foreground is odd and clearing them where it is even; the clip mask and the
 * GC's function are ignored. Each returns 1 if `d` is a bitmap, so the caller
 * knows it was handled, and 0 if not (nothing is drawn). */
int xshim_bitmap_fill_rect(Display *, Drawable, GC, int x, int y,
                           unsigned int w, unsigned int h);
int xshim_bitmap_fill_polygon(Display *, Drawable, GC, const int *xy, int n);

#endif
