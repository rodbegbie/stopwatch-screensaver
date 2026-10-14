/* Builds hextrail.c with USE_GL set for this file only (see hacks/gears_gl.c),
 * in single precision, and with its decimal literals made floats at build
 * time (tools/float_literals.py). The hack walks every hexagon each frame and
 * does its colour, size and corner maths in `double`, which the ESP32-S3's
 * FPU does in software.
 *
 * It passes the addresses of `double` locals to get_position and
 * get_rotation, which take `double *`: as floats they would be written eight
 * bytes (see moire_single.c). The adapters come before single_precision.h, so
 * their `double` is the real one.
 *
 * Each hexagon's `pos` and every corner are `XYZ`, which normals.h declares
 * with `double` fields before single_precision.h can rename them, so every
 * vertex (`h->pos.x + corners[j].x * size1`) was a software double add and
 * multiply. The hack only stores XYZ (it never calls calc_normal), so
 * normals.h sees its own name for the double struct and the hack gets a float
 * one. */
#define USE_GL
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xlockmore.h"
#include "colors.h"
#define XYZ glx_double_xyz
#include "normals.h"
#undef XYZ
#include "rotator.h"
#include "gltrackball.h"

static void hextrail_get_position(rotator *rot, float *x, float *y, float *z,
                                  Bool update_p) {
  double dx, dy, dz;
  get_position(rot, &dx, &dy, &dz, update_p);
  *x = (float)dx;
  *y = (float)dy;
  *z = (float)dz;
}

static void hextrail_get_rotation(rotator *rot, float *x, float *y, float *z,
                                  Bool update_p) {
  double dx, dy, dz;
  get_rotation(rot, &dx, &dy, &dz, update_p);
  *x = (float)dx;
  *y = (float)dy;
  *z = (float)dz;
}

#include "hacks/single_precision.h"
#include "hacks/single_frand.h"

typedef struct {
  double x, y, z;
} XYZ;

#define get_position hextrail_get_position
#define get_rotation hextrail_get_rotation

#include "hextrail_floatlit.c"
