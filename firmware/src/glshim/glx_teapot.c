/* The unmodified glx/teapot.c, built with USE_GL (see hacks/gears_gl.c).
 * HAVE_JWZGLES picks its triangle table (teapot2.h) over GL evaluators
 * (glMap2f, glEvalMesh2), which TinyGL lacks. */
#define USE_GL
#define HAVE_JWZGLES
#include "xs_support/glx/teapot.c"
