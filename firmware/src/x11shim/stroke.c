#include "x11shim/stroke.h"

#include <math.h>
#include <stdint.h>

typedef int64_t wide_t;

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

/* Fills a convex polygon whose vertices are in pixel-centre coordinates (the
 * pixel at x, y has its centre at x + 0.5, y + 0.5). A pixel is drawn when its
 * centre is inside, the rule canvas_fill_polygon uses, but the vertices stay
 * floats: canvas_fill_polygon takes ints, which would move the edges of a
 * thin line by up to half a pixel. */
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
      if ((yi <= sy && yj > sy) || (yj <= sy && yi > sy)) {
        const float x = pts[i][0] + (sy - yi) / (yj - yi) * (pts[j][0] - pts[i][0]);
        xl = fminf(xl, x);
        xr = fmaxf(xr, x);
      }
    }
    if (xl > xr) continue;
    fill_box(c, (wide_t)ceilf(xl - 0.5f), y, (wide_t)ceilf(xr - 0.5f) - 1, y,
             colour);
  }
}

/* A slanted segment as a quad, lengthened by half a pixel at each end so the
 * end pixels are covered, as they are on an axis-aligned line, and by half the
 * width more at an end with a projecting cap. Ends cut by the clip are off
 * screen anyway. */
static void slanted(Canvas *c, wide_t ix1, wide_t iy1, wide_t ix2, wide_t iy2,
                    int cap1, int cap2, int w, uint16_t colour) {
  double x1 = (double)ix1, y1 = (double)iy1, x2 = (double)ix2, y2 = (double)iy2;
  if (!clip_to_canvas(c, (wide_t)w + 2, &x1, &y1, &x2, &y2)) return;
  const float fx1 = (float)x1 + 0.5f, fy1 = (float)y1 + 0.5f;
  const float fx2 = (float)x2 + 0.5f, fy2 = (float)y2 + 0.5f;
  const float dx = fx2 - fx1, dy = fy2 - fy1;
  const float len = sqrtf(dx * dx + dy * dy);
  if (len < 0.001f) return;
  const float ux = dx / len, uy = dy / len;
  const float half = (float)w / 2.0f;
  const float e1 = 0.5f + (cap1 == CapProjecting ? half : 0.0f);
  const float e2 = 0.5f + (cap2 == CapProjecting ? half : 0.0f);
  const float ax = fx1 - ux * e1, ay = fy1 - uy * e1;
  const float bx = fx2 + ux * e2, by = fy2 + uy * e2;
  const float nx = -uy * half, ny = ux * half;
  const float quad[4][2] = {
      {ax + nx, ay + ny}, {bx + nx, by + ny}, {bx - nx, by - ny},
      {ax - nx, ay - ny}};
  fill_convex(c, quad, 4, colour);
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
