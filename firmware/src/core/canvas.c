#include "core/canvas.h"

#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* The ESP32 has a 32-bit long; clip arithmetic needs 64 bits to be safe. */
typedef int64_t wide_t;

static void mark_all_dirty(Canvas *c) {
  for (int y = 0; y < c->h; y++) {
    c->dirty_x0[y] = 0;
    c->dirty_x1[y] = (int16_t)(c->w - 1);
  }
}

/* The caller has already clipped x0..x1 and y to the canvas. */
static inline void mark_span(Canvas *c, wide_t x0, wide_t x1, wide_t y) {
  if (x0 < c->dirty_x0[y]) c->dirty_x0[y] = (int16_t)x0;
  if (x1 > c->dirty_x1[y]) c->dirty_x1[y] = (int16_t)x1;
}

int canvas_init(Canvas *c, int w, int h, void *(*alloc)(size_t)) {
  c->w = w;
  c->h = h;
  c->px = NULL;
  c->dirty_x0 = c->dirty_x1 = NULL;
  c->vis_x0 = c->vis_x1 = NULL;
  /* The dirty spans are int16_t. */
  if (w <= 0 || h <= 0 || w > INT16_MAX || h > INT16_MAX) return -1;
  c->px = (uint16_t *)alloc((size_t)w * (size_t)h * sizeof(uint16_t));
  if (!c->px) return -1;
  memset(c->px, 0, (size_t)w * (size_t)h * sizeof(uint16_t));
  c->dirty_x0 = (int16_t *)malloc((size_t)h * sizeof(int16_t));
  c->dirty_x1 = (int16_t *)malloc((size_t)h * sizeof(int16_t));
  if (!c->dirty_x0 || !c->dirty_x1) {
    canvas_free(c);
    return -1;
  }
  mark_all_dirty(c);
  return 0;
}

void canvas_free(Canvas *c) {
  free(c->px);
  free(c->dirty_x0);
  free(c->dirty_x1);
  free(c->vis_x0);
  free(c->vis_x1);
  c->px = NULL;
  c->dirty_x0 = c->dirty_x1 = NULL;
  c->vis_x0 = c->vis_x1 = NULL;
}

/* Distance from the centre to the nearest point of [2i, 2i + 2], all doubled
 * so an odd size's half-pixel centre stays whole. */
static wide_t doubled_gap(wide_t i, wide_t centre2) {
  if (centre2 < 2 * i) return 2 * i - centre2;
  if (centre2 > 2 * i + 2) return centre2 - 2 * i - 2;
  return 0;
}

int canvas_set_disc(Canvas *c, void *(*alloc)(size_t)) {
  int16_t *x0s = (int16_t *)alloc((size_t)c->h * sizeof(int16_t));
  int16_t *x1s = (int16_t *)alloc((size_t)c->h * sizeof(int16_t));
  if (!x0s || !x1s) {
    free(x0s);
    free(x1s);
    return -1;
  }
  const wide_t cx2 = c->w, cy2 = c->h, r2 = c->w < c->h ? c->w : c->h;
  for (int y = 0; y < c->h; y++) {
    const wide_t dy = doubled_gap(y, cy2);
    int x0 = 0;
    while (x0 < c->w) {
      const wide_t dx = doubled_gap(x0, cx2);
      if (dx * dx + dy * dy < r2 * r2) break;
      x0++;
    }
    x0s[y] = (int16_t)x0;
    x1s[y] = (int16_t)(c->w - 1 - x0); /* mirror image; empty if x0 == w */
  }
  free(c->vis_x0);
  free(c->vis_x1);
  c->vis_x0 = x0s;
  c->vis_x1 = x1s;
  return 0;
}

bool canvas_visible_span(const Canvas *c, int y, int *x0, int *x1) {
  if (y < 0 || y >= c->h) return false;
  if (!c->vis_x0) {
    *x0 = 0;
    *x1 = c->w - 1;
    return true;
  }
  if (c->vis_x0[y] > c->vis_x1[y]) return false;
  *x0 = c->vis_x0[y];
  *x1 = c->vis_x1[y];
  return true;
}

uint16_t px_swap(uint16_t v) { return (uint16_t)((v << 8) | (v >> 8)); }

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return px_swap((uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)));
}

