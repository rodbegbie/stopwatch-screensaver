/* Builds hypnowheel.c with USE_GL set for this file only (see hacks/gears_gl.c),
 * in single precision. Each vertex of each spiral arm calls `cos` and `sin` in
 * `double`, which the ESP32-S3's FPU does in software: about 275 ms of its
 * 706 ms step on the board (issue #43).
 *
 * It passes the addresses of `double` locals to get_position and
 * get_rotation, which take `double *`: as floats they would be written eight
 * bytes (see moire_single.c). The adapters come before single_precision.h, so
 * their `double` is the real one.
 *
 * With -DHYPNOWHEEL_HALF it renders at half the width and height (issue #43):
 * its fill is about three quarters of its frame. */
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

#ifdef HYPNOWHEEL_HALF
/* Half resolution (glshim_set_render_scale): an init that asks for scale 2
 * before the hack's init_GL opens the context, swapped into the hack's
 * function table as loop_direct.c does. */
static void init_hypnowheel_half(ModeInfo *mi) {
  glshim_set_render_scale(2);
  init_hypnowheel(mi);
}

__attribute__((constructor)) static void hypnowheel_renders_at_half_size(void) {
  hypnowheel_xlockmore_function_table.hack_init = init_hypnowheel_half;
}
#endif
