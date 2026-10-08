/* Builds the unmodified galaxy.c in single precision (about 150 ms a frame in
 * double, 40 ms in float). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xlockmore.h"

#include "hacks/single_precision.h"

#include "galaxy/galaxy.c"
