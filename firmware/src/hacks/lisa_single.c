/* Builds the unmodified lisa.c in single precision (the ESP32-S3's FPU is
 * single-precision only, so `double` maths runs in software), with its decimal
 * literals made floats at build time (tools/float_literals.py). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xlockmore.h"

#include "hacks/single_precision.h"

#include "lisa_floatlit.c"
