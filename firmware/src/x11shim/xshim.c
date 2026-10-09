#include "x11shim/xshim.h"

#include <math.h>
#include <stdlib.h>

#include "fps.h"
#include "x11shim/arc.h"
#include "x11shim/pixmap.h"
#include "x11shim/stroke.h"
#include "yarandom.h"

Bool mono_p = False;
const char *progname = "stopwatch-screensaver";
const char *progclass = "StopwatchScreensaver";

Display *xshim_open_display(Canvas *canvas) {
  Display *dpy = (Display *)calloc(1, sizeof(*dpy));
  if (dpy) dpy->canvas = canvas;
  return dpy;
}

void xshim_close_display(Display *dpy) { free(dpy); }

GC XCreateGC(Display *dpy, Drawable d, unsigned long mask, XGCValues *v) {
  (void)dpy;
  (void)d;
  GC gc = (GC)calloc(1, sizeof(*gc));
  if (!gc) return NULL;
  gc->foreground = (mask & GCForeground) ? v->foreground : 0xFFFF;
  gc->cap_style = CapButt;
  gc->join_style = JoinMiter;
  XChangeGC(dpy, gc, mask & ~GCForeground, v);
  return gc;
}

int XFreeGC(Display *dpy, GC gc) {
  (void)dpy;
  if (gc) xshim_gc_release_clip(gc);
  free(gc);
  return 0;
}

int XSetForeground(Display *dpy, GC gc, unsigned long pixel) {
  (void)dpy;
  gc->foreground = pixel;
  return 0;
}

int XChangeGC(Display *dpy, GC gc, unsigned long mask, XGCValues *v) {
  (void)dpy;
  if (mask & GCForeground) gc->foreground = v->foreground;
  if (mask & GCLineWidth) gc->line_width = v->line_width;
  if (mask & GCCapStyle) gc->cap_style = v->cap_style;
  if (mask & GCJoinStyle) gc->join_style = v->join_style;
  return 0;
}

int XSetLineAttributes(Display *dpy, GC gc, unsigned int width, int line_style,
                       int cap_style, int join_style) {
  (void)dpy, (void)line_style;
  gc->line_width = width > 0x7FFFFFFF ? 0x7FFFFFFF : (int)width;
  gc->cap_style = cap_style;
  gc->join_style = join_style;
  return 0;
}

Status XGetWindowAttributes(Display *dpy, Window w, XWindowAttributes *a) {
  (void)w;
  a->x = a->y = 0;
  a->width = dpy->canvas->w;
  a->height = dpy->canvas->h;
  a->depth = 16;
  a->visual = NULL;
  a->screen = NULL;
  a->colormap = 1;
  a->your_event_mask = 0;
  return 1;
}

int XClearWindow(Display *dpy, Window w) {
  (void)w;
  canvas_clear(dpy->canvas, 0);
  return 0;
}

int XDrawPoint(Display *dpy, Drawable d, GC gc, int x, int y) {
  (void)d;
  canvas_point(dpy->canvas, x, y, (uint16_t)gc->foreground);
  return 0;
}

int XDrawPoints(Display *dpy, Drawable d, GC gc, XPoint *pts, int n,
                int mode) {
  (void)mode;
  for (int i = 0; i < n; i++) XDrawPoint(dpy, d, gc, pts[i].x, pts[i].y);
  return 0;
}

int XDrawLine(Display *dpy, Drawable d, GC gc, int x1, int y1, int x2,
              int y2) {
  (void)d;
  if (gc->line_width > 1)
    stroke_segment(dpy->canvas, gc, x1, y1, x2, y2);
  else
    canvas_line(dpy->canvas, x1, y1, x2, y2, (uint16_t)gc->foreground);
  return 0;
}

/* Sums are done in 64 bits so a huge width cannot overflow int. Anything
 * past +-2^30 is far outside the canvas, so clamping it changes nothing. */
