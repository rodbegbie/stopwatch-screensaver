/* Builds the unmodified graphstat.c with USE_GL set for this file only, so no
 * other hack sees a GL build. See hacks/gears_gl.c. Its word list,
 * graphstat.h, is copied into the build directory by vendor_data.py. */
#define USE_GL
#include "graphstat/graphstat.c"
