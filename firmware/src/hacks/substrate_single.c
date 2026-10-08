/* Builds the unmodified substrate.c in single precision. Its sand painter
 * calls sin() four times per grain, in double, and the ESP32-S3 emulates
 * double in software: the step grew from 5 ms to 121 ms as the picture filled.
 * The header turns the calls into sinf and cosf; M_PI is a double literal, so
 * it is made a float one as well, or every angle would still be computed in
 * double. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "screenhack.h"

#include "hacks/single_precision.h"

#undef M_PI
#define M_PI 3.14159265358979323846f

#include "substrate/substrate.c"
