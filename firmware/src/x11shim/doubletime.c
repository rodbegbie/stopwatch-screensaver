#include "doubletime.h"

#include <sys/time.h>

double double_time(void) {
  struct timeval now;
  gettimeofday(&now, NULL);
  return (double)now.tv_sec + (double)now.tv_usec * 0.000001;
}