uint16_t rgb565_from16(uint16_t r, uint16_t g, uint16_t b) {
  return rgb565((uint8_t)(r >> 8), (uint8_t)(g >> 8), (uint8_t)(b >> 8));
}

bool canvas_dirty_row(const Canvas *c, int y, int *x0, int *x1) {
  if (y < 0 || y >= c->h || c->dirty_x0[y] > c->dirty_x1[y]) return false;
  *x0 = c->dirty_x0[y];
  *x1 = c->dirty_x1[y];
  return true;
}

void canvas_clear_dirty(Canvas *c) {
  for (int y = 0; y < c->h; y++) {
    c->dirty_x0[y] = (int16_t)c->w;
    c->dirty_x1[y] = -1;
  }
}

void canvas_clear(Canvas *c, uint16_t color) {
  for (int i = 0, n = c->w * c->h; i < n; i++) c->px[i] = color;
  mark_all_dirty(c);
}

void canvas_point(Canvas *c, int x, int y, uint16_t color) {
  if (x < 0 || y < 0 || x >= c->w || y >= c->h) return;
  c->px[(size_t)y * c->w + x] = color;
  mark_span(c, x, x, y);
}

static void hspan(Canvas *c, wide_t x0, wide_t x1, wide_t y, uint16_t color) {
  if (y < 0 || y >= c->h || x1 < 0 || x0 >= c->w) return;
  if (x0 < 0) x0 = 0;
  if (x1 >= c->w) x1 = c->w - 1;
  if (x0 > x1) return;
  mark_span(c, x0, x1, y);
  uint16_t *row = c->px + (size_t)y * c->w;
  for (int x = (int)x0, last = (int)x1; x <= last; x++) row[x] = color;
}

void canvas_fill_rect(Canvas *c, int x, int y, int w, int h, uint16_t color) {
  if (w <= 0 || h <= 0) return;
  wide_t x1 = (wide_t)x + w - 1, y1 = (wide_t)y + h - 1;
  wide_t y0 = y < 0 ? 0 : y;
  if (y1 >= c->h) y1 = c->h - 1;
  for (wide_t yy = y0; yy <= y1; yy++) hspan(c, x, x1, yy, color);
}

/* Liang-Barsky clip of the segment to [0,w-1]x[0,h-1]; false if outside. */
static int clip_line(const Canvas *c, double *x0, double *y0, double *x1,
                     double *y1) {
  double dx = *x1 - *x0, dy = *y1 - *y0, t0 = 0, t1 = 1;
  double p[4] = {-dx, dx, -dy, dy};
  double q[4] = {*x0, (c->w - 1) - *x0, *y0, (c->h - 1) - *y0};
  for (int i = 0; i < 4; i++) {
    if (p[i] == 0) {
      if (q[i] < 0) return 0;
    } else {
      double t = q[i] / p[i];
      if (p[i] < 0) {
        if (t > t1) return 0;
        if (t > t0) t0 = t;
      } else {
        if (t < t0) return 0;
        if (t < t1) t1 = t;
      }
    }
  }
  double ax = *x0, ay = *y0;
  *x0 = ax + t0 * dx;
  *y0 = ay + t0 * dy;
  *x1 = ax + t1 * dx;
  *y1 = ay + t1 * dy;
  return 1;
}

void canvas_line(Canvas *c, int x0, int y0, int x1, int y1, uint16_t color) {
  double ax = x0, ay = y0, bx = x1, by = y1;
  int clipped = x0 < 0 || y0 < 0 || x0 >= c->w || y0 >= c->h || x1 < 0 ||
                y1 < 0 || x1 >= c->w || y1 >= c->h;
  if (clipped) {
    if (!clip_line(c, &ax, &ay, &bx, &by)) return;
    ax = floor(ax + 0.5);
    ay = floor(ay + 0.5);
    bx = floor(bx + 0.5);
    by = floor(by + 0.5);
  }
  int ix0 = (int)ax, iy0 = (int)ay, ix1 = (int)bx, iy1 = (int)by;
  int dx = abs(ix1 - ix0), sx = ix0 < ix1 ? 1 : -1;
  int dy = -abs(iy1 - iy0), sy = iy0 < iy1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    canvas_point(c, ix0, iy0, color);
    if (ix0 == ix1 && iy0 == iy1) break;
    int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      ix0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      iy0 += sy;
    }
  }
}

