/* Builds hypnowheel.c with USE_GL set for this file only (see hacks/gears_gl.c),
 * in single precision. Each vertex of each spiral arm calls `cos` and `sin` in
 * `double`, which the ESP32-S3's FPU does in software: about 275 ms of its
 * 706 ms step on the board (issue #43).
 *
 * It passes the addresses of `double` locals to get_position and
 * get_rotation, which take `double *`: as floats they would be written eight
 * bytes (see moire_single.c). The adapters come before single_precision.h, so
 * their `double` is the real one. */
#define USE_GL
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xlockmore.h"
#include "colors.h"
#include "rotator.h"

static void hypnowheel_get_position(rotator *rot, float *x, float *y, float *z,
                                    Bool update_p) {
  double dx, dy, dz;
  get_position(rot, &dx, &dy, &dz, update_p);
  *x = (float)dx;
  *y = (float)dy;
  *z = (float)dz;
}

static void hypnowheel_get_rotation(rotator *rot, float *x, float *y, float *z,
                                    Bool update_p) {
  double dx, dy, dz;
  get_rotation(rot, &dx, &dy, &dz, update_p);
  *x = (float)dx;
  *y = (float)dy;
  *z = (float)dz;
}

#include "hacks/single_precision.h"
#include "hacks/single_frand.h"

#undef M_PI
#define M_PI 3.14159265358979323846f

#define get_position hypnowheel_get_position
#define get_rotation hypnowheel_get_rotation

#include "hypnowheel/hypnowheel.c"
