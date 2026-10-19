/* Builds cubenetic.c with USE_GL set for this file only (see hacks/gears_gl.c),
 * in single precision, with its decimal literals made floats at build time
 * (tools/float_literals.py) and its plasma texture cut from 256 by 256 to
 * CUBENETIC_TEXTURE_SIZE (tools/cubenetic_patch.py). The texture is worked out
 * again every frame, a square root per wave source per pixel: 847 ms a frame
 * at 256.
 *
 * The hack passes the addresses of `double` locals to get_position and
 * get_rotation: as floats they would be written eight bytes (see
 * hextrail_single_gl.c). The adapters come before single_precision.h, so their
 * `double` is the real one. */
#define USE_GL
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xlockmore.h"
#include "colors.h"
#include "rotator.h"
#include "gltrackball.h"

static void cubenetic_get_position(rotator *rot, float *x, float *y, float *z,
                                   Bool update_p) {
  double dx, dy, dz;
  get_position(rot, &dx, &dy, &dz, update_p);
  *x = (float)dx;
  *y = (float)dy;
  *z = (float)dz;
}

static void cubenetic_get_rotation(rotator *rot, float *x, float *y, float *z,
                                   Bool update_p) {
  double dx, dy, dz;
  get_rotation(rot, &dx, &dy, &dz, update_p);
  *x = (float)dx;
  *y = (float)dy;
  *z = (float)dz;
}

#include "hacks/single_precision.h"
#include "hacks/single_frand.h"

#define get_position cubenetic_get_position
#define get_rotation cubenetic_get_rotation

#ifndef CUBENETIC_TEXTURE_SIZE
#define CUBENETIC_TEXTURE_SIZE 64
#endif

#include "cubenetic_floatlit.c"

/* free_cube never frees the array of cubes that init_cube callocs (320 bytes
 * a start), so the wrapper frees it first. The restart leak test found it. */
static void free_cube_and_its_cubes(ModeInfo *mi) {
  cube_configuration *cc = &ccs[MI_SCREEN(mi)];
  if (cc->glx_context) {
    free(cc->cubes);
    cc->cubes = NULL;
  }
  free_cube(mi);
}

__attribute__((constructor)) static void cubenetic_frees_its_cubes(void) {
  cubenetic_xlockmore_function_table.hack_free = free_cube_and_its_cubes;
}
