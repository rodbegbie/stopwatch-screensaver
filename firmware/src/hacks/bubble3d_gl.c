/* Builds the unmodified Bubble3D sources with USE_GL set for each file alone, as
 * upstream builds them, so no other hack sees a GL build. See hacks/gears_gl.c.
 *
 * Upstream's own GLB_SLOW_GL switch takes the bubble sphere from 3 subdivisions
 * (1,280 triangles) to 2 (320). Depth 3 ran at 4-5 fps on the board, with 30,720
 * vertices a frame through TinyGL.
 *
 * b_lockglue.c's init() builds a sphere and drops the result (the sphere that
 * draws is b_draw.c's own), leaking it on every start. Nothing reads that
 * value, so here the call returns NULL. */
#define USE_GL
#include "bubble3d/bubble3d.h"

#undef GLB_SLOW_GL
#define GLB_SLOW_GL 1
#define glb_sphere_init() NULL
#include "bubble3d/b_lockglue.c"
