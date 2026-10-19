/* Builds the unmodified menger.c with USE_GL set for this file only, so no
 * other hack sees a GL build. See hacks/gears_gl.c.
 *
 * The sponge fills most of the screen, so its fill is most of the step: it
 * renders at half the width and height (glshim_set_render_scale), as
 * etruscanvenus_single_gl.c does. `-DMENGER_FULL` keeps full size. */
#define USE_GL
#include "xlockmore.h"

#include "menger/menger.c"

#ifndef MENGER_FULL
static void init_sponge_half(ModeInfo *mi) {
  glshim_set_render_scale(2);
  init_sponge(mi);
}

__attribute__((constructor)) static void menger_renders_at_half_size(void) {
  menger_xlockmore_function_table.hack_init = init_sponge_half;
}
#endif
