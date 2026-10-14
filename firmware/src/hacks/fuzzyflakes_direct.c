/* Builds the unmodified fuzzyflakes.c in single precision (the ESP32-S3's FPU is
 * single-precision only, so `double` maths runs in software) and
 * with its decimal literals made floats at build time
 * (tools/float_literals.py), or `0.5 * x` would stay software double, with HAVE_JWXYZ
 * defined after the framework headers. Upstream defines it where the window
 * system double-buffers for the hack, and the hack then draws straight on the
 * window. The shim can draw into pixmaps, but a back buffer would cost 434 KB
 * and a full-frame copy each frame, and would dirty every row. */
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

#include "fuzzyflakes_floatlit.c"

static const char *const k_fuzzyflakes_overrides[] = {"*density: 3", NULL};

const HackEntry fuzzyflakes_hack = {"FuzzyFlakes", fuzzyflakes_defaults, fuzzyflakes_init,
                                fuzzyflakes_draw, fuzzyflakes_free, NULL, k_fuzzyflakes_overrides, 0, 0, 1};
