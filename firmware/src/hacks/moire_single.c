/* Builds the unmodified moire.c in single precision (the ESP32-S3's FPU is
 * single-precision only, so `double` maths runs in software). It has no
 * decimal literals, so unlike the other wrappers it needs no literal pass.
 * Each pixel squares two offsets and divides by the ring spacing in double:
 * 44 ms for a 20-row chunk as copied. A float holds every square exactly, but
 * its rounding can move a ring edge by one pixel where the quotient lies
 * within 0.06 of a whole number. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "screenhack.h"
#include "xshm.h"

/* The hack passes the addresses of its own `double` locals to rgb_to_hsv,
 * which takes `double *`: as floats they would be written eight bytes. This
 * comes before single_precision.h, so its `double` is the real one. */
static void moire_rgb_to_hsv(unsigned short r, unsigned short g,
                             unsigned short b, int *h, float *s, float *v) {
  double ds, dv;
  rgb_to_hsv(r, g, b, h, &ds, &dv);
  *s = (float)ds;
  *v = (float)dv;
}

#include "hacks/single_precision.h"

#define rgb_to_hsv moire_rgb_to_hsv

#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)

#include "moire/moire.c"

const HackEntry moire_hack = {"Moire", moire_defaults, moire_init,
                              moire_draw, moire_free, NULL, NULL};
