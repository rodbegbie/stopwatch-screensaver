#include "x11shim/stroke.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

typedef int64_t wide_t;
typedef struct {
  float x, y;
} vec2;

static wide_t min_w(wide_t a, wide_t b) { return a < b ? a : b; }
static wide_t max_w(wide_t a, wide_t b) { return a > b ? a : b; }
static wide_t clamp_w(wide_t v, wide_t lo, wide_t hi) {
  return v < lo ? lo : v > hi ? hi : v;
}

/* A disc of diameter w centred on the pixel (cx, cy). Skipped when it cannot
 * reach the canvas. */
static void disc(Canvas *c, wide_t cx, wide_t cy, int w, uint16_t colour) {
  if (cx < -(wide_t)w || cy < -(wide_t)w || cx > (wide_t)c->w + w ||
      cy > (wide_t)c->h + w)
    return;
  canvas_fill_ellipse(c, (int)(cx - w / 2), (int)(cy - w / 2), w, w, colour);
}

/* Fills x0..x1 by y0..y1 (inclusive), clipped to the canvas first so the
 * ints handed on are small. */
static void fill_box(Canvas *c, wide_t x0, wide_t y0, wide_t x1, wide_t y1,
                     uint16_t colour) {
  x0 = max_w(x0, 0);
  y0 = max_w(y0, 0);
  x1 = min_w(x1, (wide_t)c->w - 1);
  y1 = min_w(y1, (wide_t)c->h - 1);
  if (x0 > x1 || y0 > y1) return;
  canvas_fill_rect(c, (int)x0, (int)y0, (int)(x1 - x0 + 1), (int)(y1 - y0 + 1),
                   colour);
}

/* Axis-aligned segment as one rectangle, caps included. */
static void axis_aligned(Canvas *c, wide_t x1, wide_t y1, wide_t x2, wide_t y2,
                         int cap1, int cap2, int w, uint16_t colour) {
  const wide_t before = w / 2;
  const wide_t ext = w / 2;
  if (y1 == y2) {
    const int lo_cap = x1 <= x2 ? cap1 : cap2, hi_cap = x1 <= x2 ? cap2 : cap1;
    const wide_t xa = min_w(x1, x2) - (lo_cap == CapProjecting ? ext : 0);
    const wide_t xb = max_w(x1, x2) + (hi_cap == CapProjecting ? ext : 0);
    fill_box(c, xa, y1 - before, xb, y1 - before + w - 1, colour);
  } else {
    const int lo_cap = y1 <= y2 ? cap1 : cap2, hi_cap = y1 <= y2 ? cap2 : cap1;
    const wide_t ya = min_w(y1, y2) - (lo_cap == CapProjecting ? ext : 0);
    const wide_t yb = max_w(y1, y2) + (hi_cap == CapProjecting ? ext : 0);
    fill_box(c, x1 - before, ya, x1 - before + w - 1, yb, colour);
  }
}

/* Clips the segment to the canvas grown by margin, in double, because the
 * ends can be anywhere in the int range. Returns 0 when nothing is left. */
static int clip_to_canvas(const Canvas *c, wide_t margin, double *x1,
                          double *y1, double *x2, double *y2) {
  const double xmin = -(double)margin, xmax = (double)c->w + margin;
  const double ymin = -(double)margin, ymax = (double)c->h + margin;
  const double dx = *x2 - *x1, dy = *y2 - *y1;
  const double p[4] = {-dx, dx, -dy, dy};
  const double q[4] = {*x1 - xmin, xmax - *x1, *y1 - ymin, ymax - *y1};
  double t0 = 0, t1 = 1;
  for (int i = 0; i < 4; i++) {
    if (p[i] == 0) {
      if (q[i] < 0) return 0;
    } else {
      const double t = q[i] / p[i];
      if (p[i] < 0) {
        if (t > t1) return 0;
        if (t > t0) t0 = t;
      } else {
        if (t < t0) return 0;
        if (t < t1) t1 = t;
      }
    }
  }
  const double ax = *x1 + t0 * dx, ay = *y1 + t0 * dy;
  const double bx = *x1 + t1 * dx, by = *y1 + t1 * dy;
  *x1 = ax;
  *y1 = ay;
  *x2 = bx;
  *y2 = by;
  return 1;
}

/* Fills a convex polygon whose vertices are in pixel coordinates, where the
 * pixel at x, y has its centre at x + 0.5, y + 0.5. A pixel is drawn when its
 * centre is inside, the left and top edges excluded and the right and bottom
 * edges included. The vertices stay floats: canvas_fill_polygon takes ints,
 * which would move the edges of a thin line by up to half a pixel. */
