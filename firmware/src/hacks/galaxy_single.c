/* Builds galaxy.c in single precision (about 150 ms a frame in double, 40 ms in
 * float), with its decimal literals made floats at build time
 * (tools/float_literals.py) and MAXRAND and M_PI made floats too
 * (single_constants.h). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xlockmore.h"

#include "hacks/single_precision.h"
#include "hacks/single_constants.h"

#include "galaxy_floatlit.c"
