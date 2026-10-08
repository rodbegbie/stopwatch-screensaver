/* Stand-in for xscreensaver's hacks/screenhack.h, so hacks compile
 * unmodified against the shim. Not derived from xscreensaver code. */
#ifndef XSHIM_SCREENHACK_H
#define XSHIM_SCREENHACK_H

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "hsv.h"
#include "runner/hack_entry.h"
#include "utils.h"
#include "x11shim/xshim.h"

#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif

/* Uniform double in [0, f). */
#define frand(f) (((double)random() / ((double)RAND_MAX + 1.0)) * (double)(f))

/* Registers the hack as `<PREFIX>_hack`; see runner/hack_entry.h. */
#define XSCREENSAVER_MODULE(CLASS, PREFIX)                                  \
  const HackEntry PREFIX##_hack = {CLASS, PREFIX##_defaults, PREFIX##_init, \
                                   PREFIX##_draw, PREFIX##_free};

#endif
