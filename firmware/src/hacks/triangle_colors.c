/* Builds the unmodified triangle.c with colors.h already included. Upstream
 * reaches free_colors and make_smooth_colormap through its own header chain;
 * the shim's xlockmore path does not include colors.h, and putting it there
 * would change what every xlockmore hack sees. */
#include "screenhack.h"

#include "triangle/triangle.c"
