/* Builds glmatrix.c with USE_GL set for this file only (see hacks/gears_gl.c),
 * in single precision, and with its decimal literals made floats at build
 * time (tools/float_literals.py): draw_glyph runs for every visible glyph.
 *
 * TinyGL has no alpha, and GLMatrix keeps its look in it: the glyph texture's
 * alpha is the original green channel, each glyph's brightness is the vertex
 * alpha, and the blend is `GL_SRC_ALPHA, GL_ONE` (added to the screen). Both
 * alphas are folded into colour instead, which an additive blend cannot tell
 * apart: the texture is premultiplied as it is uploaded, and `glColor4f`
 * scales its own colour (`fold_alpha.h` clamps the alpha, which a spinner glyph
 * pushes past 1). The shim's `glTexImage2D` would otherwise drop the
 * alpha and leave solid green squares. `xlockmore.h` comes first because it
 * brings in glshim.h, which defines `glTexImage2D` as that function. */
#define USE_GL
#include <stdlib.h>

#include "xlockmore.h"
#include "hacks/fold_alpha.h"

static void glmatrix_tex_image_2d(GLenum target, GLint level, GLint internal,
                                  GLsizei w, GLsizei h, GLint border,
                                  GLenum format, GLenum type,
                                  const GLvoid *data) {
  (void)internal;
  (void)format;
  (void)type;
  const unsigned char *rgba = data;
  unsigned char *rgb = malloc((size_t)w * h * 3);
  if (!rgb) return;
  for (size_t i = 0, n = (size_t)w * h; i < n; i++) {
    const unsigned a = rgba[i * 4 + 3];
    for (int c = 0; c < 3; c++)
      rgb[i * 3 + c] = (unsigned char)((rgba[i * 4 + c] * a + 127) / 255);
  }
  glTexImage2D(target, level, GL_RGB, w, h, border, GL_RGB, GL_UNSIGNED_BYTE,
               rgb);
  free(rgb);
}

static void glmatrix_color4f(GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
  float c[3];
  fold_alpha(r, g, b, a, c);
  glColor3f(c[0], c[1], c[2]);
}

#undef glTexImage2D
#define glTexImage2D glmatrix_tex_image_2d
#define glColor4f glmatrix_color4f
#include "hacks/single_precision.h"
#include "hacks/single_frand.h"

#include "glmatrix_floatlit.c"

#ifndef GLMATRIX_FULL
/* Half resolution (glshim_set_render_scale): an init that asks for scale 2
 * before the hack's init_GL opens the context, swapped into the hack's
 * function table as etruscanvenus_single_gl.c does. Fill was 100 of a 145 ms
 * step at full size. */
static void init_matrix_half(ModeInfo *mi) {
  glshim_set_render_scale(2);
  init_matrix(mi);
}

__attribute__((constructor)) static void glmatrix_renders_at_half_size(void) {
  glmatrix_xlockmore_function_table.hack_init = init_matrix_half;
}
#endif
