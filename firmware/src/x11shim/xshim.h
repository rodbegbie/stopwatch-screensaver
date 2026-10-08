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

typedef struct XshimGC {
  unsigned long foreground;
} *GC;

typedef struct XshimScreen Screen;
typedef struct XshimVisual Visual;

/* background, function and line_width are accepted but ignored: drawing is
 * always GXcopy with 1-pixel lines. */
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
} XWindowAttributes;

typedef struct {
  short x, y;
} XPoint;

typedef struct {
  short x, y;
  unsigned short width, height;
} XRectangle;

typedef union {
  int type;
  struct {
    int type;
    unsigned int button;
  } xbutton;
} XEvent;

typedef struct {
  const char *option;
  const char *specifier;
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
#define DoRed 1
#define DoGreen 2
#define DoBlue 4
#define CoordModeOrigin 0
#define Complex 0

#define DefaultScreen(dpy) ((void)(dpy), 0)
#define WhitePixel(dpy, scr) ((void)(dpy), (void)(scr), 0xFFFFUL)
#define BlackPixel(dpy, scr) ((void)(dpy), (void)(scr), 0x0000UL)

extern Bool mono_p;
extern const char *progname;

Display *xshim_open_display(Canvas *canvas);
void xshim_close_display(Display *dpy);

GC XCreateGC(Display *, Drawable, unsigned long mask, XGCValues *);
int XFreeGC(Display *, GC);
int XSetForeground(Display *, GC, unsigned long pixel);
Status XGetWindowAttributes(Display *, Window, XWindowAttributes *);
int XClearWindow(Display *, Window);
int XDrawPoint(Display *, Drawable, GC, int x, int y);
int XDrawLine(Display *, Drawable, GC, int x1, int y1, int x2, int y2);
int XDrawLines(Display *, Drawable, GC, XPoint *pts, int n, int mode);
int XFillRectangle(Display *, Drawable, GC, int x, int y, unsigned int w,
                   unsigned int h);
int XFillRectangles(Display *, Drawable, GC, XRectangle *rects, int n);
/* Only full ellipses (angle2 >= 360*64) are drawn; partial arcs are ignored. */
int XFillArc(Display *, Drawable, GC, int x, int y, unsigned int w,
             unsigned int h, int angle1, int angle2);
int XFillPolygon(Display *, Drawable, GC, XPoint *pts, int n, int shape,
                 int mode);
Status XAllocColor(Display *, Colormap, XColor *);
int XFreeColors(Display *, Colormap, unsigned long *pixels, int n,
                unsigned long planes);

/* True when the event should end the hack's current picture (a button press).
 * The device has no events yet, so the runner never calls this. */
Bool screenhack_event_helper(Display *, Window, XEvent *);

/* Resources come from the running hack's defaults table. */
void xshim_set_defaults(const char *const *defaults); /* NULL-terminated */
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
