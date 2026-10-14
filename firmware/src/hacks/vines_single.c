/* Builds vines.c without software double maths. It takes the sin and cos of
 * `(double) fp->a`, a whole number that grows into the billions, so a float
 * cannot stand in for it: the angle would lose its fraction. `double` is
 * built as `long` instead, so the wrapper's sin and cos receive the exact
 * integer, work out its phase in whole turns with a 64-bit multiply, and only
 * then go to float. The hack's decimal literals are made floats at build time
 * (tools/float_literals.py) and M_PI is a float (single_constants.h). */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "xlockmore.h"

#include "hacks/single_constants.h"

/* 2^64 / (2 pi), rounded: multiplying by it wraps a radian count into turns
 * of 2^64, exactly for any 32-bit integer. */
#define VINES_TURNS_PER_RADIAN UINT64_C(0x28be60db9391054a)

static inline float vines_angle(long radians) {
  const uint64_t turns = (uint64_t)(int64_t)radians * VINES_TURNS_PER_RADIAN;
  return (float)(uint32_t)(turns >> 32) * (6.28318530717958647692f / 4294967296.0f);
}

#define cos(x) cosf(vines_angle(x))
#define sin(x) sinf(vines_angle(x))
#define double long

#include "vines_floatlit.c"
