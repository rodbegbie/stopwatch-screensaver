#include "x11shim/arc.h"

#include <math.h>
#include <stdlib.h>

#define FULL_SWEEP (360 * 64)
#define DEGREES_TO_RADIANS 0.017453292519943295f

static int sweep_of(int angle2) {
  if (angle2 > FULL_SWEEP) return FULL_SWEEP;
  if (angle2 < -FULL_SWEEP) return -FULL_SWEEP;
  return angle2;
}

/* About one point every two pixels along the larger radius. */
int arc_point_count(unsigned w, unsigned h, int angle2) {
  const int sweep = sweep_of(angle2);
  if (sweep == 0) return 0;
  const float radius = (float)(w > h ? w : h) / 2.0f;
  const float sweep_radians = (float)abs(sweep) / 64.0f * DEGREES_TO_RADIANS;
  const float n = ceilf(sweep_radians * radius / 2.0f) + 1.0f;
  if (n < 2.0f) return 2;
  return n > (float)ARC_MAX_POINTS ? ARC_MAX_POINTS : (int)n;
}

/* Points between a full sinf and cosf (and a divide) each. The S3 has no float
 * divide, sin or cos, so each is a library call. A run of RUN_LENGTH points is
 * worked out from its first by rotating through the step between points, which
 * is four multiplies and two adds a point, and a point is kept unless it lies
 * within the rotation's error of a rounding boundary; there, and for the first
 * of each run, it is worked out as it always was. The error is about 3e-6 of the
 * radius after RUN_LENGTH steps, from sinf and cosf's own and the rounding of
 * each step: the tolerance below is generous. Angles past ROTATE_ANGLE_LIMIT
 * radians (where float's own spacing is near a millionth of a radian) and
 * coordinates past ROTATE_COORD_LIMIT take the plain loop. */
#define RUN_LENGTH 16
#define ROTATE_ANGLE_LIMIT 32.0f
#define ROTATE_COORD_LIMIT 1.0e6f

/* The int lrintf would give, for a value clear of a rounding boundary. */
static int rounded(float v) { return (int)(v + (v < 0 ? -0.5f : 0.5f)); }

/* Whether v is within tol of a half, where lrintf's choice flips. */
static int near_half(float v, float tol) {
  const float frac = v - (float)(int)v;
  const float off = (frac < 0 ? -frac : frac) - 0.5f;
  return off <= tol && off >= -tol;
}

int arc_points(int x, int y, unsigned w, unsigned h, int angle1, int angle2,
               int *xy, int max_points) {
  int n = arc_point_count(w, h, angle2);
  if (n > max_points) n = max_points;
  if (n < 2) return n < 0 ? 0 : n;
  const float cx = (float)x + (float)w / 2.0f, cy = (float)y + (float)h / 2.0f;
  const float rx = (float)w / 2.0f, ry = (float)h / 2.0f;
  const float start = (float)angle1 / 64.0f * DEGREES_TO_RADIANS;
  const float sweep = (float)sweep_of(angle2) / 64.0f * DEGREES_TO_RADIANS;
  const float radius = rx > ry ? rx : ry;
  const int rotate = n > 2 && start > -ROTATE_ANGLE_LIMIT && start < ROTATE_ANGLE_LIMIT &&
                     sweep > -ROTATE_ANGLE_LIMIT && sweep < ROTATE_ANGLE_LIMIT &&
                     cx > -ROTATE_COORD_LIMIT && cx < ROTATE_COORD_LIMIT &&
                     cy > -ROTATE_COORD_LIMIT && cy < ROTATE_COORD_LIMIT &&
                     radius < ROTATE_COORD_LIMIT;
  const float step = sweep / (float)(n - 1);
  const float cos_step = rotate ? cosf(step) : 0.0f, sin_step = rotate ? sinf(step) : 0.0f;
  const float tol = 2e-3f + 4e-5f * radius;
  float c = 0, sn = 0;
  for (int i = 0; i < n; i++) {
    if (rotate && i % RUN_LENGTH != 0) {
      const float next_c = c * cos_step - sn * sin_step;
      sn = sn * cos_step + c * sin_step;
      c = next_c;
      const float vx = cx + rx * c, vy = cy - ry * sn;
      if (!near_half(vx, tol) && !near_half(vy, tol)) {
        xy[2 * i] = rounded(vx);
        xy[2 * i + 1] = rounded(vy);
        continue;
      }
    }
    const float a = start + sweep * (float)i / (float)(n - 1);
    c = cosf(a);
    sn = sinf(a);
    xy[2 * i] = (int)lrintf(cx + rx * c);
    xy[2 * i + 1] = (int)lrintf(cy - ry * sn);
  }
  return n;
}
