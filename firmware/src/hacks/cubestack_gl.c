/* Builds the unmodified cubestack.c with USE_GL set for this file only, so no
 * other hack sees a GL build. See hacks/gears_gl.c.
 *
 * Each face's alpha is the opacity times how far its transition has run, and
 * the blend is `GL_SRC_ALPHA, GL_ONE` (added to the screen). TinyGL has no
 * alpha, so `glColor4fv` scales its own colour instead, as glmatrix_single_gl.c
 * does: an additive blend cannot tell the difference. */
#define USE_GL
#include "xlockmore.h"

static void cubestack_color4fv(const GLfloat *c) {
  glColor3f(c[0] * c[3], c[1] * c[3], c[2] * c[3]);
}

#define glColor4fv cubestack_color4fv
#include "cubestack/cubestack.c"

/* `ccolor` wraps with `> ncolors`, one cycle late: at 32 the next draw reads
 * colors[32] of a 32 entry array, after about 12,900 frames. Draw with it
 * wrapped first; frames are the same until then. */
static void draw_cube_in_range(ModeInfo *mi) {
  cube_configuration *bp = &bps[MI_SCREEN(mi)];
  if (bp->ccolor >= bp->ncolors) bp->ccolor = 0;
  draw_cube(mi);
}

__attribute__((constructor)) static void cubestack_wraps_its_palette(void) {
  cubestack_xlockmore_function_table.hack_draw = draw_cube_in_range;
}
