/* Builds blocktube.c with USE_GL set for this file only (see hacks/gears_gl.c),
 * in single precision with its decimal literals made floats at build time
 * (tools/float_literals.py), with BLOCKTUBE_ENTITIES blocks instead of 1,000
 * (tools/blocktube_patch.py), and at half resolution. Its texture,
 * blocktube.png, is converted from vendor/ at build time
 * (firmware/gl_textures.py).
 *
 * TinyGL has no fog. The hack's linear black fog is applied here per block
 * instead of per pixel: the hack sets each block's colour once its transform
 * is in place, so the colour call reads the block's distance from the
 * modelview and scales the colour by the fog factor, which darkens the
 * texture too (TinyGL modulates it by the colour). A block the fog hides
 * completely is not drawn: it would be black on black, behind which there is
 * only more fog. TinyGL has no sphere map either (glTexGeni is accepted and
 * ignored), so each face shows the whole texture through the coordinates the
 * hack gives it, instead of a reflection. */
#define USE_GL
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xlockmore.h"
#include "colors.h"

/* On the board at half size: 1,000 blocks ran at 5.0 fps, 500 at 8.0 to 9.0,
 * 400 at 8.8 to 10.0 (full size, 1,000: 3.3). -DBLOCKTUBE_ENTITIES=<n> and
 * -DBLOCKTUBE_FULL try the others. */
#ifndef BLOCKTUBE_ENTITIES
#define BLOCKTUBE_ENTITIES 400
#endif

static int blocktube_fog_on;
static GLfloat blocktube_fog_start, blocktube_fog_end = 1, blocktube_fog_factor = 1;

static void blocktube_fogi(GLint pname, GLint param) { (void)pname, (void)param; }
static void blocktube_fogfv(GLint pname, const GLfloat *param) { (void)pname, (void)param; }
static void blocktube_fogf(GLint pname, GLfloat param) {
  if (pname == GL_FOG_START) blocktube_fog_start = param;
  if (pname == GL_FOG_END) {
    blocktube_fog_end = param;
    blocktube_fog_on = 1;
  }
}

/* Linear fog by the block's distance along the view axis. */
static void blocktube_color4ub(GLubyte r, GLubyte g, GLubyte b, GLubyte a) {
  blocktube_fog_factor = 1;
  if (blocktube_fog_on && blocktube_fog_end > blocktube_fog_start) {
    GLfloat m[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, m);
    GLfloat f = (blocktube_fog_end - -m[14]) / (blocktube_fog_end - blocktube_fog_start);
    blocktube_fog_factor = f < 0 ? 0 : f > 1 ? 1 : f;
  }
  const GLfloat f = blocktube_fog_factor;
  glColor4ub((GLubyte)(r * f), (GLubyte)(g * f), (GLubyte)(b * f), a);
}

static void blocktube_call_list(GLuint list) {
  if (blocktube_fog_factor > 0) glCallList(list);
}

/* TinyGL warns on every enable it does not know, each frame. */
static void blocktube_enable(GLint cap) {
  if (cap != GL_FOG && cap != GL_TEXTURE_GEN_S && cap != GL_TEXTURE_GEN_T) glEnable(cap);
}

#define glEnable blocktube_enable
#define glFogi blocktube_fogi
#define glFogf blocktube_fogf
#define glFogfv blocktube_fogfv
#define glColor4ub blocktube_color4ub
#define glCallList blocktube_call_list

#include "hacks/single_precision.h"
#include "hacks/single_frand.h"

#include "blocktube_floatlit.c"

#ifndef BLOCKTUBE_FULL
/* Half resolution (glshim_set_render_scale), as etruscanvenus_single_gl.c. */
static void init_blocktube_half(ModeInfo *mi) {
  glshim_set_render_scale(2);
  init_blocktube(mi);
}

__attribute__((constructor)) static void blocktube_renders_at_half_size(void) {
  blocktube_xlockmore_function_table.hack_init = init_blocktube_half;
}
#endif