static void fill_convex(Canvas *c, const float pts[][2], int n,
                        uint16_t colour) {
  float ymin = pts[0][1], ymax = ymin;
  for (int i = 1; i < n; i++) {
    ymin = fminf(ymin, pts[i][1]);
    ymax = fmaxf(ymax, pts[i][1]);
  }
  const wide_t y0 = max_w(0, (wide_t)floorf(ymin));
  const wide_t y1 = min_w((wide_t)c->h - 1, (wide_t)ceilf(ymax));
  for (wide_t y = y0; y <= y1; y++) {
    const float sy = (float)y + 0.5f;
    float xl = INFINITY, xr = -INFINITY;
    for (int i = 0, j = n - 1; i < n; j = i++) {
      const float yi = pts[i][1], yj = pts[j][1];
      if ((yi < sy && yj >= sy) || (yj < sy && yi >= sy)) {
        const float x = pts[i][0] + (sy - yi) / (yj - yi) * (pts[j][0] - pts[i][0]);
        xl = fminf(xl, x);
        xr = fmaxf(xr, x);
      }
    }
    if (xl > xr) continue;
    fill_box(c, (wide_t)floorf(xl - 0.5f) + 1, y, (wide_t)floorf(xr - 0.5f), y,
             colour);
  }
}

/* Where a vertex's line runs, in pixel coordinates (see stroke.h). */
static float centre_offset(int w) { return (w & 1) ? 0.5f : 0.0f; }

static vec2 vertex(int x, int y, int w) {
  const float o = centre_offset(w);
  return (vec2){(float)x + o, (float)y + o};
}

/* A segment between two vertices already in pixel coordinates, as a quad. An
 * end with a cap is lengthened enough to cover the pixel at the vertex (half a
 * pixel on an axis-aligned line; up to 0.71 on a diagonal when the width is
 * even and the vertex is a pixel corner), and by half the width more for a
 * projecting cap. An end that meets another segment (STROKE_NO_CAP) is not
 * lengthened at all. */
static void segment_quad(Canvas *c, vec2 a, vec2 b, int cap1, int cap2, int w,
                         uint16_t colour) {
  const float dx = b.x - a.x, dy = b.y - a.y;
  const float len = sqrtf(dx * dx + dy * dy);
  if (len < 0.001f) return;
  const float ux = dx / len, uy = dy / len;
  const float half = (float)w / 2.0f;
  const float pixel_along = (0.5f - centre_offset(w)) * (ux + uy);
  const float e1 = cap1 == STROKE_NO_CAP ? 0.0f
                   : fmaxf(0.5f, -pixel_along) + (cap1 == CapProjecting ? half : 0.0f);
  const float e2 = cap2 == STROKE_NO_CAP ? 0.0f
                   : fmaxf(0.5f, pixel_along) + (cap2 == CapProjecting ? half : 0.0f);
  const float nx = -uy * half, ny = ux * half;
  const vec2 p = {a.x - ux * e1, a.y - uy * e1};
  const vec2 q = {b.x + ux * e2, b.y + uy * e2};
  const float quad[4][2] = {
      {p.x + nx, p.y + ny}, {q.x + nx, q.y + ny}, {q.x - nx, q.y - ny},
      {p.x - nx, p.y - ny}};
  fill_convex(c, quad, 4, colour);
}

/* One slanted segment whose ends can be anywhere in the int range. Ends cut by
 * the clip are off screen anyway. */
static void slanted(Canvas *c, wide_t ix1, wide_t iy1, wide_t ix2, wide_t iy2,
                    int cap1, int cap2, int w, uint16_t colour) {
  double x1 = (double)ix1, y1 = (double)iy1, x2 = (double)ix2, y2 = (double)iy2;
  if (!clip_to_canvas(c, (wide_t)w + 2, &x1, &y1, &x2, &y2)) return;
  const float o = centre_offset(w);
  segment_quad(c, (vec2){(float)x1 + o, (float)y1 + o},
               (vec2){(float)x2 + o, (float)y2 + o}, cap1, cap2, w, colour);
}

void stroke_segment_ends(Canvas *c, const struct XshimGC *gc, int x1, int y1,
                         int x2, int y2, int cap_start, int cap_end) {
  const int w = gc->line_width;
  const uint16_t colour = (uint16_t)gc->foreground;

  if (cap_start == CapRound) disc(c, x1, y1, w, colour);
  if (cap_end == CapRound) disc(c, x2, y2, w, colour);

  if (x1 == x2 && y1 == y2) {
    if (cap_start == CapProjecting || cap_end == CapProjecting)
      axis_aligned(c, x1, y1, x2, y2, cap_start, cap_end, w, colour);
    return;
  }
  if (x1 == x2 || y1 == y2)
    axis_aligned(c, x1, y1, x2, y2, cap_start, cap_end, w, colour);
  else
    slanted(c, x1, y1, x2, y2, cap_start, cap_end, w, colour);
}

