/* Shim replacement for xscreensaver's utils/spline.h: a closed uniform cubic
 * B-spline through a hack's control points. Not derived from xscreensaver
 * code; the struct has the same fields because hacks reach into it (and free
 * its arrays themselves, so all of them are malloc'd). */
#ifndef XSHIM_SPLINE_H
#define XSHIM_SPLINE_H

#include "x11shim/xshim.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _spline {
  /* input: the hack fills these in */
  unsigned int n_controls;
  double *control_x;
  double *control_y;

  /* output: compute_closed_spline fills these in */
  unsigned int n_points;
  XPoint *points;
  unsigned int allocated_points;
} spline;

/* Zeroed control arrays of `size` entries and no points, or NULL. */
spline *make_spline(unsigned int size);
/* Fills `points` with SPLINE_STEPS points for each control point: the curve
 * through them closes on itself and cuts each corner. */
void compute_closed_spline(spline *s);
void free_spline(spline *s);

#ifdef __cplusplus
}
#endif

#endif
