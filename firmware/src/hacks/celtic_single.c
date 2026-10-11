/* Builds celtic.c in single precision, with its decimal literals made floats at
 * build time (tools/float_literals.py). Each animation tick walks every spline's
 * look-ahead point forward in steps of 0.0001 until it is a shadow's width
 * away, so a step is about 33,000 cubic Bezier evaluations, which the
 * ESP32-S3 runs in software double precision. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "screenhack.h"

#include "erase.h"

#include "hacks/single_precision.h"

#define rint(x) rintf(x)
#define floor(x) floorf(x)
#undef M_PI
#define M_PI 3.14159265358979323846f

#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)

#include "celtic_floatlit.c"

/* celtic_init callocs the colour table and celtic_free never frees it. */
static void celtic_free_with_colors(Display *dpy, Window window, void *closure) {
  free(((struct state *)closure)->colors);
  celtic_free(dpy, window, closure);
}

const HackEntry celtic_hack = {"Celtic", celtic_defaults, celtic_init,
                               celtic_draw, celtic_free_with_colors};
