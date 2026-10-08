#include "core/canvas.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

int canvas_init(Canvas *c, int w, int h, void *(*alloc)(size_t)) {
  c->w = w;
  c->h = h;
  c->px = (uint16_t *)alloc((size_t)w * (size_t)h * sizeof(uint16_t));
  if (!c->px) return -1;
  memset(c->px, 0, (size_t)w * (size_t)h * sizeof(uint16_t));
  return 0;
}

void canvas_free(Canvas *c) {
  free(c->px);
  c->px = NULL;
}

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

uint16_t rgb565_from16(uint16_t r, uint16_t g, uint16_t b) {
  return rgb565((uint8_t)(r >> 8), (uint8_t)(g >> 8), (uint8_t)(b >> 8));
}

void canvas_clear(Canvas *c, uint16_t color) {
  for (long i = 0, n = (long)c->w * c->h; i < n; i++) c->px[i] = color;
}

void canvas_point(Canvas *c, int x, int y, uint16_t color) {
  if (x < 0 || y < 0 || x >= c->w || y >= c->h) return;
  c->px[(size_t)y * c->w + x] = color;
}

static void hspan(Canvas *c, long x0, long x1, long y, uint16_t color) {
  if (y < 0 || y >= c->h || x1 < 0 || x0 >= c->w) return;
  if (x0 < 0) x0 = 0;
  if (x1 >= c->w) x1 = c->w - 1;
  uint16_t *row = c->px + (size_t)y * c->w;
  for (long x = x0; x <= x1; x++) row[x] = color;
}

void canvas_fill_rect(Canvas *c, int x, int y, int w, int h, uint16_t color) {
  if (w <= 0 || h <= 0) return;
  long x1 = (long)x + w - 1, y1 = (long)y + h - 1;
  long y0 = y < 0 ? 0 : y;
  if (y1 >= c->h) y1 = c->h - 1;
  for (long yy = y0; yy <= y1; yy++) hspan(c, x, x1, yy, color);
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

void canvas_fill_ellipse(Canvas *c, int x, int y, int w, int h,
                         uint16_t color) {
  if (w <= 0 || h <= 0) return;
  double cx = x + (w - 1) / 2.0, cy = y + (h - 1) / 2.0;
  double rx = w / 2.0, ry = h / 2.0;
  long y0 = y < 0 ? 0 : y, y1 = (long)y + h - 1;
  if (y1 >= c->h) y1 = c->h - 1;
  for (long yy = y0; yy <= y1; yy++) {
    double ny = (yy - cy) / ry;
    double t = 1.0 - ny * ny;
    if (t < 0) continue;
    double half = rx * sqrt(t);
    long xa = (long)ceil(cx - half), xb = (long)floor(cx + half);
    if (xb < xa) continue;
    hspan(c, xa, xb, yy, color);
  }
}

void canvas_fill_polygon(Canvas *c, const int *xy, int n, uint16_t color) {
  if (n < 3) return;
  long ymin = xy[1], ymax = xy[1];
  for (int i = 1; i < n; i++) {
    if (xy[2 * i + 1] < ymin) ymin = xy[2 * i + 1];
    if (xy[2 * i + 1] > ymax) ymax = xy[2 * i + 1];
  }
  if (ymin < 0) ymin = 0;
  if (ymax >= c->h) ymax = c->h - 1;
  for (long yy = ymin; yy <= ymax; yy++) {
    double xs[64];
    int nx = 0;
    double sy = yy + 0.5;
    for (int i = 0, j = n - 1; i < n; j = i++) {
      double yi = xy[2 * i + 1], yj = xy[2 * j + 1];
      if ((yi <= sy && yj > sy) || (yj <= sy && yi > sy)) {
        double t = (sy - yi) / (yj - yi);
        if (nx < 64) xs[nx++] = xy[2 * i] + t * (xy[2 * j] - xy[2 * i]);
      }
    }
    for (int a = 1; a < nx; a++)
      for (int b = a; b > 0 && xs[b - 1] > xs[b]; b--) {
        double tmp = xs[b];
        xs[b] = xs[b - 1];
        xs[b - 1] = tmp;
      }
    for (int k = 0; k + 1 < nx; k += 2)
      hspan(c, (long)ceil(xs[k] - 0.5), (long)ceil(xs[k + 1] - 0.5) - 1, yy,
            color);
  }
}
