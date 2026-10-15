/* Builds the unmodified Bubble3D sources with USE_GL set for each file alone, as
 * upstream builds them, so no other hack sees a GL build. See hacks/gears_gl.c.
 *
 * b_lockglue.c's init() builds a sphere and drops the result (the sphere that
 * draws is b_draw.c's own), leaking it on every start. Nothing reads that
 * value, so here the call returns NULL. */
#define USE_GL
#include "bubble3d/bubble3d.h"

#define glb_sphere_init() NULL
#include "bubble3d/b_lockglue.c"
