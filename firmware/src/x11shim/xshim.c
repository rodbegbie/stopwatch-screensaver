#include "x11shim/xshim.h"

#include <stdlib.h>

Bool mono_p = False;
const char *progname = "stopwatch-screensaver";

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
  free(gc);
  return 0;
}

int XSetForeground(Display *dpy, GC gc, unsigned long pixel) {
  (void)dpy;
  gc->foreground = pixel;
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

int XDrawLine(Display *dpy, Drawable d, GC gc, int x1, int y1, int x2,
              int y2) {
  (void)d;
  canvas_line(dpy->canvas, x1, y1, x2, y2, (uint16_t)gc->foreground);
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
  if (n < 3 || n > 256) return 0;
  int xy[512];
  for (int i = 0; i < n; i++) {
    xy[2 * i] = pts[i].x;
    xy[2 * i + 1] = pts[i].y;
  }
  canvas_fill_polygon(dpy->canvas, xy, n, (uint16_t)gc->foreground);
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
