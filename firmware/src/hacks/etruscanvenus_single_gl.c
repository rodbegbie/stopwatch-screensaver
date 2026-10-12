/* Builds etruscanvenus.c with USE_GL set for this file only (see
 * hacks/gears_gl.c), in single precision, with its decimal literals made
 * floats at build time (tools/float_literals.py), and on a smaller grid
 * (tools/etruscanvenus_patch.py): upstream works out a 192 by 128 grid of
 * points and normals every frame and draws about 49,000 lit vertices. Its
 * maths is float already, but the surface's ranges come in as `double`.
 * Without HAVE_GLSL it takes its fixed-function path. */
#ifndef ETRUSCANVENUS_NUMU
#define ETRUSCANVENUS_NUMU 64
#endif
#ifndef ETRUSCANVENUS_NUMV
#define ETRUSCANVENUS_NUMV 48
#endif

#define USE_GL
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xlockmore.h"
#include "gltrackball.h"

#include "hacks/single_precision.h"
#include "hacks/single_frand.h"

#include "etruscanvenus_floatlit.c"
