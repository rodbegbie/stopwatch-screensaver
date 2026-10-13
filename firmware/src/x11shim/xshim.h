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
struct eraser_state;

typedef struct XshimDisplay {
  Canvas *canvas;
  /* Every pixmap a hack has made and not freed, newest first. */
  struct XshimPixmap *pixmaps;
  /* Every eraser state erase_window made and the hack has not freed. */
  struct eraser_state *erasers;
  /* What XClearWindow paints; XSetWindowBackground sets it. */
  unsigned long window_background;
  /* The display that was current when this one opened, for xshim_current_display. */
  struct XshimDisplay *previous;
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
  /* GXcopy, GXor or GXxor as the hack set it. The shim's own drawing always
   * copies; a wrapper that draws into bit planes reads this. */
  int function;
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
  /* Accepted and ignored: a polygon is always filled even-odd, and always
   * solid (no fill style or stipple). */
  int fill_rule;
  int fill_style;
  Pixmap stipple;
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

/* A string's or a glyph's extent: X's layout. */
typedef struct {
  short lbearing, rbearing, width, ascent, descent;
  unsigned short attributes;
} XCharStruct;

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

typedef unsigned long KeySym;
/* The values X uses. No key event ever arrives, so a hack only compares
 * against them. */
#define XK_Left 0xFF51
#define XK_Up 0xFF52
#define XK_Right 0xFF53
#define XK_Down 0xFF54
#define XK_Prior 0xFF55
#define XK_Next 0xFF56

/* The device has no keyboard: nothing makes one, and XLookupString finds no
 * characters in it. The type exists so a hack's key handler compiles. */
typedef struct {
  int type;
  unsigned int state;
  unsigned int keycode;
} XKeyEvent;

typedef union {
  int type;
  struct {
    int type;
  } xany;
  XKeyEvent xkey;
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

#define KeyPress 2
#define ButtonPress 4
#define ButtonRelease 5
#define MotionNotify 6
#define Button1 1
#define Button2 2
#define Button3 3
#define Button4 4
#define Button5 5
#define Button6 6
#define Button7 7

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
#define GCFillStyle (1L << 8)
#define GCFillRule (1L << 9)
#define GCStipple (1L << 11)
#define EvenOddRule 0
#define WindingRule 1
#define GCLineWidth (1L << 4)
#define GCCapStyle (1L << 6)
#define GCJoinStyle (1L << 7)
#define LineSolid 0
#define LineOnOffDash 1 /* accepted; lines still draw solid */
#define LineDoubleDash 2
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
#define GXxor 0x6
#define GXor 0x7
#define ButtonPress 4
#define Expose 12
#define DoRed 1
#define DoGreen 2
#define DoBlue 4
#define CoordModeOrigin 0
#define CoordModePrevious 1
#define Complex 0

#define DefaultScreen(dpy) ((void)(dpy), 0)

/* The runner's one window. Any other drawable but None is taken to be a
 * pixmap pointer, so everything that names the window must use this. */
#define XSHIM_WINDOW ((Window)1)
#define RootWindowOfScreen(screen) ((void)(screen), XSHIM_WINDOW)
/* There is one screen and it is true colour: 65,536 colours, none writable. */
#define DefaultScreenOfDisplay(dpy) ((void)(dpy), (Screen *)0)
/* The screen carries no pointer back, so this is the display opened last. */
#define DisplayOfScreen(screen) ((void)(screen), xshim_current_display())
#define CellsOfScreen(screen) ((void)(screen), 65536)
#define PointerMotionMask (1L << 6)
/* Events never arrive, so selecting them does nothing. */
int XSelectInput(Display *, Window, long mask);
#define WhitePixel(dpy, scr) ((void)(dpy), (void)(scr), 0xFFFFUL)
#define BlackPixel(dpy, scr) ((void)(dpy), (void)(scr), 0x0000UL)
#define WhitePixelOfScreen(screen) ((void)(screen), 0xFFFFUL)
#define BlackPixelOfScreen(screen) ((void)(screen), 0x0000UL)

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
/* Frees every eraser state erase_window made on this display and the hack did
 * not free, as xshim_release_pixmaps does for pixmaps. */
void xshim_release_erasers(Display *dpy);
/* Test hooks for erase_window: pin the wipe to effect `index`, or pass -1 for
 * a random one; and how many effects there are. */
void xshim_erase_force_effect(int index);
int xshim_erase_effect_count(void);
/* Test hook: what erase_window calls to make its GC; NULL restores XCreateGC. */
void xshim_erase_set_gc_factory(GC (*factory)(Display *, Drawable, unsigned long,
                                              XGCValues *));

GC XCreateGC(Display *, Drawable, unsigned long mask, XGCValues *);
int XFreeGC(Display *, GC);
/* The display xshim_open_display made most recently and has not closed. */
Display *xshim_current_display(void);
int XSetForeground(Display *, GC, unsigned long pixel);
/* Remembered in the GC's `function`, but the shim's own drawing is always
 * GXcopy (see XGCValues). */
int XSetFunction(Display *, GC, int function);
/* A GC may draw with alpha under jwxyz; here it is accepted and ignored. */
int jwxyz_XSetAlphaAllowed(Display *, GC, Bool allowed);
/* Honours GCForeground, GCBackground, GCLineWidth, GCCapStyle and
 * GCJoinStyle; the rest of the mask is ignored. */
int XChangeGC(Display *, GC, unsigned long mask, XGCValues *);
/* No expose events ever arrive: accepted and ignored. */
int XSetGraphicsExposures(Display *, GC, Bool);
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
/* A width or height of 0 runs to the window's edge, as in X. exposures is
 * ignored: no Expose event is ever sent. */
int XClearArea(Display *, Window, int x, int y, unsigned int w, unsigned int h,
               Bool exposures);
int XSetWindowBackground(Display *, Window, unsigned long pixel);
int XDrawPoint(Display *, Drawable, GC, int x, int y);
/* CoordModeOrigin and CoordModePrevious (each point an offset from the one
 * before) are supported by the point, line and polygon calls. */
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
/* data is X bitmap data: rows padded to whole bytes, the least significant bit
 * of a byte the leftmost pixel. Set bits take fg and clear bits bg; a depth of
 * 1 makes a bitmap (fg and bg ignored), any other a colour pixmap. */
Pixmap XCreatePixmapFromBitmapData(Display *, Drawable, char *data,
                                   unsigned int w, unsigned int h,
                                   unsigned long fg, unsigned long bg,
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
/* An XImage is a block of pixels in the hack's own memory; XPutImage draws it.
 * ZPixmap images are 16-bit ones (what a hack that fills in its own
 * pixels asks for) and 24 or 32-bit ones (a mask scaled down from a loaded
 * picture, or XGetImage's copy of the canvas). An XYBitmap image is depth 1,
 * one bit a pixel, least significant bit first: XPutImage draws its set bits in
 * the GC's foreground and its clear bits in the background. */
#define XYBitmap 0
#define XYPixmap 1
#define LSBFirst 0
#define MSBFirst 1
/* The server is little-endian (the S3 and the hosts the tests run on). */
#define ImageByteOrder(dpy) ((void)(dpy), LSBFirst)
/* Image rows pad to whole 32-bit words, as on a typical X server. */
#define BitmapPad(dpy) ((void)(dpy), 32)
#define ZPixmap 2
typedef struct XImage {
  int width, height;
  int depth;
  int bits_per_pixel;
  int bytes_per_line;
  char *data;
} XImage;

/* A 16-bit image is two bytes a pixel of ordinary little-endian RGB565, as on
 * an X server: XPutImage byte-swaps it into the canvas. A 24 or 32-bit image
 * is four bytes a pixel. Returns NULL for a depth other than 16, 24 or 32, a zero or oversized image, or
 * no memory. A bytes_per_line of 0 means a tight row, padded to bitmap_pad
 * bits. `data` may be NULL: the hack then points it at its own allocation of
 * height * bytes_per_line bytes, and XDestroyImage frees whatever it holds. */
XImage *XCreateImage(Display *, Visual *, unsigned int depth, int format,
                     int offset, char *data, unsigned int w, unsigned int h,
                     int bitmap_pad, int bytes_per_line);
/* A pixel outside the image, or in an image with no data, reads as 0 and
 * writes nothing. */
unsigned long XGetPixel(XImage *, int x, int y);
int XPutPixel(XImage *, int x, int y, unsigned long pixel);
int XDestroyImage(XImage *);

/* Copies a region of the window or a colour pixmap into a new image of
 * 32-bit pixels holding the raw RGB565 value. Only ZPixmap; a bitmap format,
 * None, or an empty region returns NULL. Pixels outside the drawable read 0.
 * The caller frees it with XDestroyImage. plane_mask is ignored. */
XImage *XGetImage(Display *, Drawable, int x, int y, unsigned int w,
                  unsigned int h, unsigned long plane_mask, int format);
/* Writes the low 16 bits of each pixel of the image region onto the window or
 * a colour pixmap, clipped to both. The GC (and its clip mask) is ignored. */
int XPutImage(Display *, Drawable, GC, XImage *, int src_x, int src_y,
              int dst_x, int dst_y, unsigned int w, unsigned int h);

/* Always finds no characters (see XKeyEvent) and sets *keysym to 0. */
int XLookupString(XKeyEvent *, char *buffer, int bytes, KeySym *keysym,
                  void *status);

/* Drawing is synchronous, so there is nothing to wait for. */
int XSync(Display *, Bool discard);

/* In 4-byte units, as a typical X server reports it. Nothing is sent, so the
 * only reader is a hack sizing its XDrawLines batches. */
long XMaxRequestSize(Display *);

Status XParseColor(Display *, Colormap, const char *spec, XColor *);
Status XAllocColor(Display *, Colormap, XColor *);
/* Fills in red, green and blue from c->pixel, as XAllocColor made it: the
 * channels are scaled up from RGB565, so 5 and 6 bit values come back as
 * multiples of 0x0842 and 0x0410 and not the colour that was asked for. */
int XQueryColor(Display *, Colormap, XColor *);
/* Nothing to store: colours are plain pixel values, not cells to rewrite. */
int XStoreColors(Display *, Colormap, XColor *, int ncolors);
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
