/* Builds the unmodified substrate.c in single precision. Its sand painter
 * calls sin() four times per grain, in double, and the ESP32-S3 emulates
 * double in software: the step grew from 5 ms to 121 ms as the picture filled.
 * The header turns the calls into sinf and cosf; M_PI is a double literal, so
 * it is made a float one as well, or every angle would still be computed in
 * double. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "screenhack.h"

#include "core/canvas.h"
#include "hacks/single_precision.h"

/* Substrate's alpha blend takes the red, green and blue bits out of pixel
 * values itself (point2rgb), assuming ordinary RGB565. The canvas holds the
 * bytes swapped, so swap where a colour comes back from XAllocColor and where
 * it goes into XSetForeground, which leaves Substrate working in ordinary
 * RGB565. Its own foreground and background are black and white, which are the
 * same in either order, so get_pixel_resource needs no wrapper unless those
 * defaults change. */
static Status substrate_alloc_color(Display *dpy, Colormap cmap, XColor *c) {
  Status ok = XAllocColor(dpy, cmap, c);
  c->pixel = px_swap((uint16_t)c->pixel);
  return ok;
}
#define XAllocColor substrate_alloc_color
#define XSetForeground(dpy, gc, pixel) \
  XSetForeground(dpy, gc, px_swap((uint16_t)(pixel)))

#undef M_PI
#define M_PI 3.14159265358979323846f

#include "substrate/substrate.c"
