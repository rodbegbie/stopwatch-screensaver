#ifndef HACKS_HALO_PLANES_H
#define HACKS_HALO_PLANES_H

#include <stdint.h>

#include "core/canvas.h"
#include "x11shim/pixmap.h"
#include "x11shim/xshim.h"

/* Halo draws circles into depth-1 pixmaps and merges them with XCopyPlane. The
 * shim cannot draw into a bitmap, so its wrapper makes every pixmap a colour
 * pixmap and treats a pixel of 0 as a clear bit and anything else as a set one
 * (the hack paints them with foreground 1 and 0).
 *
 * To the window, set bits take the GC's foreground and clear bits its
 * background, as XCopyPlane does. To another pixmap the GC is the hack's
 * GXxor one, so the source's bits are flipped into it: the shim ignores the GC
 * function, and Halo copies to a pixmap only for that. */
static void halo_copy_plane(Display *dpy, Drawable src, Drawable dst, GC gc,
                            int sx, int sy, unsigned int w, unsigned int h,
                            int dx, int dy, unsigned long plane) {
  (void)plane;
  Canvas *const s = xshim_target(dpy, src);
  Canvas *const d = xshim_target(dpy, dst);
  if (!s || !d) return;
  const int to_window = d == dpy->canvas;
  const int64_t i0 = sx < 0 ? -(int64_t)sx : 0;
  const int64_t j0 = sy < 0 ? -(int64_t)sy : 0;
  int64_t i1 = (int64_t)s->w - sx, j1 = (int64_t)s->h - sy;
  if (i1 > (int64_t)w) i1 = w;
  if (j1 > (int64_t)h) j1 = h;
  const int64_t di0 = dx < 0 ? -(int64_t)dx : 0;
  const int64_t dj0 = dy < 0 ? -(int64_t)dy : 0;
  const int64_t a0 = i0 > di0 ? i0 : di0, b0 = j0 > dj0 ? j0 : dj0;
  int64_t a1 = i1, b1 = j1;
  if (a1 > (int64_t)d->w - dx) a1 = (int64_t)d->w - dx;
  if (b1 > (int64_t)d->h - dy) b1 = (int64_t)d->h - dy;
  if (a1 <= a0 || b1 <= b0) return;
  const uint16_t fg = (uint16_t)gc->foreground, bg = (uint16_t)gc->background;
  for (int64_t j = b0; j < b1; j++) {
    const uint16_t *in = s->px + (sy + j) * s->w + sx + a0;
    uint16_t *out = d->px + (dy + j) * d->w + dx + a0;
    if (to_window)
      for (int64_t i = a0; i < a1; i++, in++, out++) *out = *in ? fg : bg;
    else
      for (int64_t i = a0; i < a1; i++, in++, out++) *out ^= *in ? 1 : 0;
  }
  if (to_window) canvas_mark_dirty(d, (int)(dx + a0), (int)(dy + b0), (int)(a1 - a0), (int)(b1 - b0));
}

#endif
