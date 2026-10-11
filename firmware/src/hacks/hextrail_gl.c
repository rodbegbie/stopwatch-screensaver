/* Builds the unmodified hextrail.c with USE_GL set for this file only, so no
 * other hack sees a GL build. See hacks/gears_gl.c. */
#define USE_GL
#include "hextrail/hextrail.c"