static int clamp_coord(int64_t v) {
  const int64_t lim = 1 << 30;
  return (int)(v > lim ? lim : v < -lim ? -lim : v);
}

int XDrawRectangle(Display *dpy, Drawable d, GC gc, int x, int y,
                   unsigned int w, unsigned int h) {
  int x2 = clamp_coord((int64_t)x + w);
  int y2 = clamp_coord((int64_t)y + h);
  if (gc->line_width > 1) {
    const int xy[8] = {x, y, x2, y, x2, y2, x, y2};
    stroke_polyline(dpy->canvas, gc, xy, 4, 1);
    return 0;
  }
  XDrawLine(dpy, d, gc, x, y, x2, y);
  XDrawLine(dpy, d, gc, x2, y, x2, y2);
  XDrawLine(dpy, d, gc, x2, y2, x, y2);
  XDrawLine(dpy, d, gc, x, y2, x, y);
  return 0;
}

int XDrawLines(Display *dpy, Drawable d, GC gc, XPoint *pts, int n, int mode) {
  (void)mode;
  if (gc->line_width > 1 && n > 0) {
    int stack_xy[2 * 64];
    int *xy = stack_xy;
    if (n > 64) {
      xy = (int *)malloc((size_t)n * 2 * sizeof(int));
      if (!xy) return 0;
    }
    for (int i = 0; i < n; i++) {
      xy[2 * i] = pts[i].x;
      xy[2 * i + 1] = pts[i].y;
    }
    stroke_polyline(dpy->canvas, gc, xy, n, 0);
    if (xy != stack_xy) free(xy);
    return 0;
  }
  for (int i = 1; i < n; i++)
    XDrawLine(dpy, d, gc, pts[i - 1].x, pts[i - 1].y, pts[i].x, pts[i].y);
  return 0;
}

int XDrawSegments(Display *dpy, Drawable d, GC gc, XSegment *segs, int n) {
  for (int i = 0; i < n; i++)
    XDrawLine(dpy, d, gc, segs[i].x1, segs[i].y1, segs[i].x2, segs[i].y2);
  return 0;
}

int XFillRectangle(Display *dpy, Drawable d, GC gc, int x, int y,
                   unsigned int w, unsigned int h) {
  (void)d;
  canvas_fill_rect(dpy->canvas, x, y, w > 0x7FFFFFFF ? 0x7FFFFFFF : (int)w,
                   h > 0x7FFFFFFF ? 0x7FFFFFFF : (int)h,
                   (uint16_t)gc->foreground);
  return 0;
}

int XFillRectangles(Display *dpy, Drawable d, GC gc, XRectangle *rects,
                    int n) {
  for (int i = 0; i < n; i++)
    XFillRectangle(dpy, d, gc, rects[i].x, rects[i].y, rects[i].width,
                   rects[i].height);
  return 0;
}

Bool screenhack_event_helper(Display *dpy, Window w, XEvent *event) {
  (void)dpy;
  (void)w;
  return event->type == ButtonPress;
}

#define FULL_ARC (360 * 64)

/* Room for the points of an arc on the stack; a bigger arc uses the heap. */
#define ARC_STACK_POINTS 128

int XDrawArc(Display *dpy, Drawable d, GC gc, int x, int y, unsigned int w,
             unsigned int h, int angle1, int angle2) {
  (void)d;
  const int n = arc_point_count(w, h, angle2);
  if (n < 2) return 0;
  int stack_xy[2 * ARC_STACK_POINTS];
  int *xy = stack_xy;
  if (n > ARC_STACK_POINTS) {
    xy = (int *)malloc((size_t)n * 2 * sizeof(int));
    if (!xy) return 0;
  }
  arc_points(x, y, w, h, angle1, angle2, xy, n);
  if (gc->line_width > 1) {
    stroke_polyline(dpy->canvas, gc, xy, n, angle2 >= FULL_ARC || angle2 <= -FULL_ARC);
  } else {
    for (int i = 1; i < n; i++)
      canvas_line(dpy->canvas, xy[2 * i - 2], xy[2 * i - 1], xy[2 * i],
                  xy[2 * i + 1], (uint16_t)gc->foreground);
  }
  if (xy != stack_xy) free(xy);
  return 0;
}

