/* Builds the unmodified gears.c with USE_GL set for this file only, so no
 * other hack sees a GL build. Its helpers are built by the gl_helper_*.c
 * wrappers, one translation unit each as upstream does (tube.h and normals.h
 * define clashing XYZ types). Spike only: needs TinyGL, compiled with
 * -DTGL_SPIKE. */
#ifdef TGL_SPIKE

#define USE_GL

#include "../../../vendor/xscreensaver-6.16/hacks/glx/gears.c"

#endif
