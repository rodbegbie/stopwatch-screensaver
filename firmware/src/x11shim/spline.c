#include <math.h>
#include <stdlib.h>

#include "spline.h"

/* Points a segment between two control points is cut into. */
#define SPLINE_STEPS 6

spline *make_spline(unsigned int size) {
  spline *s = (spline *)calloc(1, sizeof(*s));
  if (!s) return NULL;
  s->n_controls = size;
  s->control_x = (double *)calloc(size ? size : 1, sizeof(double));
  s->control_y = (double *)calloc(size ? size : 1, sizeof(double));
  if (!s->control_x || !s->control_y) {
    free(s->control_x);
    free(s->control_y);
    free(s);
    return NULL;
  }
  return s;
}

/* The four weights of each step along a segment, worked out once. */
static double g_weights[SPLINE_STEPS][4];
static float g_weights_f[SPLINE_STEPS][4];
static int g_weights_built;

static void build_weights(void) {
  for (int j = 0; j < SPLINE_STEPS; j++) {
    const double t = (double)j / SPLINE_STEPS, t2 = t * t, t3 = t2 * t;
    g_weights[j][0] = (1 - t) * (1 - t) * (1 - t) / 6;
    g_weights[j][1] = (3 * t3 - 6 * t2 + 4) / 6;
    g_weights[j][2] = (-3 * t3 + 3 * t2 + 3 * t + 1) / 6;
    g_weights[j][3] = t3 / 6;
    for (int k = 0; k < 4; k++) g_weights_f[j][k] = (float)g_weights[j][k];
  }
  g_weights_built = 1;
}

/* Control values past this are worked out in double throughout: float's error
 * grows with them. */
#define SPLINE_FLOAT_LIMIT 1e6

/* The point of a blend of four controls, rounded half away from zero as lround
 * does. The S3 does double in software, so the blend is first worked out in
 * float (hardware) and kept when it is clear of a half pixel by more than float's
 * error: about 1e-6 of the largest control, with a floor for small ones. A
 * result near a half pixel is worked out in double, as before, because there
 * the two can round to different pixels. */
static short blended(const float *fc, const double *dc, unsigned a, unsigned b,
                     unsigned c, unsigned d, const float *wf, const double *w,
                     float tol) {
  const float v = wf[0] * fc[a] + wf[1] * fc[b] + wf[2] * fc[c] + wf[3] * fc[d];
  const int whole = (int)v;
  const float frac = v - (float)whole;
  const float off = (frac < 0 ? -frac : frac) - 0.5f;
  if (off > tol || off < -tol) return (short)(int)(v + (v < 0 ? -0.5f : 0.5f));
  return (short)lround(w[0] * dc[a] + w[1] * dc[b] + w[2] * dc[c] + w[3] * dc[d]);
}

void compute_closed_spline(spline *s) {
  const unsigned n = s->n_controls;
  s->n_points = 0;
  if (n == 0) return;
  const unsigned total = n * SPLINE_STEPS;
  if (s->allocated_points < total) {
    XPoint *grown = (XPoint *)realloc(s->points, (size_t)total * sizeof(XPoint));
    if (!grown) return;
    s->points = grown;
    s->allocated_points = total;
  }
  if (!g_weights_built) build_weights();
  double biggest = 0;
  for (unsigned i = 0; i < n; i++) {
    const double ax = s->control_x[i] < 0 ? -s->control_x[i] : s->control_x[i];
    const double ay = s->control_y[i] < 0 ? -s->control_y[i] : s->control_y[i];
    if (ax > biggest) biggest = ax;
    if (ay > biggest) biggest = ay;
  }
  /* A NaN fails the comparison, so it takes the double path too. */
  const int use_float = biggest <= SPLINE_FLOAT_LIMIT;
  float stack_floats[2 * 64];
  float *heap = NULL, *fx = NULL, *fy = NULL;
  if (use_float) {
    if (n <= 64) {
      fx = stack_floats;
    } else {
      heap = (float *)malloc((size_t)n * 2 * sizeof(float));
      fx = heap;
    }
    if (fx) {
      fy = fx + n;
      for (unsigned i = 0; i < n; i++) {
        fx[i] = (float)s->control_x[i];
        fy[i] = (float)s->control_y[i];
      }
    }
  }
  const float tol = 1e-3f + 2e-6f * (float)biggest;
  unsigned k = 0;
  for (unsigned i = 0; i < n; i++) {
    const unsigned a = (i + n - 1) % n, b = i, c = (i + 1) % n, d = (i + 2) % n;
    for (int j = 0; j < SPLINE_STEPS; j++) {
      if (fx) {
        s->points[k].x = blended(fx, s->control_x, a, b, c, d, g_weights_f[j], g_weights[j], tol);
        s->points[k].y = blended(fy, s->control_y, a, b, c, d, g_weights_f[j], g_weights[j], tol);
      } else {
        const double *w = g_weights[j];
        s->points[k].x = (short)lround(w[0] * s->control_x[a] + w[1] * s->control_x[b] +
                                       w[2] * s->control_x[c] + w[3] * s->control_x[d]);
        s->points[k].y = (short)lround(w[0] * s->control_y[a] + w[1] * s->control_y[b] +
                                       w[2] * s->control_y[c] + w[3] * s->control_y[d]);
      }
      k++;
    }
  }
  free(heap);
  s->n_points = k;
}

void free_spline(spline *s) {
  if (!s) return;
  free(s->control_x);
  free(s->control_y);
  free(s->points);
  free(s);
}
