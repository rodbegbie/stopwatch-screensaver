/* Builds braid.c in single precision: its decimal literals are made floats at
 * build time (tools/float_literals.py), because `double` literals would keep
 * the arithmetic in software double precision. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xlockmore.h"

#include "hacks/single_precision.h"

#include "hacks/fast_trig.h"
#undef sin
#undef cos
#define sin(x) fast_sinf(x)
#define cos(x) fast_cosf(x)

/* math.h defines these as doubles. */
#undef M_PI
#undef M_PI_2
#define M_PI 3.14159265358979323846f
#define M_PI_2 1.57079632679489661923f

#include "braid_floatlit.c"
