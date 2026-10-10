/* Builds the unmodified euler2d.c in single precision: the ESP32-S3's FPU is
 * single-precision only, so its `double` maths runs in software. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xlockmore.h"

#include "hacks/single_precision.h"

#undef M_PI
#define M_PI 3.14159265358979323846f

#include "euler2d/euler2d.c"
