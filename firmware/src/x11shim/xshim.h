#ifndef X11SHIM_XSHIM_H
#define X11SHIM_XSHIM_H

/* A small Xlib look-alike that draws into a Canvas. Names, argument orders
 * and semantics follow Xlib so xscreensaver hacks compile unmodified. */

#include "core/canvas.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int Bool;
#define True 1
#define False 0
typedef int Status;

typedef unsigned long Window;
typedef unsigned long Drawable;
typedef unsigned long Colormap;

typedef struct XshimDisplay {
  Canvas *canvas;
} Display;

typedef unsigned long Pixmap;
#define None 0L

/* The clip mask is the GC's own copy: Xlib servers copy it too, and hacks
 * free the pixmap straight after XSetClipMask. */
typedef struct XshimGC {
  unsigned long foreground;
  unsigned long background;
  struct XshimPixmap *clip;
  int clip_x, clip_y;
} *GC;

typedef struct XshimScreen Screen;
typedef struct XshimVisual Visual;

/* background, function and line_width are accepted but ignored here (set a
 * background with XSetBackground): drawing is always GXcopy with 1-pixel
 * lines. */
typedef struct {
  unsigned long foreground;
  unsigned long background;
  int function;
  int line_width;
} XGCValues;

typedef struct {
  unsigned long pixel;
  unsigned short red, green, blue;
  char flags;
  char pad;
} XColor;

typedef struct {
  int x, y;
  int width, height;
  int depth;
  Visual *visual;
  Screen *screen;
  Colormap colormap;
  long your_event_mask;
} XWindowAttributes;

typedef struct {
  short x, y;
} XPoint;

typedef struct {
  short x, y;
  unsigned short width, height;
} XRectangle;

typedef struct {
  short x, y;
  unsigned short width, height;
  short angle1, angle2;
} XArc;

typedef union {
  int type;
  struct {
    int type;
    unsigned int button;
  } xbutton;
} XEvent;

typedef struct {
  char *option;
  char *specifier;
  int argKind;
  void *value;
} XrmOptionDescRec;

enum { XrmoptionNoArg, XrmoptionIsArg, XrmoptionStickyArg, XrmoptionSepArg };

#define GCFunction (1L << 0)
#define GCForeground (1L << 2)
#define GCBackground (1L << 3)
#define GCLineWidth (1L << 4)
#define GXcopy 0x3
#define ButtonPress 4
#define Expose 12
#define DoRed 1
#define DoGreen 2
#define DoBlue 4
#define CoordModeOrigin 0
#define Complex 0

#define DefaultScreen(dpy) ((void)(dpy), 0)

/* The runner has one window, number 1, whatever screen it is asked about. */
#define RootWindowOfScreen(screen) ((void)(screen), (Window)1)
#define PointerMotionMask (1L << 6)
/* Events never arrive, so selecting them does nothing. */
int XSelectInput(Display *, Window, long mask);
#define WhitePixel(dpy, scr) ((void)(dpy), (void)(scr), 0xFFFFUL)
#define BlackPixel(dpy, scr) ((void)(dpy), (void)(scr), 0x0000UL)

extern Bool mono_p;
extern const char *progname;
extern const char *progclass;

Display *xshim_open_display(Canvas *canvas);
void xshim_close_display(Display *dpy);

GC XCreateGC(Display *, Drawable, unsigned long mask, XGCValues *);
int XFreeGC(Display *, GC);
int XSetForeground(Display *, GC, unsigned long pixel);
/* Only GCForeground is honoured; the rest of the mask is ignored. */
int XChangeGC(Display *, GC, unsigned long mask, XGCValues *);
Status XGetWindowAttributes(Display *, Window, XWindowAttributes *);
int XClearWindow(Display *, Window);
int XDrawPoint(Display *, Drawable, GC, int x, int y);
/* Only CoordModeOrigin is supported. */
int XDrawPoints(Display *, Drawable, GC, XPoint *pts, int n, int mode);
int XDrawLine(Display *, Drawable, GC, int x1, int y1, int x2, int y2);
/* Outlines a (w + 1) by (h + 1) box, as Xlib does. */
int XDrawRectangle(Display *, Drawable, GC, int x, int y, unsigned int w,
                   unsigned int h);
