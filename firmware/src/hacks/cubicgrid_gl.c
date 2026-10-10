/* Builds the unmodified cubicgrid.c with USE_GL set for this file only, so no
 * other hack sees a GL build. See hacks/gears_gl.c. */
#define USE_GL
#include "cubicgrid/cubicgrid.c"
