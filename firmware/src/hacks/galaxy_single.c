/* Builds the unmodified galaxy.c in single precision. The ESP32-S3's FPU is
 * single-precision only, so galaxy's per-star `double` maths ran in software
 * at about 150 ms a frame. The framework headers come first so shared structs
 * keep their `double` layout. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xlockmore.h"

#define double float
#define sqrt sqrtf

#include "galaxy/galaxy.c"
