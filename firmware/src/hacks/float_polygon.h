#ifndef HACKS_FLOAT_POLYGON_H
#define HACKS_FLOAT_POLYGON_H

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include "core/canvas.h"
#include "x11shim/pixmap.h"
#include "x11shim/xshim.h"

/* Fills any polygon even-odd (a self-crossing outline leaves a hole), in
 * float, for a hack whose wrapper maps XFillPolygon onto it. The shim's
 * canvas_fill_polygon divides in double for every edge on every row, which the
 * S3 does in software. This keeps an edge table sorted by starting row and
 * steps each edge's x by its slope, so there is no division per row.
 *
 * A pixel is filled when its centre is inside, as the shim does it. Edges
 * crossing exactly at a half pixel can round to the neighbouring pixel. */
typedef struct {
  float x, slope;
  int y0, y1; /* the edge crosses rows y0 <= y < y1 */
} FloatEdge;

static int float_edge_order(const void *a, const void *b) {
  const FloatEdge *ea = *(const FloatEdge *const *)a, *eb = *(const FloatEdge *const *)b;
  return (ea->y0 > eb->y0) - (ea->y0 < eb->y0);
}

static void float_fill_polygon(Canvas *c, const XPoint *pts, int n, uint16_t color) {
  if (!pts || n < 3) return;
  FloatEdge stack_edges[128];
  FloatEdge *edges = stack_edges;
  const FloatEdge *stack_order[128];
  const FloatEdge **order = stack_order;
  const FloatEdge *stack_active[128];
  const FloatEdge **active = stack_active;
  float stack_xs[128];
  float *xs = stack_xs;
  FloatEdge **heap = NULL;
  if (n > 128) {
    heap = (FloatEdge **)malloc((size_t)n * (sizeof(FloatEdge) + 3 * sizeof(void *) + sizeof(float)));
    if (!heap) return;
    edges = (FloatEdge *)heap;
    order = (const FloatEdge **)(edges + n);
    active = order + n;
    xs = (float *)(active + n);
  }
  int ne = 0, ymin = INT32_MAX, ymax = INT32_MIN;
  for (int i = 0, j = n - 1; i < n; j = i++) {
    int xa = pts[j].x, ya = pts[j].y, xb = pts[i].x, yb = pts[i].y;
    if (ya == yb) continue;
    if (ya > yb) {
      const int tx = xa, ty = ya;
      xa = xb, ya = yb, xb = tx, yb = ty;
    }
    FloatEdge *e = &edges[ne++];
    e->slope = (float)(xb - xa) / (float)(yb - ya);
    e->y0 = ya;
    e->y1 = yb;
    e->x = (float)xa + 0.5f * e->slope; /* at the centre of row ya */
    if (ya < ymin) ymin = ya;
    if (yb > ymax) ymax = yb;
  }
  if (ne >= 2) {
    for (int i = 0; i < ne; i++) order[i] = &edges[i];
    qsort(order, (size_t)ne, sizeof(order[0]), float_edge_order);
    int y = ymin < 0 ? 0 : ymin;
    const int last = ymax > c->h ? c->h : ymax;
    int next = 0, na = 0;
    for (; y < last; y++) {
      while (next < ne && order[next]->y0 <= y) {
        FloatEdge *e = (FloatEdge *)order[next++];
        if (e->y0 < y) e->x += e->slope * (float)(y - e->y0);
        active[na++] = e;
      }
      int nx = 0;
      for (int k = 0; k < na;) {
        FloatEdge *e = (FloatEdge *)active[k];
        if (y >= e->y1) {
          active[k] = active[--na];
          continue;
        }
        float v = e->x;
        e->x += e->slope;
        int m = nx++;
        while (m > 0 && xs[m - 1] > v) {
          xs[m] = xs[m - 1];
          m--;
        }
        xs[m] = v;
        k++;
      }
      for (int k = 0; k + 1 < nx; k += 2) {
        const int xa = (int)ceilf(xs[k] - 0.5f);
        const int xb = (int)ceilf(xs[k + 1] - 0.5f) - 1;
        if (xb >= xa) canvas_fill_rect(c, xa, y, xb - xa + 1, 1, color);
      }
    }
  }
  free(heap);
}

/* XFillPolygon's arguments, for a wrapper to `#define XFillPolygon` onto: the
 * shape and mode are ignored, as the shim's are. A bitmap or None draws
 * nothing. */
static int float_x_fill_polygon(Display *dpy, Drawable d, GC gc, XPoint *pts,
                                int n, int shape, int mode) {
  (void)shape, (void)mode;
  Canvas *c = xshim_target(dpy, d);
  if (c) float_fill_polygon(c, pts, n, (uint16_t)gc->foreground);
  return 0;
}

#endif
