/* Builds superquadrics.c with USE_GL set for this file only (see
 * hacks/gears_gl.c), in single precision, with its decimal literals made
 * floats at build time (tools/float_literals.py), and at half resolution.
 *
 * Every frame it works out each mesh vertex in `double`, which the ESP32-S3's
 * FPU does in software, and the fill of its big shaded shape was 37 of a
 * 70 ms step: half the width and height (glshim_set_render_scale) cuts that
 * to a quarter. `-DSUPERQUADRICS_FULL` keeps full size. */
#define USE_GL
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xlockmore.h"

#include "hacks/single_precision.h"

#include "superquadrics_floatlit.c"

#ifndef SUPERQUADRICS_FULL
static void init_superquadrics_half(ModeInfo *mi) {
  glshim_set_render_scale(2);
  init_superquadrics(mi);
}

__attribute__((constructor)) static void superquadrics_renders_at_half_size(void) {
  superquadrics_xlockmore_function_table.hack_init = init_superquadrics_half;
}
#endif
