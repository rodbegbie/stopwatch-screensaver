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

struct XshimPixmap;

typedef struct XshimDisplay {
  Canvas *canvas;
  /* Every pixmap a hack has made and not freed, newest first. */
  struct XshimPixmap *pixmaps;
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
  int line_width, cap_style, join_style;
  /* 0 draws opaque. 1 to 31 is the weight out of 32 given to this GC's colour
   * when a wide line or circle (stroke.c) is blended over the canvas. */
  int alpha;
} *GC;

typedef struct XshimScreen Screen;
typedef struct XshimVisual Visual;

/* function is accepted but ignored: drawing is always GXcopy. Lines honour
 * line_width, cap_style and join_style (width 0 or 1 draws a plain
 * one-pixel line). plane_mask is accepted and ignored. */
typedef struct {
  unsigned long foreground;
  unsigned long background;
  int function;
  unsigned long plane_mask;
  int line_width;
  int cap_style;
  int join_style;
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

typedef struct {
  short x1, y1, x2, y2;
} XSegment;

typedef union {
  int type;
  struct {
    int type;
  } xany;
  struct {
    int type;
    unsigned int button;
    int x, y;
    unsigned int state;
  } xbutton;
  struct {
    int type;
    int x, y;
    unsigned int state;
  } xmotion;
} XEvent;

#define ButtonPress 4
#define ButtonRelease 5
#define MotionNotify 6
#define Button1 1
#define Button2 2
#define Button3 3
#define Button4 4
#define Button5 5

typedef struct {
  char *option;
  char *specifier;
  int argKind;
  void *value;
} XrmOptionDescRec;

enum { XrmoptionNoArg, XrmoptionIsArg, XrmoptionStickyArg, XrmoptionSepArg };

#define GCFunction (1L << 0)
#define GCPlaneMask (1L << 1)
#define GCForeground (1L << 2)
#define GCBackground (1L << 3)
#define GCLineWidth (1L << 4)
#define GCCapStyle (1L << 6)
#define GCJoinStyle (1L << 7)
#define LineSolid 0
#define CapNotLast 0
#define CapButt 1
#define CapRound 2
#define CapProjecting 3
#define JoinMiter 0
#define JoinRound 1
#define JoinBevel 2
#define Nonconvex 1
#define FillSolid 0
#define FillTiled 1
#define FillStippled 2
#define FillOpaqueStippled 3
#define Convex 2
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
/* Frees every pixmap the hack made and did not free, as an X server does when
 * a client disconnects. Closing the display does it too. A hack that only
 * leaks (Pacman keeps neither the unscaled sprites nor some scaled ones) is
 * made whole by calling this when it stops. */
void xshim_release_pixmaps(Display *dpy);
/* A subsystem that holds per-hack state (the GL layer) registers a hook and is
 * told when the hack stops: xshim_release_pixmaps calls it once, with the
 * display, after freeing the pixmaps, and then clears it. */
void xshim_set_release_hook(void (*hook)(Display *dpy));

GC XCreateGC(Display *, Drawable, unsigned long mask, XGCValues *);
int XFreeGC(Display *, GC);
int XSetForeground(Display *, GC, unsigned long pixel);
/* Honours GCForeground, GCLineWidth, GCCapStyle and GCJoinStyle; the rest of
 * the mask is ignored (set a background with XSetBackground). */
int XChangeGC(Display *, GC, unsigned long mask, XGCValues *);
/* The fill style is ignored: everything is drawn solid. */
int XSetFillStyle(Display *, GC, int fill_style);
/* There is no pointer: reports False, with every output zero. */
Bool XQueryPointer(Display *, Window, Window *root_return, Window *child_return,
                   int *root_x, int *root_y, int *win_x, int *win_y,
                   unsigned int *mask_return);
/* line_style is ignored: lines are always solid. */
int XSetLineAttributes(Display *, GC, unsigned int width, int line_style,
                       int cap_style, int join_style);
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
/* Each segment is drawn on its own, with the GC's caps and no joins. */
int XDrawSegments(Display *, Drawable, GC, XSegment *segs, int n);
int XFillRectangle(Display *, Drawable, GC, int x, int y, unsigned int w,
                   unsigned int h);
int XFillRectangles(Display *, Drawable, GC, XRectangle *rects, int n);
/* Angles are in 64ths of a degree from three o'clock, counter-clockwise; a
 * sweep of 360 degrees or more is a full ellipse. A partial arc fills as a pie
 * slice. */
int XDrawArc(Display *, Drawable, GC, int x, int y, unsigned int w,
             unsigned int h, int angle1, int angle2);
int XDrawArcs(Display *, Drawable, GC, XArc *arcs, int n);
int XFillArc(Display *, Drawable, GC, int x, int y, unsigned int w,
             unsigned int h, int angle1, int angle2);
/* Each arc goes through XFillArc. */
int XFillArcs(Display *, Drawable, GC, XArc *arcs, int n);
int XFillPolygon(Display *, Drawable, GC, XPoint *pts, int n, int shape,
                 int mode);

/* A pixmap is made by XCreatePixmap (zero filled; depth 1 is a bitmap, any
 * other depth is RGB565) or image_data_to_pixmap (ximage-loader.h). The only
 * thing that writes into one is XCopyArea: drawing primitives given a pixmap
 * as their drawable still draw on the canvas. */
Pixmap XCreatePixmap(Display *, Drawable, unsigned int w, unsigned int h,
                     unsigned int depth);
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
/* To the window, the source must be a colour pixmap. To a pixmap, source and
 * destination must have the same depth (colour to colour, bitmap to bitmap).
 * Anything else draws nothing. Honours the clip mask, which is positioned in
 * destination coordinates. */
int XCopyArea(Display *, Drawable src, Drawable dst, GC, int src_x, int src_y,
              unsigned int w, unsigned int h, int dst_x, int dst_y);
/* Source must be a depth-1 pixmap and plane 1: set bits are drawn in the
 * foreground, clear bits in the background. Honours the clip mask. */
int XCopyPlane(Display *, Drawable src, Drawable dst, GC, int src_x,
               int src_y, unsigned int w, unsigned int h, int dst_x,
               int dst_y, unsigned long plane);
/* Drawing is synchronous, so there is nothing to wait for. */
int XSync(Display *, Bool discard);

Status XParseColor(Display *, Colormap, const char *spec, XColor *);
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
/* The running hack's `background` resource as a pixel, or black when it
 * defines none. The runner paints the canvas with it before init, as
 * screenhack.c paints the window. */
unsigned long xshim_background_pixel(void);
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
