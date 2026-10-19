/* Builds kaleidocycle.c with USE_GL set for this file only, so no other hack
 * sees a GL build. See hacks/gears_gl.c. The copy is the build's patched one
 * (tools/kaleidocycle_patch.py): upstream's first frame divides 0 by 0 for a
 * piece's angle. */
#define USE_GL
#include "kaleidocycle_patched.c"
