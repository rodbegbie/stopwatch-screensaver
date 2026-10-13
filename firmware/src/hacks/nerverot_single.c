/* Builds the unmodified nerverot.c in single precision (the ESP32-S3's FPU is
 * single-precision only, so `double` maths runs in software, and the hack
 * rotates and scales every blot in double each frame) and with its decimal
 * literals made floats at build time (tools/float_literals.py). */
#include <math.h>
#include <stdlib.h>

#include "screenhack.h"

#include "hacks/single_precision.h"
#undef M_PI
#define M_PI 3.14159265358979323846f

#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)

#include "nerverot_floatlit.c"

const HackEntry nerverot_hack = {"NerveRot", nerverot_defaults, nerverot_init,
                                 nerverot_draw, nerverot_free, NULL, NULL};
