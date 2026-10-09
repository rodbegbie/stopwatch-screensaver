/* Single-precision sin and cos for the *_single.c wrappers that call them in a
 * hot loop. The S3 has no trigonometry hardware, and its libm sinf and cosf cost
 * a call each; these reduce the angle to [-pi/2, pi/2] and run a degree-11
 * polynomial (error under 2e-6 for angles up to 60 radians, see test_hacks.c). */
#ifndef HACKS_FAST_TRIG_H
#define HACKS_FAST_TRIG_H

#define FAST_TRIG_INV_PI 0.31830988618f
#define FAST_TRIG_HALF_PI 1.5707963267948966f

/* sin(r) * (-1)^k for r within pi/2 of zero. */
static inline float fast_trig_poly(float r, int k) {
  const float r2 = r * r;
  const float p = r * (1.0f + r2 * (-1.6666667e-1f +
                                    r2 * (8.3333333e-3f +
                                          r2 * (-1.9841270e-4f +
                                                r2 * (2.7557319e-6f +
                                                      r2 * -2.5052108e-8f)))));
  return (k & 1) ? -p : p;
}

/* x - k * pi, with pi in three parts (Cephes' constants times 4) so that each
 * product is exact for the small k a hack produces. */
static inline float fast_trig_reduce(float x, int k) {
  const float kf = (float)k;
  return ((x - kf * 3.140625f) - kf * 9.67502593994140625e-4f) -
         kf * 1.509957990978376432e-7f;
}

static inline float fast_sinf(float x) {
  const int k = (int)(x * FAST_TRIG_INV_PI + (x < 0.0f ? -0.5f : 0.5f));
  return fast_trig_poly(fast_trig_reduce(x, k), k);
}

/* cos(x) = sin(x + pi/2), reduced with the half pi added after the subtraction
 * so that the rounding of x + pi/2 does not cost precision. */
static inline float fast_cosf(float x) {
  const float q = x * FAST_TRIG_INV_PI + 0.5f;
  const int k = (int)(q + (q < 0.0f ? -0.5f : 0.5f));
  return fast_trig_poly(fast_trig_reduce(x, k) + FAST_TRIG_HALF_PI, k);
}

#endif
