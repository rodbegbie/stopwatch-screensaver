/* Builds the unmodified lmorph.c in single precision (the ESP32-S3's FPU is
 * single-precision only, so `double` maths runs in software), with its decimal
 * literals made floats at build time (tools/float_literals.py), or `0.5 * x`
 * would stay software double. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "screenhack.h"

#include "hacks/single_precision.h"

#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)
#undef M_PI
#define M_PI 3.14159265358979323846f

#include "lmorph_floatlit.c"

/* Its 70 ms pause between frames is 14 fps by design; the step and the push
 * already take that long on the board. */
static const char *const k_lmorph_overrides[] = {"*delay: 10000", NULL};

const HackEntry lmorph_hack = {"LMorph", lmorph_defaults, lmorph_init,
                                lmorph_draw, lmorph_free, NULL, k_lmorph_overrides, 0, 0, 1};