void stroke_segment(Canvas *c, const struct XshimGC *gc, int x1, int y1, int x2,
                    int y2) {
  stroke_segment_ends(c, gc, x1, y1, x2, y2, gc->cap_style, gc->cap_style);
}

/* The corner between a segment arriving along d1 and one leaving along d2
 * (unit vectors), at vertex (ix, iy). */
static void join_at(Canvas *c, int ix, int iy, vec2 d1, vec2 d2, int w,
                    int style, uint16_t colour) {
  const float dot = d1.x * d2.x + d1.y * d2.y;
  const float cross = d1.x * d2.y - d1.y * d2.x;
  if (fabsf(cross) < 1e-4f && dot > 0) return;
  if (style == JoinRound) {
    disc(c, ix, iy, w, colour);
    return;
  }
  const vec2 v = vertex(ix, iy, w);
  const float half = (float)w / 2.0f;
  const float side = cross > 0 ? -1.0f : 1.0f;
  const vec2 n1 = {-d1.y * side, d1.x * side}, n2 = {-d2.y * side, d2.x * side};
  const vec2 p1 = {v.x + n1.x * half, v.y + n1.y * half};
  const vec2 p2 = {v.x + n2.x * half, v.y + n2.y * half};
  /* Xlib's miter limit of 10: the miter is longer than ten widths when the
   * segments meet at less than about 11.5 degrees, i.e. dot < -0.98. */
  if (style == JoinMiter && dot >= -0.98f) {
    const float k = half / (1.0f + dot);
    const float quad[4][2] = {{v.x, v.y},
                              {p1.x, p1.y},
                              {v.x + (n1.x + n2.x) * k, v.y + (n1.y + n2.y) * k},
                              {p2.x, p2.y}};
    fill_convex(c, quad, 4, colour);
    return;
  }
  const float tri[3][2] = {{v.x, v.y}, {p1.x, p1.y}, {p2.x, p2.y}};
  fill_convex(c, tri, 3, colour);
}

static vec2 direction(const int *from, const int *to) {
  const float dx = (float)((wide_t)to[0] - from[0]);
  const float dy = (float)((wide_t)to[1] - from[1]);
  const float len = sqrtf(dx * dx + dy * dy);
  return (vec2){dx / len, dy / len};
}

void stroke_polyline(Canvas *c, const struct XshimGC *gc, const int *xy, int n,
                     int closed) {
  if (n <= 0) return;
  const int w = gc->line_width;
  const uint16_t colour = (uint16_t)gc->foreground;

  int stack_v[2 * 64];
  int *v = stack_v;
  if (n > 64) {
    v = (int *)malloc((size_t)n * 2 * sizeof(int));
    if (!v) return;
  }
  int m = 0;
  for (int i = 0; i < n; i++) {
    if (m > 0 && xy[2 * i] == v[2 * (m - 1)] && xy[2 * i + 1] == v[2 * (m - 1) + 1])
      continue;
    v[2 * m] = xy[2 * i];
    v[2 * m + 1] = xy[2 * i + 1];
    m++;
  }
  if (closed && m > 1 && v[2 * (m - 1)] == v[0] && v[2 * (m - 1) + 1] == v[1]) m--;

  if (m == 1 || (m == 2 && !closed)) {
    stroke_segment(c, gc, v[0], v[1], v[2 * (m - 1)], v[2 * (m - 1) + 1]);
  } else {
    const int nseg = closed ? m : m - 1;
    for (int i = 0; i < nseg; i++) {
      const int *a = v + 2 * i, *b = v + 2 * ((i + 1) % m);
      const int cap1 = (!closed && i == 0) ? gc->cap_style : STROKE_NO_CAP;
      const int cap2 = (!closed && i == nseg - 1) ? gc->cap_style : STROKE_NO_CAP;
      if (cap1 == CapRound) disc(c, a[0], a[1], w, colour);
      if (cap2 == CapRound) disc(c, b[0], b[1], w, colour);
      segment_quad(c, vertex(a[0], a[1], w), vertex(b[0], b[1], w), cap1, cap2, w,
                   colour);
    }
    const int first = closed ? 0 : 1, last = closed ? m : m - 1;
    for (int j = first; j < last; j++) {
      const int *prev = v + 2 * ((j + m - 1) % m), *at = v + 2 * j;
      const int *next = v + 2 * ((j + 1) % m);
      join_at(c, at[0], at[1], direction(prev, at), direction(at, next), w,
              gc->join_style, colour);
    }
  }
  if (v != stack_v) free(v);
}
