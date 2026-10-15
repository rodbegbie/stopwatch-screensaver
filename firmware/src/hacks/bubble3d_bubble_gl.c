/* bubble3d/bubble3d.c with USE_GL, in single precision, and with its decimal
 * literals made floats at build time (tools/float_literals.py). Each frame it
 * wobbles every vertex of every bubble with a `cos` per nudge axis (about
 * 25,000 `cos` calls in software `double` on the ESP32-S3) and normalises
 * each one (see hacks/bubble3d_gl.c).
 *
 * bubble3d.h comes first, so `glb_config`'s `double p_bubble_group` keeps its
 * layout for the other three files, which are built in double. */
#define USE_GL
#include "bubble3d/bubble3d.h"
#include "hacks/single_precision.h"
#include "hacks/single_constants.h"
#include "bubble3d_floatlit.c"
