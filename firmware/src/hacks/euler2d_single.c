/* Builds the unmodified euler2d.c in single precision: the ESP32-S3's FPU is
 * single-precision only, so its `double` maths runs in software. Its decimal
 * literals are made floats at build time (tools/float_literals.py), or
 * `0.5 * x` would stay software double. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xlockmore.h"

#include "hacks/single_precision.h"

/* single_precision.h leaves floor and ceil on the double versions. */
#define floor(x) floorf(x)
#define ceil(x) ceilf(x)

#undef M_PI
#define M_PI 3.14159265358979323846f

#include "euler2d_floatlit.c"