int XDrawLines(Display *, Drawable, GC, XPoint *pts, int n, int mode);
int XFillRectangle(Display *, Drawable, GC, int x, int y, unsigned int w,
                   unsigned int h);
int XFillRectangles(Display *, Drawable, GC, XRectangle *rects, int n);
/* Only full ellipses (angle2 >= 360*64) are drawn; partial arcs are ignored. */
int XFillArc(Display *, Drawable, GC, int x, int y, unsigned int w,
             unsigned int h, int angle1, int angle2);
/* Each arc goes through XFillArc, so the same full-ellipse limit applies. */
int XFillArcs(Display *, Drawable, GC, XArc *arcs, int n);
int XFillPolygon(Display *, Drawable, GC, XPoint *pts, int n, int shape,
                 int mode);

/* Pixmaps are read-only sources for XCopyArea and XCopyPlane; the only way
 * to get one is image_data_to_pixmap (ximage-loader.h). Drawing into a pixmap
 * is not supported. A depth-1 pixmap is a bitmap, anything else is RGB565. */
int XFreePixmap(Display *, Pixmap);
/* Reports the canvas for the window and the pixmap's own size otherwise.
 * Returns 0 and writes nothing for a pixmap that does not exist. */
Status XGetGeometry(Display *, Drawable, Window *root, int *x, int *y,
                    unsigned int *w, unsigned int *h, unsigned int *border,
                    unsigned int *depth);
/* Copies the mask, so the pixmap can be freed afterwards. None clears it.
 * A pixel is drawn only where the mask has a set bit; pixels outside the mask
 * are not drawn. The mask is positioned by XSetClipOrigin. */
int XSetClipMask(Display *, GC, Pixmap mask);
int XSetClipOrigin(Display *, GC, int x, int y);
int XSetBackground(Display *, GC, unsigned long pixel);
/* Source must be a colour pixmap and the destination the window. Honours the
 * clip mask. */
int XCopyArea(Display *, Drawable src, Drawable dst, GC, int src_x, int src_y,
              unsigned int w, unsigned int h, int dst_x, int dst_y);
/* Source must be a depth-1 pixmap and plane 1: set bits are drawn in the
 * foreground, clear bits in the background. Honours the clip mask. */
int XCopyPlane(Display *, Drawable src, Drawable dst, GC, int src_x,
               int src_y, unsigned int w, unsigned int h, int dst_x,
               int dst_y, unsigned long plane);
/* Drawing is synchronous, so there is nothing to wait for. */
int XSync(Display *, Bool discard);

Status XAllocColor(Display *, Colormap, XColor *);
int XFreeColors(Display *, Colormap, unsigned long *pixels, int n,
                unsigned long planes);

/* True when the event should end the hack's current picture (a button press).
 * The device has no events yet, so the runner never calls this. */
Bool screenhack_event_helper(Display *, Window, XEvent *);

/* Resources come from the running hack's defaults table. */
void xshim_set_defaults(const char *const *defaults); /* NULL-terminated */
/* Looked up before the hack's own defaults; NULL clears them. */
void xshim_set_overrides(const char *const *overrides);
int get_integer_resource(Display *, const char *name, const char *cls);
double get_float_resource(Display *, const char *name, const char *cls);
Bool get_boolean_resource(Display *, const char *name, const char *cls);
char *get_string_resource(Display *, const char *name, const char *cls);
unsigned long get_pixel_resource(Display *, Colormap, const char *name,
                                 const char *cls);

#ifdef __cplusplus
}
#endif

#endif
