/* Builds hopalong.c in single precision: its orbit maths is `double`, which
 * runs in software on the ESP32-S3. Its decimal literals are made floats at
 * build time (tools/float_literals.py), and MAXRAND and M_PI are floats
 * (single_constants.h). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xlockmore.h"

#include "hacks/single_precision.h"
#include "hacks/single_constants.h"

#include "hopalong_floatlit.c"
