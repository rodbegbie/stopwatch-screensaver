/* Builds the unmodified eruption.c with HAVE_INTTYPES_H: without it the hack
 * typedefs uint32_t as `unsigned long` itself, which clashes with the C
 * library's.
 *
 * It renders at 1/ERUPTION_SCALE of the width and height (hacks/lowrez_put.h):
 * the fire's blur and the conversion to RGB565 walk every pixel of a
 * window-sized buffer in PSRAM each frame, 72 ms of a step at full size.
 * lowrez_put.h takes ordinary RGB565, so the palette is swapped where it comes
 * back from XAllocColor. */
#include <stdint.h>

#include "screenhack.h"

#include "core/canvas.h"
#include "xshm.h"

#ifndef ERUPTION_SCALE
#define ERUPTION_SCALE 2
#endif
#if ERUPTION_SCALE < 1 || ERUPTION_SCALE > 4
#error "ERUPTION_SCALE must be 1 to 4"
#endif
#define LOWREZ_SCALE ERUPTION_SCALE
#include "hacks/lowrez_put.h"

static Status eruption_alloc_color(Display *dpy, Colormap cmap, XColor *c) {
  const Status ok = XAllocColor(dpy, cmap, c);
  c->pixel = px_swap((uint16_t)c->pixel);
  return ok;
}

#define XAllocColor eruption_alloc_color
#define XGetWindowAttributes lowrez_get_attributes
#define put_xshm_image(dpy, d, gc, im, sx, sy, dx, dy, w, h, shm) \
  lowrez_put_image(dpy, d, gc, im, sx, sy, dx, dy, w, h)

#define HAVE_INTTYPES_H 1
#include "eruption/eruption.c"
