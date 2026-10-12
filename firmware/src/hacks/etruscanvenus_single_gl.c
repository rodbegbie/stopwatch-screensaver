/* Builds etruscanvenus.c with USE_GL set for this file only (see
 * hacks/gears_gl.c), in single precision, with its decimal literals made
 * floats at build time (tools/float_literals.py), on a smaller grid
 * (tools/etruscanvenus_patch.py), and at half resolution. Upstream works out a
 * 192 by 128 grid of points and normals every frame and draws about 49,000 lit
 * vertices. Its maths is float already, but the surface's ranges and M_PI come
 * in as `double`. Without HAVE_GLSL it takes its fixed-function path.
 *
 * On the board, 64 by 48 at full size ran at 4.2 to 7.4 fps (fill 50 to 65 ms
 * a frame in the modes that fill the screen), and 48 by 32 at half size at 9.2
 * to 12.6. -DETRUSCANVENUS_NUMU=<n>, -DETRUSCANVENUS_NUMV=<n> and
 * -DETRUSCANVENUS_FULL try the others. */
#ifndef ETRUSCANVENUS_NUMU
#define ETRUSCANVENUS_NUMU 48
#endif
#ifndef ETRUSCANVENUS_NUMV
#define ETRUSCANVENUS_NUMV 32
#endif

#define USE_GL
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xlockmore.h"
#include "gltrackball.h"

#include "hacks/single_precision.h"
#include "hacks/single_frand.h"
#define floor(x) floorf(x)
#define round(x) roundf(x)
#undef M_PI
#define M_PI 3.14159265358979323846f
#undef M_PI_2
#define M_PI_2 1.57079632679489661923f
#undef M_SQRT2
#define M_SQRT2 1.41421356237309504880f

#include "etruscanvenus_floatlit.c"

#ifndef ETRUSCANVENUS_FULL
/* Half resolution (glshim_set_render_scale): an init that asks for scale 2
 * before the hack's init_GL opens the context, swapped into the hack's
 * function table as hypnowheel_single_gl.c does. */
static void init_etruscanvenus_half(ModeInfo *mi) {
  glshim_set_render_scale(2);
  init_etruscanvenus(mi);
}

__attribute__((constructor)) static void etruscanvenus_renders_at_half_size(void) {
  etruscanvenus_xlockmore_function_table.hack_init = init_etruscanvenus_half;
}
#endif
