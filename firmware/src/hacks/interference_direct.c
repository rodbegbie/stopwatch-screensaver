/* Builds the unmodified interference.c. HAVE_INTTYPES_H keeps it from typedef'ing
 * uint32_t itself (as `unsigned int`, which clashes with the board's C
 * library), GETTIMEOFDAY_TWO_ARGS matches the C library's gettimeofday, and the
 * hack's clock is the shim's double_time(): the waves move by the time between
 * frames, and host tests install a fake clock so that frames repeat. */
#include <sys/time.h>

#include "doubletime.h"
#include "screenhack.h"

#define HAVE_INTTYPES_H 1
#define GETTIMEOFDAY_TWO_ARGS 1

static int interference_gettimeofday(struct timeval *tv, void *tz) {
  (void)tz;
  const double now = double_time();
  tv->tv_sec = (time_t)now;
  tv->tv_usec = (suseconds_t)((now - (double)tv->tv_sec) * 1e6);
  return 0;
}
#define gettimeofday interference_gettimeofday

#include "interference/interference.c"
