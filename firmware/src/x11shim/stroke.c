#include "x11shim/stroke.h"

#include <math.h>
#include <stdbool.h>
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

static void fill_box(Canvas *c, wide_t x0, wide_t y0, wide_t x1, wide_t y1,
                     uint16_t colour);

/* The rows of a disc of each diameter up to DISC_MAX_W, relative to its corner.
 * canvas_fill_ellipse works each row out in double precision, which the S3 does
 * in software, and a round-capped line draws two discs per segment. A disc's
 * rows do not depend on where it sits, so they are worked out once per width.
 * A row with xb < xa is empty. */
#define DISC_MAX_W 16
typedef struct {
  int16_t xa, xb;
} disc_row;
static disc_row g_disc_rows[DISC_MAX_W + 1][DISC_MAX_W];
static bool g_disc_built[DISC_MAX_W + 1];

/* The same arithmetic as canvas_fill_ellipse, for an ellipse at the origin. */
static void build_disc(int w) {
  const double centre = (w - 1) / 2.0, radius = w / 2.0;
  for (int row = 0; row < w; row++) {
    const double ny = (row - centre) / radius;
    const double t = 1.0 - ny * ny;
    disc_row d = {0, -1};
    if (t >= 0) {
      const double half = radius * sqrt(t);
      const int xa = (int)ceil(centre - half), xb = (int)floor(centre + half);
      if (xb >= xa) d = (disc_row){(int16_t)xa, (int16_t)xb};
    }
    g_disc_rows[w][row] = d;
  }
  g_disc_built[w] = true;
}

/* The alpha of the GC being drawn with, set at each public entry point. */
static int g_alpha;

/* A disc wider than the cached rows, with the same arithmetic, drawn through
 * fill_box so it blends or sets mask bits like the rest of its shape. Only
 * the rows on the canvas are worked out. */
static void wide_disc_rows(Canvas *c, wide_t x, wide_t y, int w, uint16_t colour) {
  const double centre = (w - 1) / 2.0, radius = w / 2.0;
  const wide_t first = max_w(0, -y), last = min_w(w - 1, (wide_t)c->h - 1 - y);
  for (wide_t row = first; row <= last; row++) {
    const double ny = ((double)row - centre) / radius;
    const double t = 1.0 - ny * ny;
    if (t < 0) continue;
    const double half = radius * sqrt(t);
    const wide_t xa = (wide_t)ceil(centre - half), xb = (wide_t)floor(centre + half);
    if (xb >= xa) fill_box(c, x + xa, y + row, x + xb, y + row, colour);
  }
}

/* A disc of diameter w centred on the pixel (cx, cy). Skipped when it cannot
 * reach the canvas. */
static void disc(Canvas *c, wide_t cx, wide_t cy, int w, uint16_t colour) {
  if (cx < -(wide_t)w || cy < -(wide_t)w || cx > (wide_t)c->w + w ||
      cy > (wide_t)c->h + w)
    return;
  const wide_t x = cx - w / 2, y = cy - w / 2;
  if (w > DISC_MAX_W && g_alpha) {
    wide_disc_rows(c, x, y, w, colour);
    return;
  }
  if (w < 1 || w > DISC_MAX_W) {
    canvas_fill_ellipse(c, (int)x, (int)y, w, w, colour);
    return;
  }
  if (!g_disc_built[w]) build_disc(w);
  for (int row = 0; row < w; row++) {
    const disc_row d = g_disc_rows[w][row];
    if (d.xb >= d.xa) fill_box(c, x + d.xa, y + row, x + d.xb, y + row, colour);
  }
}

static uint16_t swap16(uint16_t v) { return (uint16_t)((v << 8) | (v >> 8)); }

/* Red and blue in bits 0-4 and 11-15, green in bits 21-26, so one multiply
 * scales all three channels and the gaps take the carries. */
