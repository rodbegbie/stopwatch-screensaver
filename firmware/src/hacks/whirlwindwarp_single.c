/* Builds whirlwindwarp.c in single precision: its star maths mixes `double`
 * and `float`, so every step widens to software double on the ESP32-S3. Its
 * decimal literals are made floats at build time (tools/float_literals.py)
 * and M_PI is a float (single_constants.h). */
#include <math.h>

#include "screenhack.h"
#include "erase.h"
#include "hsv.h"

#include "hacks/single_precision.h"
#include "hacks/single_constants.h"

#include "whirlwindwarp_floatlit.c"
