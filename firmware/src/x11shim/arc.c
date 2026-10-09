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

int arc_points(int x, int y, unsigned w, unsigned h, int angle1, int angle2,
               int *xy, int max_points) {
  int n = arc_point_count(w, h, angle2);
  if (n > max_points) n = max_points;
  if (n < 2) return n < 0 ? 0 : n;
  const float cx = (float)x + (float)w / 2.0f, cy = (float)y + (float)h / 2.0f;
  const float rx = (float)w / 2.0f, ry = (float)h / 2.0f;
  const float start = (float)angle1 / 64.0f * DEGREES_TO_RADIANS;
  const float sweep = (float)sweep_of(angle2) / 64.0f * DEGREES_TO_RADIANS;
  for (int i = 0; i < n; i++) {
    const float a = start + sweep * (float)i / (float)(n - 1);
    xy[2 * i] = (int)lrintf(cx + rx * cosf(a));
    xy[2 * i + 1] = (int)lrintf(cy - ry * sinf(a));
  }
  return n;
}