#define SPREAD_MASK 0x07E0F81Fu
static uint32_t spread(uint16_t v) { return ((uint32_t)v | ((uint32_t)v << 16)) & SPREAD_MASK; }
static uint16_t unspread(uint32_t v) { return (uint16_t)((v & 0xFFFFu) | (v >> 16)); }

/* The canvas holds byte-swapped RGB565, so each pixel is swapped to blend and
 * swapped back. The box is already clipped. */
static void blend_box(Canvas *c, int x, int y, int w, int h, uint16_t colour) {
  const uint32_t src = spread(swap16(colour));
  const uint32_t a = (uint32_t)g_alpha;
  for (int yy = y; yy < y + h; yy++) {
    uint16_t *row = c->px + (size_t)yy * c->w + x;
    for (int i = 0; i < w; i++) {
      const uint32_t dst = spread(swap16(row[i]));
      row[i] = swap16(unspread((((src - dst) * a >> 5) + dst) & SPREAD_MASK));
    }
  }
  canvas_mark_dirty(c, x, y, w, h);
}

/* A shape made of overlapping pieces (a polyline's segments and joins, a
 * round cap and its segment) would blend twice where they overlap. While a
 * mask is open, pieces only set bits in it, one per pixel of the canvas, and
 * close_mask blends each set pixel once. */
static uint32_t *g_mask;
static int g_mask_words;

static void open_mask(const Canvas *c) {
  g_mask_words = (c->w + 31) / 32;
  g_mask = (uint32_t *)calloc((size_t)g_mask_words * (size_t)c->h, sizeof(uint32_t));
}

static void mask_row(int y, int x0, int x1) {
  uint32_t *row = g_mask + (size_t)y * g_mask_words;
  for (int w = x0 / 32; w <= x1 / 32; w++) {
    const int lo = w == x0 / 32 ? x0 % 32 : 0;
    const int hi = w == x1 / 32 ? x1 % 32 : 31;
    const uint32_t bits = (hi == 31 ? ~0u : (1u << (hi + 1)) - 1u) & (~0u << lo);
    row[w] |= bits;
  }
}

