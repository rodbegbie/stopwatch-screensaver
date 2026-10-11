/* Builds the unmodified starfish.c in single precision (the ESP32-S3's FPU is
 * single-precision only, so `double` maths runs in software), with its decimal
 * literals made floats at build time (tools/float_literals.py), or `0.5 * x`
 * would stay software double. Its polygon is the new outline and the old one, a
 * ring a few hundred edges round, which the shim fills in double a row at a
 * time: hacks/float_polygon.h does it in float. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "screenhack.h"

/* spline.h must come in before `double` becomes `float`: the hack includes it
 * too, but the include guard then keeps this copy, and spline.c is built with
 * doubles. Seen through the macro, the control arrays are float*, spline.c
 * reads them as double*, and every blob becomes garbage. */
#include "spline.h"
#include "hacks/float_polygon.h"
#include "hacks/single_precision.h"
#include "hacks/single_frand.h"

#define XFillPolygon float_x_fill_polygon

#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)
#undef M_PI
#define M_PI 3.14159265358979323846f

#include "starfish_floatlit.c"

const HackEntry starfish_hack = {"Starfish", starfish_defaults, starfish_init,
                                 starfish_draw, starfish_free, NULL, NULL};