/* Past this the squares below could overflow 64 bits; such an ellipse is far
 * bigger than any screen, so the slow path may have it. */
#define ELLIPSE_INT_MAX 32768

/* A row of an ellipse in whole numbers (the S3 does double in software, and
 * every row of every dot paid for a sqrt, a ceil and a floor). A pixel is in
 * when its centre is: with distances from the ellipse's centre doubled to
 * clear the halves, dx2^2 * h^2 <= w^2 * (h^2 - dy2^2). This is the largest
 * |dx2| that satisfies it for a row at doubled offset dy2. The float sqrt is
 * only the starting guess; the two loops make it exact. */
static int64_t ellipse_reach2(int64_t w, int64_t h, int64_t dy2) {
  const int64_t q = h * h - dy2 * dy2;
  const int64_t limit = w * w * q;
  int64_t s = (int64_t)(sqrtf((float)q) * (float)w / (float)h);
  while (s > 0 && s * s * h * h > limit) s--;
  while ((s + 1) * (s + 1) * h * h <= limit) s++;
  return s;
}

static int64_t floor_half(int64_t n) { return (n - (n & 1)) / 2; }

static void fill_ellipse_in_double(Canvas *c, int x, int y, int w, int h,
                                   uint16_t color);

void canvas_fill_ellipse(Canvas *c, int x, int y, int w, int h,
                         uint16_t color) {
  if (w <= 0 || h <= 0) return;
  if (w > ELLIPSE_INT_MAX || h > ELLIPSE_INT_MAX) {
    fill_ellipse_in_double(c, x, y, w, h, color);
    return;
  }
  const int64_t centre2 = 2 * (int64_t)x + w - 1; /* twice the centre column */
  wide_t y0 = y < 0 ? 0 : y, y1 = (wide_t)y + h - 1;
  if (y1 >= c->h) y1 = c->h - 1;
  for (wide_t yy = y0; yy <= y1; yy++) {
    const int64_t dy2 = 2 * (yy - y) - h + 1;
    const int64_t reach = ellipse_reach2(w, h, dy2);
    hspan(c, floor_half(centre2 - reach + 1), floor_half(centre2 + reach), yy, color);
  }
}

static void fill_ellipse_in_double(Canvas *c, int x, int y, int w, int h,
                                   uint16_t color) {
  double cx = x + (w - 1) / 2.0, cy = y + (h - 1) / 2.0;
  double rx = w / 2.0, ry = h / 2.0;
  wide_t y0 = y < 0 ? 0 : y, y1 = (wide_t)y + h - 1;
  if (y1 >= c->h) y1 = c->h - 1;
  for (wide_t yy = y0; yy <= y1; yy++) {
    double ny = (yy - cy) / ry;
    double t = 1.0 - ny * ny;
    if (t < 0) continue;
    double half = rx * sqrt(t);
    wide_t xa = (wide_t)ceil(cx - half), xb = (wide_t)floor(cx + half);
    if (xb < xa) continue;
    hspan(c, xa, xb, yy, color);
  }
}

/* Insertion sort is quadratic, and with software doubles a 1000-point Pedal
 * picture spent seconds in it. Past a handful of crossings qsort wins. */
#define SORT_INSERTION_MAX 16

static int compare_doubles(const void *a, const void *b) {
  double x = *(const double *)a, y = *(const double *)b;
  return (x > y) - (x < y);
}

/* Past this the products below could outgrow 64 bits, and the polygon is far
 * off any screen: the slow path may have it. */
#define POLYGON_INT_MAX (1 << 24)

/* An edge of the polygon being filled, oriented downwards: it crosses the
 * centre line of rows ylo <= y < yhi. At row y it crosses at
 * x = xlo + (2k + 1) * dx / (2 * dy), k = y - ylo, and the first pixel at or
 * right of that is ceil(x - 0.5) = xlo + ceil(((2k + 1) * dx - dy) / (2 * dy)).
 * `col` and `rem` hold that numerator's floor quotient and remainder over
 * `den` = 2 * dy, so each row down adds 2 * dx to the numerator without a
 * division. */
