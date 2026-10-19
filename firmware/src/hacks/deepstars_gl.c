/* Builds the unmodified deepstars.c with USE_GL set for this file only, so no
 * other hack sees a GL build. See hacks/gears_gl.c.
 *
 * Each smear copy of the sky is drawn at a lower alpha, blended over the one
 * before. TinyGL has no alpha, and its blend is right only for black and
 * white, so the wrapper folds the alpha into the colour and draws opaque:
 * `glColor4fv` scales its own colour, and the blend is never switched on. The
 * stars are at distinct places, so a dimmer copy only overwrites its own
 * brighter one where they touch.
 *
 * It also stops `glEnable(GL_POINT_SMOOTH)`, which TinyGL does not know and
 * prints a warning for every frame. */
#define USE_GL
#include "xlockmore.h"

static void deepstars_enable(GLenum cap) {
  if (cap != GL_BLEND && cap != GL_POINT_SMOOTH) glEnable(cap);
}

static void deepstars_color4fv(const GLfloat *c) {
  glColor3f(c[0] * c[3], c[1] * c[3], c[2] * c[3]);
}

#define glEnable deepstars_enable
#define glColor4fv deepstars_color4fv
#include "deepstars/deepstars.c"
