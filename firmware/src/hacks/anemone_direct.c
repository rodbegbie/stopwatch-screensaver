/* Builds the unmodified anemone.c in single precision (the ESP32-S3's FPU is
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

#include "anemone_floatlit.c"

/* 128 arms of up to 64 wide segments each took 49 ms a frame in single
 * precision; half the arms halve the drawing. */
static const char *const k_anemone_overrides[] = {"*arms: 64", "*delay: 10000", NULL};

const HackEntry anemone_hack = {"Anemone", anemone_defaults, anemone_init,
                                anemone_draw, anemone_free, NULL, k_anemone_overrides};
