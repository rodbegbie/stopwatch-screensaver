/* Builds the unmodified flow.c in single precision (the ESP32-S3's FPU is
 * single-precision only, so `double` maths runs in software, and the hack
 * iterates a strange attractor in double for every bee each frame) and with its
 * decimal literals made floats at build time (tools/float_literals.py). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xlockmore.h"

#include "hacks/single_precision.h"
#undef M_PI
#define M_PI 3.14159265358979323846f

#include "flow_floatlit.c"