int XDrawArcs(Display *dpy, Drawable d, GC gc, XArc *arcs, int n) {
  for (int i = 0; i < n; i++)
    XDrawArc(dpy, d, gc, arcs[i].x, arcs[i].y, arcs[i].width, arcs[i].height,
             arcs[i].angle1, arcs[i].angle2);
  return 0;
}

int XFillArc(Display *dpy, Drawable d, GC gc, int x, int y, unsigned int w,
             unsigned int h, int angle1, int angle2) {
  (void)d;
  if (angle2 >= FULL_ARC || angle2 <= -FULL_ARC) {
    canvas_fill_ellipse(dpy->canvas, x, y, w > 0x7FFFFFFF ? 0x7FFFFFFF : (int)w,
                        h > 0x7FFFFFFF ? 0x7FFFFFFF : (int)h,
                        (uint16_t)gc->foreground);
    return 0;
  }
  const int n = arc_point_count(w, h, angle2);
  if (n < 2) return 0;
  int stack_xy[2 * (ARC_STACK_POINTS + 1)];
  int *xy = stack_xy;
  if (n + 1 > ARC_STACK_POINTS + 1) {
    xy = (int *)malloc((size_t)(n + 1) * 2 * sizeof(int));
    if (!xy) return 0;
  }
  xy[0] = (int)lrintf((float)x + (float)w / 2.0f);
  xy[1] = (int)lrintf((float)y + (float)h / 2.0f);
  arc_points(x, y, w, h, angle1, angle2, xy + 2, n);
  canvas_fill_polygon(dpy->canvas, xy, n + 1, (uint16_t)gc->foreground);
  if (xy != stack_xy) free(xy);
  return 0;
}

int XFillArcs(Display *dpy, Drawable d, GC gc, XArc *arcs, int n) {
  for (int i = 0; i < n; i++)
    XFillArc(dpy, d, gc, arcs[i].x, arcs[i].y, arcs[i].width, arcs[i].height,
             arcs[i].angle1, arcs[i].angle2);
  return 0;
}

int XFillPolygon(Display *dpy, Drawable d, GC gc, XPoint *pts, int n,
                 int shape, int mode) {
  (void)d;
  (void)shape;
  (void)mode;
  if (n < 3) return 0;
  int stack_xy[2 * 64];
  int *xy = stack_xy;
  if (n > 64) {
    xy = (int *)malloc((size_t)n * 2 * sizeof(int));
    if (!xy) return 0;
  }
  for (int i = 0; i < n; i++) {
    xy[2 * i] = pts[i].x;
    xy[2 * i + 1] = pts[i].y;
  }
  canvas_fill_polygon(dpy->canvas, xy, n, (uint16_t)gc->foreground);
  if (xy != stack_xy) free(xy);
  return 0;
}

Status XAllocColor(Display *dpy, Colormap cmap, XColor *c) {
  (void)dpy;
  (void)cmap;
  c->pixel = rgb565_from16(c->red, c->green, c->blue);
  return 1;
}

int XFreeColors(Display *dpy, Colormap cmap, unsigned long *pixels, int n,
                unsigned long planes) {
  (void)dpy;
  (void)cmap;
  (void)pixels;
  (void)n;
  (void)planes;
  return 0;
}

int XSelectInput(Display *dpy, Window w, long mask) {
  (void)dpy;
  (void)w;
  (void)mask;
  return 0;
}

void ya_rand_init(unsigned int seed) {
  if (seed) srandom(seed);
}

void fps_compute(fps_state *st, unsigned long polys, double depth) {
  (void)st;
  (void)polys;
  (void)depth;
}

void fps_draw(fps_state *st) { (void)st; }

void fps_free(fps_state *st) { (void)st; }
