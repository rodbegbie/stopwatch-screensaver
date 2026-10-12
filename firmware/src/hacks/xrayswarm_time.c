/* Builds the unmodified xrayswarm.c with gettimeofday turned on. Upstream
 * takes it from config.h, and without it the hack never defines `initTime`
 * but still calls it. */
#include "screenhack.h"

#define HAVE_GETTIMEOFDAY 1
#define GETTIMEOFDAY_TWO_ARGS 1

#include "xrayswarm/xrayswarm.c"
