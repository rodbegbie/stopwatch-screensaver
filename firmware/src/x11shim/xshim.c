#include "x11shim/xshim.h"

#include <stdlib.h>

#include "fps.h"
#include "x11shim/pixmap.h"
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
  if (gc) gc->foreground = (mask & GCForeground) ? v->foreground : 0xFFFF;
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
  XDrawLine(dpy, d, gc, x, y, x2, y);
  XDrawLine(dpy, d, gc, x2, y, x2, y2);
  XDrawLine(dpy, d, gc, x2, y2, x, y2);
  XDrawLine(dpy, d, gc, x, y2, x, y);
  return 0;
}

int XDrawLines(Display *dpy, Drawable d, GC gc, XPoint *pts, int n, int mode) {
  (void)mode;
  for (int i = 1; i < n; i++)
    XDrawLine(dpy, d, gc, pts[i - 1].x, pts[i - 1].y, pts[i].x, pts[i].y);
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

int XFillArc(Display *dpy, Drawable d, GC gc, int x, int y, unsigned int w,
             unsigned int h, int angle1, int angle2) {
  (void)d;
  (void)angle1;
  if (angle2 < 360 * 64) return 0;
  canvas_fill_ellipse(dpy->canvas, x, y, w > 0x7FFFFFFF ? 0x7FFFFFFF : (int)w,
                      h > 0x7FFFFFFF ? 0x7FFFFFFF : (int)h,
                      (uint16_t)gc->foreground);
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
