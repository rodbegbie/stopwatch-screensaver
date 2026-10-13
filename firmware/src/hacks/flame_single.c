/* Builds flame.c in single precision, with its decimal literals made floats at
 * build time (tools/float_literals.py) and MAXRAND and M_PI made floats too
 * (single_constants.h). */
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "screenhack.h"

#include "hacks/single_precision.h"
#include "hacks/single_constants.h"
#include "hacks/single_frand.h"

#include "flame_floatlit.c"