typedef struct {
  int ylo, yhi, xlo, dx;
  int den, step_col, step_rem;
  int col, rem;
  int row; /* the row col and rem are for; INT_MIN before the first */
} PolyEdge;

static int64_t floor_div(int64_t a, int64_t b) {
  const int64_t q = a / b;
  return a % b < 0 ? q - 1 : q;
}

static int compare_ints(const void *a, const void *b) {
  const int x = *(const int *)a, y = *(const int *)b;
  return (x > y) - (x < y);
}

/* Even-odd fill in whole numbers (the S3 does double in software, and the old
 * fill divided in double for every edge on every row). Exact: a pixel on a
 * left edge is in and one on a right edge is out, whatever the slope. */
static void fill_polygon_in_integers(Canvas *c, const int *xy, int n, uint16_t color) {
  PolyEdge stack_edges[64];
  int stack_xs[64];
  PolyEdge *edges = stack_edges;
  int *xs = stack_xs;
  void *heap = NULL;
  if (n > 64) {
    heap = malloc((size_t)n * (sizeof(PolyEdge) + sizeof(int)));
    if (!heap) return;
    edges = (PolyEdge *)heap;
    xs = (int *)(edges + n);
  }
  int ne = 0, ymin = xy[1], ymax = xy[1];
  for (int i = 0, j = n - 1; i < n; j = i++) {
    int xa = xy[2 * i], ya = xy[2 * i + 1], xb = xy[2 * j], yb = xy[2 * j + 1];
    if (ya < ymin) ymin = ya;
    if (ya > ymax) ymax = ya;
    if (ya == yb) continue;
    if (ya > yb) {
      const int tx = xa, ty = ya;
      xa = xb, ya = yb, xb = tx, yb = ty;
    }
    PolyEdge *e = &edges[ne++];
    e->ylo = ya, e->yhi = yb, e->xlo = xa, e->dx = xb - xa;
    e->den = 2 * (yb - ya);
    e->step_col = (int)floor_div(2 * (int64_t)e->dx, e->den);
    e->step_rem = 2 * e->dx - e->step_col * e->den;
    e->row = INT_MIN;
  }
  if (ymin < 0) ymin = 0;
  if (ymax >= c->h) ymax = c->h - 1;
  for (int y = ymin; y <= ymax; y++) {
    int nx = 0;
    for (int i = 0; i < ne; i++) {
      PolyEdge *e = &edges[i];
      if (y < e->ylo || y >= e->yhi) continue;
      if (e->row == y - 1) {
        e->col += e->step_col;
        e->rem += e->step_rem;
        if (e->rem >= e->den) e->rem -= e->den, e->col++;
      } else {
        const int64_t num = (2 * (int64_t)(y - e->ylo) + 1) * e->dx - e->den / 2;
        const int64_t q = floor_div(num, e->den);
        e->col = e->xlo + (int)q;
        e->rem = (int)(num - q * e->den);
      }
      e->row = y;
      xs[nx++] = e->col + (e->rem != 0);
    }
    if (nx > SORT_INSERTION_MAX) {
      qsort(xs, (size_t)nx, sizeof(int), compare_ints);
    } else {
      for (int a = 1; a < nx; a++)
        for (int b = a; b > 0 && xs[b - 1] > xs[b]; b--) {
          const int tmp = xs[b];
          xs[b] = xs[b - 1];
          xs[b - 1] = tmp;
        }
    }
    for (int k = 0; k + 1 < nx; k += 2) hspan(c, xs[k], (wide_t)xs[k + 1] - 1, y, color);
  }
  free(heap);
}

static void fill_polygon_in_double(Canvas *c, const int *xy, int n, uint16_t color);

void canvas_fill_polygon(Canvas *c, const int *xy, int n, uint16_t color) {
  if (n < 3) return;
  for (int i = 0; i < 2 * n; i++)
    if (xy[i] > POLYGON_INT_MAX || xy[i] < -POLYGON_INT_MAX) {
      fill_polygon_in_double(c, xy, n, color);
      return;
    }
  fill_polygon_in_integers(c, xy, n, color);
}

