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
  unsigned k = 0;
  for (unsigned i = 0; i < n; i++) {
    const unsigned a = (i + n - 1) % n, b = i, c = (i + 1) % n, d = (i + 2) % n;
    for (int j = 0; j < SPLINE_STEPS; j++) {
      const double t = (double)j / SPLINE_STEPS, t2 = t * t, t3 = t2 * t;
      const double w0 = (1 - t) * (1 - t) * (1 - t) / 6;
      const double w1 = (3 * t3 - 6 * t2 + 4) / 6;
      const double w2 = (-3 * t3 + 3 * t2 + 3 * t + 1) / 6;
      const double w3 = t3 / 6;
      s->points[k].x = (short)lround(w0 * s->control_x[a] + w1 * s->control_x[b] +
                                     w2 * s->control_x[c] + w3 * s->control_x[d]);
      s->points[k].y = (short)lround(w0 * s->control_y[a] + w1 * s->control_y[b] +
                                     w2 * s->control_y[c] + w3 * s->control_y[d]);
      k++;
    }
  }
  s->n_points = k;
}

void free_spline(spline *s) {
  if (!s) return;
  free(s->control_x);
  free(s->control_y);
  free(s->points);
  free(s);
}
