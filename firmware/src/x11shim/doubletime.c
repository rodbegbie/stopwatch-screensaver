#include "doubletime.h"

#include <stddef.h>
#include <sys/time.h>

static double (*g_clock)(void);

void xshim_set_clock(double (*clock)(void)) { g_clock = clock; }

double double_time(void) {
  if (g_clock) return g_clock();
  struct timeval now;
  gettimeofday(&now, NULL);
  return (double)now.tv_sec + (double)now.tv_usec * 0.000001;
}
