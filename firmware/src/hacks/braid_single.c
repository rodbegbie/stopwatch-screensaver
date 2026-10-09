/* Builds the unmodified braid.c in single precision. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xlockmore.h"

#include "hacks/single_precision.h"

#include "hacks/fast_trig.h"
#undef sin
#undef cos
#define sin(x) fast_sinf(x)
#define cos(x) fast_cosf(x)

#include "braid/braid.c"
