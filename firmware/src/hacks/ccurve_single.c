/* Builds the unmodified ccurve.c in single precision (the ESP32-S3's FPU is
 * single-precision only, so `double` maths runs in software, and the hack's
 * recursion takes the sine and cosine of every segment in double) and with its
 * decimal literals made floats at build time (tools/float_literals.py). The
 * step reached 2.9 s as the curve deepened, as copied. */
#include <math.h>
#include <stdlib.h>

#include "screenhack.h"

#include "hacks/single_precision.h"
#undef M_PI
#define M_PI 3.14159265358979323846f
#undef M_PI_2
#define M_PI_2 1.57079632679489661923f
#undef M_PI_4
#define M_PI_4 0.78539816339744830962f
#undef M_SQRT2
#define M_SQRT2 1.41421356237309504880f

#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)

#include "ccurve_floatlit.c"

/* Each frame redraws the curve one iteration deeper, up to `limit` segments,
 * every one with a malloc and a recursion: 200,000 took 740 ms in single
 * precision. 30,000 is under 110 ms and still reads as a curve at 466 pixels. */
static const char *const ccurve_overrides[] = {"*limit: 30000", NULL};

const HackEntry ccurve_hack = {"CCurve", ccurve_defaults, ccurve_init,
                               ccurve_draw, ccurve_free, NULL, ccurve_overrides};
