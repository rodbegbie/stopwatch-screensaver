#include "easing.h"

#include <math.h>
#include <stdlib.h>

/* The sines run in single precision: the S3's FPU is single-precision only,
 * so a double cos is a software routine, and a hack calls this for every row
 * of every frame. */
#define PI_F 3.14159265358979323846f

double ease(easing_function fn, double x) {
  float t = (float)x;
  switch (fn) {
    case EASE_NONE:
      return x;
    case EASE_IN_SINE:
      return 1.0f - cosf(t * PI_F / 2.0f);
    case EASE_OUT_SINE:
      return sinf(t * PI_F / 2.0f);
    case EASE_IN_OUT_SINE:
      return -(cosf(PI_F * t) - 1.0f) / 2.0f;
  }
  abort();
}
