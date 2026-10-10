/* Builds the unmodified gears.c with USE_GL set for this file only, so no other
 * hack sees a GL build. Its helpers are built the same way by the glx_*.c
 * wrappers in src/glshim/, one translation unit each as upstream does
 * (tube.h and normals.h define clashing types). HAVE_GL stays undefined: it
 * changes the layout of ModeInfo, which xlockmore.c is built without. */
#define USE_GL
#include "gears/gears.c"
