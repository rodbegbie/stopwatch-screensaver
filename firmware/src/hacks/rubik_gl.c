/* Builds the unmodified rubik.c with USE_GL set for this file only, so no
 * other hack sees a GL build. See hacks/gears_gl.c.
 *
 * A sticker sits 0.01 above its cube face, about two steps of the 16-bit depth
 * buffer at the cube's distance, so the face showed through in patches: each
 * GL_POLYGON is drawn with a polygon offset toward the viewer. */
#define USE_GL
#include "GL/gl.h"

static void rubik_begin(GLenum mode) {
  if (mode == GL_POLYGON) {
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-2.0f, -2.0f);
  }
  glBegin(mode);
}

static void rubik_end(void) {
  glEnd();
  glDisable(GL_POLYGON_OFFSET_FILL);
}

#define glBegin rubik_begin
#define glEnd rubik_end
#include "rubik/rubik.c"