static void close_mask(Canvas *c, uint16_t colour) {
  if (!g_mask) return;
  uint32_t *mask = g_mask;
  g_mask = NULL;
  for (int y = 0; y < c->h; y++) {
    const uint32_t *row = mask + (size_t)y * g_mask_words;
    for (int w = 0; w < g_mask_words; w++) {
      uint32_t bits = row[w];
      while (bits) {
        const int start = __builtin_ctz(bits);
        const uint32_t shifted = bits >> start;
        const int len = shifted == ~0u >> start ? 32 - start : __builtin_ctz(~shifted);
        blend_box(c, w * 32 + start, y, len, 1, colour);
        bits = start + len >= 32 ? 0 : bits & (~0u << (start + len));
      }
    }
  }
  free(mask);
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
  if (g_mask) {
    for (wide_t y = y0; y <= y1; y++) mask_row((int)y, (int)x0, (int)x1);
    return;
  }
  if (g_alpha) {
    blend_box(c, (int)x0, (int)y0, (int)(x1 - x0 + 1), (int)(y1 - y0 + 1), colour);
    return;
  }
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
 * end with a butt or round cap is lengthened enough to cover the pixel at the
 * vertex (half a pixel on an axis-aligned line; up to 0.71 on a diagonal when
 * the width is even and the vertex is a pixel corner). A projecting cap is
 * half the width past the vertex. An end that meets another segment
 * (STROKE_NO_CAP) is not lengthened at all. */
static void segment_quad(Canvas *c, vec2 a, vec2 b, int cap1, int cap2, int w,
                         uint16_t colour) {
  const float dx = b.x - a.x, dy = b.y - a.y;
  const float len = sqrtf(dx * dx + dy * dy);
  if (len < 0.001f) return;
  const float ux = dx / len, uy = dy / len;
  const float half = (float)w / 2.0f;
  const float pixel_along = (0.5f - centre_offset(w)) * (ux + uy);
  /* A butt cap must reach the vertex pixel (at least half a pixel). A
   * projecting cap reaches half the width past the vertex, which covers it,
   * plus the vertex pixel's offset when that is past the vertex. */
  const float e1 = cap1 == STROKE_NO_CAP     ? 0.0f
                   : cap1 == CapProjecting   ? half + fmaxf(0.0f, -pixel_along)
                                             : fmaxf(0.5f, -pixel_along);
  const float e2 = cap2 == STROKE_NO_CAP     ? 0.0f
                   : cap2 == CapProjecting   ? half + fmaxf(0.0f, pixel_along)
                                             : fmaxf(0.5f, pixel_along);
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
  /* The clip is in double, which the S3 does in software, and floats are exact
   * well past any canvas, so only ends far outside need it. */
  const wide_t near = (wide_t)1 << 20;
  const int small = ix1 > -near && ix1 < near && iy1 > -near && iy1 < near &&
                    ix2 > -near && ix2 < near && iy2 > -near && iy2 < near;
  if (!small && !clip_to_canvas(c, (wide_t)w + 2, &x1, &y1, &x2, &y2)) return;
  const float o = centre_offset(w);
  segment_quad(c, (vec2){(float)x1 + o, (float)y1 + o},
               (vec2){(float)x2 + o, (float)y2 + o}, cap1, cap2, w, colour);
}

static void segment_pieces(Canvas *c, const struct XshimGC *gc, int x1, int y1,
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

/* Only a round cap overlaps the segment's own body. */
void stroke_segment_ends(Canvas *c, const struct XshimGC *gc, int x1, int y1,
                         int x2, int y2, int cap_start, int cap_end) {
  g_alpha = gc->alpha;
  const int overlaps = cap_start == CapRound || cap_end == CapRound;
  if (g_alpha && overlaps) open_mask(c);
  segment_pieces(c, gc, x1, y1, x2, y2, cap_start, cap_end);
  close_mask(c, (uint16_t)gc->foreground);
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

void stroke_circle(Canvas *c, const struct XshimGC *gc, int x, int y, unsigned w) {
  const int lw = gc->line_width;
  const uint16_t colour = (uint16_t)gc->foreground;
  g_alpha = gc->alpha;
  const float o = centre_offset(lw);
  const float radius = (float)w / 2.0f;
  const float cx = (float)x + radius + o, cy = (float)y + radius + o;
  const float outer = radius + (float)lw / 2.0f;
  const float inner = radius - (float)lw / 2.0f;
  const wide_t y0 = max_w(0, (wide_t)floorf(cy - outer));
  const wide_t y1 = min_w((wide_t)c->h - 1, (wide_t)ceilf(cy + outer));
  for (wide_t row = y0; row <= y1; row++) {
    const float dy = (float)row + 0.5f - cy;
    if (fabsf(dy) >= outer) continue;
    const float ho = sqrtf(outer * outer - dy * dy);
    const wide_t xo0 = (wide_t)ceilf(cx - ho - 0.5f);
    const wide_t xo1 = (wide_t)floorf(cx + ho - 0.5f);
    if (inner <= 0.0f || fabsf(dy) >= inner) {
      fill_box(c, xo0, row, xo1, row, colour);
      continue;
    }
    const float hi = sqrtf(inner * inner - dy * dy);
    const wide_t xi0 = (wide_t)ceilf(cx - hi - 0.5f);
    const wide_t xi1 = (wide_t)floorf(cx + hi - 0.5f);
    fill_box(c, xo0, row, xi0 - 1, row, colour);
    fill_box(c, xi1 + 1, row, xo1, row, colour);
  }
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
  g_alpha = gc->alpha;

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
    if (g_alpha) open_mask(c);
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
    close_mask(c, colour);
  }
  if (v != stack_v) free(v);
}
