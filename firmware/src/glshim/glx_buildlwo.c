/* The unmodified glx/buildlwo.c, built with USE_GL (see hacks/gears_gl.c).
 * TinyGL is built without GL_POLYGON and draws nothing for it (issue #80).
 * A triangle fan draws a convex polygon exactly as GL_POLYGON would. */
#define USE_GL
#include "GL/gl.h"
#define GL_POLYGON GL_TRIANGLE_FAN
#include "xs_support/glx/buildlwo.c"
