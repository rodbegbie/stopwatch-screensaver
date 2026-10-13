/* The unmodified glx/teapot.c, built with USE_GL (see hacks/gears_gl.c).
 * HAVE_JWZGLES picks its triangle table (teapot2.h) over GL evaluators
 * (glMap2f, glEvalMesh2), which TinyGL lacks.
 *
 * It is included from generated/teapot/, where firmware/teapot_table.py puts
 * a byte-identical copy beside a teapot2.h of our own: the table deflated,
 * read through xshim_teapot_table() (issue #104). The upstream unit_teapot is
 * renamed so the one below can unpack the table around it. */
#include <stdlib.h>

#include "glshim/glx_teapot.h"
#include "x11shim/inflate.h"

#define USE_GL
#define HAVE_JWZGLES
#define unit_teapot upstream_unit_teapot
#include "teapot/teapot.c"
#undef unit_teapot

static GLfloat *g_table;

const GLfloat *xshim_teapot_unpack(void) {
  if (g_table) return g_table;
  g_table = (GLfloat *)malloc(sizeof(GLfloat) * XSHIM_TEAPOT_FLOATS);
  if (g_table && xshim_inflate(teapot_triangles_z, sizeof(teapot_triangles_z),
                               (unsigned char *)g_table,
                               sizeof(GLfloat) * XSHIM_TEAPOT_FLOATS) != 0) {
    free(g_table);
    g_table = NULL;
  }
  return g_table;
}

void xshim_teapot_release(void) {
  free(g_table);
  g_table = NULL;
}

const GLfloat (*xshim_teapot_table(void))[XSHIM_TEAPOT_FLOATS] {
  return (const GLfloat(*)[XSHIM_TEAPOT_FLOATS])g_table;
}

/* With no memory for the table the teapot is left out, as an empty list. */
int unit_teapot(int grid, int wire_p) {
  if (!xshim_teapot_unpack()) return 0;
  const int polys = upstream_unit_teapot(grid, wire_p);
  xshim_teapot_release();
  return polys;
}
