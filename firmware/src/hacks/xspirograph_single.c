/* Builds xspirograph.c in single precision: `go` runs sin and cos twice per
 * line, 1000 lines a frame, and in double those are software calls. Its
 * angles are reduced mod 360 first, so a float holds them
 * (tools/xspirograph_patch.py), its decimal literals are made floats at build
 * time (tools/float_literals.py) and M_PI is a float (single_constants.h). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "screenhack.h"
#include "erase.h"

#include "hacks/single_precision.h"
#include "hacks/single_constants.h"
#include "hacks/single_frand.h"

#include "xspirograph_floatlit.c"
