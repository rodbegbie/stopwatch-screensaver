/* Builds the unmodified compass.c in single precision (the ESP32-S3's FPU is
 * single-precision only, so `double` maths runs in software), with HAVE_JWXYZ
 * defined after the framework headers. Upstream defines it where the window
 * system double-buffers for the hack, and the hack then draws straight on the
 * window. The shim draws only on the canvas, so drawing into a pixmap and
 * copying it over would erase the picture. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "screenhack.h"

#include "hacks/single_precision.h"

#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)
#define HAVE_JWXYZ
#undef M_PI
#define M_PI 3.14159265358979323846f

#include "compass/compass.c"

const HackEntry compass_hack = {"Compass", compass_defaults, compass_init,
                                compass_draw, compass_free, NULL, NULL};