static void fill_polygon_in_double(Canvas *c, const int *xy, int n, uint16_t color) {
  /* An edge crosses a scanline at most once, so n slots always suffice. The
   * heap is only used for big polygons, to keep the task stack small. */
  double stack_xs[64];
  double *xs = stack_xs;
  if (n > 64) {
    xs = (double *)malloc((size_t)n * sizeof(double));
    if (!xs) return;
  }
  wide_t ymin = xy[1], ymax = xy[1];
  for (int i = 1; i < n; i++) {
    if (xy[2 * i + 1] < ymin) ymin = xy[2 * i + 1];
    if (xy[2 * i + 1] > ymax) ymax = xy[2 * i + 1];
  }
  if (ymin < 0) ymin = 0;
  if (ymax >= c->h) ymax = c->h - 1;
  for (wide_t yy = ymin; yy <= ymax; yy++) {
    int nx = 0;
    double sy = yy + 0.5;
    for (int i = 0, j = n - 1; i < n; j = i++) {
      double yi = xy[2 * i + 1], yj = xy[2 * j + 1];
      if ((yi <= sy && yj > sy) || (yj <= sy && yi > sy)) {
        double t = (sy - yi) / (yj - yi);
        xs[nx++] = xy[2 * i] + t * (xy[2 * j] - xy[2 * i]);
      }
    }
    if (nx > SORT_INSERTION_MAX) {
      qsort(xs, (size_t)nx, sizeof(double), compare_doubles);
    } else {
      for (int a = 1; a < nx; a++)
        for (int b = a; b > 0 && xs[b - 1] > xs[b]; b--) {
          double tmp = xs[b];
          xs[b] = xs[b - 1];
          xs[b - 1] = tmp;
        }
    }
    for (int k = 0; k + 1 < nx; k += 2)
      hspan(c, (wide_t)ceil(xs[k] - 0.5), (wide_t)ceil(xs[k + 1] - 0.5) - 1, yy,
            color);
  }
  if (xs != stack_xs) free(xs);
}

/* Visible part of the rect as [x0,x1) x [y0,y1) in canvas coordinates; false
 * if none of it is visible. */
static bool visible_rect(const Canvas *c, int x, int y, int w, int h,
                         int *x0, int *y0, int *x1, int *y1) {
  if (w <= 0 || h <= 0) return false;
  wide_t lx = x < 0 ? 0 : x, ly = y < 0 ? 0 : y;
  wide_t rx = (wide_t)x + w, ry = (wide_t)y + h;
  if (rx > c->w) rx = c->w;
  if (ry > c->h) ry = c->h;
  if (lx >= rx || ly >= ry) return false;
  *x0 = (int)lx, *y0 = (int)ly, *x1 = (int)rx, *y1 = (int)ry;
  return true;
}

void canvas_copy_rect(const Canvas *c, int x, int y, int w, int h, uint16_t *dst) {
  int x0, y0, x1, y1;
  if (!visible_rect(c, x, y, w, h, &x0, &y0, &x1, &y1)) return;
  for (int py = y0; py < y1; py++)
    memcpy(dst + (size_t)(py - y) * w + (x0 - x), c->px + (size_t)py * c->w + x0,
           (size_t)(x1 - x0) * sizeof(uint16_t));
}

void canvas_paste_rect(Canvas *c, int x, int y, int w, int h, const uint16_t *src) {
  int x0, y0, x1, y1;
  if (!visible_rect(c, x, y, w, h, &x0, &y0, &x1, &y1)) return;
  for (int py = y0; py < y1; py++) {
    memcpy(c->px + (size_t)py * c->w + x0, src + (size_t)(py - y) * w + (x0 - x),
           (size_t)(x1 - x0) * sizeof(uint16_t));
    mark_span(c, x0, x1 - 1, py);
  }
}

void canvas_mark_dirty(Canvas *c, int x, int y, int w, int h) {
  int x0, y0, x1, y1;
  if (!visible_rect(c, x, y, w, h, &x0, &y0, &x1, &y1)) return;
  for (int py = y0; py < y1; py++) mark_span(c, x0, x1 - 1, py);
}
