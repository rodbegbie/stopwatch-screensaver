/* Builds the unmodified interaggregate.c in single precision (the ESP32-S3's FPU is
 * single-precision only, so `double` maths runs in software) and
 * with its decimal literals made floats at build time
 * (tools/float_literals.py), or `0.5 * x` would stay software double, with HAVE_JWXYZ
 * defined after the framework headers. Upstream defines it where the window
 * system double-buffers for the hack, and the hack then draws straight on the
 * window. The shim draws only on the canvas, so drawing into a pixmap and
 * copying it over would erase the picture. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "screenhack.h"

#include "core/canvas.h"
#include "hacks/single_precision.h"

/* Interaggregate's alpha blend takes the red, green and blue bits out of
 * pixel values itself (point2rgb), assuming ordinary RGB565. The canvas holds
 * the bytes swapped, so swap where a colour comes back from XAllocColor and
 * where it goes into XSetForeground, which leaves the hack working in ordinary
 * RGB565. Its own foreground and background are black and white, which are the
 * same in either order, so get_pixel_resource needs no wrapper unless those
 * defaults change. */
static Status interaggregate_alloc_color(Display *dpy, Colormap cmap, XColor *c) {
  Status ok = XAllocColor(dpy, cmap, c);
  c->pixel = px_swap((uint16_t)c->pixel);
  return ok;
}
#define XAllocColor interaggregate_alloc_color
#define XSetForeground(dpy, gc, pixel) \
  XSetForeground(dpy, gc, px_swap((uint16_t)(pixel)))

#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)
#define HAVE_JWXYZ
#undef M_PI
#define M_PI 3.14159265358979323846f

#include "interaggregate_floatlit.c"

const HackEntry interaggregate_hack = {"Interaggregate", interaggregate_defaults, interaggregate_init,
                                interaggregate_draw, interaggregate_free, NULL, NULL};
