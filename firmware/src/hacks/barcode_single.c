/* Builds barcode.c in single precision: its `FLOAT` is `double`, which runs
 * in software on the ESP32-S3. Its decimal literals are made floats at build
 * time (tools/float_literals.py) and M_PI is a float (single_constants.h). */
#include <math.h>
#include <time.h>

#include "screenhack.h"
#include "utf8wc.h"

#include "hacks/single_precision.h"
#include "hacks/single_constants.h"

#include "barcode_floatlit.c"
