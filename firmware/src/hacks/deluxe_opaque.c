/* Builds the unmodified deluxe.c with resources that suit the shim. Its
 * `transparent` default draws through colour-plane masks, which an RGB565
 * canvas cannot do, and its `doubleBuffer` default draws into pixmaps, but
 * the shim only draws on the canvas, so the copy to the window would erase
 * the picture.
 *
 * Its circles are 50 pixels wide arcs up to 700 across. The shim strokes an
 * arc as a polyline of about a thousand wide segments, each filled a row at a
 * time, and that was 94% of a frame (2 to 5 fps); stroke_circle fills the same
 * ring in two spans a row. */
#include "screenhack.h"
#include "x11shim/stroke.h"

static int deluxe_draw_arc(Display *dpy, Drawable d, GC gc, int x, int y,
                           unsigned int w, unsigned int h, int angle1,
                           int angle2) {
  if (gc->line_width > 1 && w == h && (angle2 >= 360 * 64 || angle2 <= -360 * 64)) {
    stroke_circle(dpy->canvas, gc, x, y, w);
    return 0;
  }
  return XDrawArc(dpy, d, gc, x, y, w, h, angle1, angle2);
}

#define XDrawArc deluxe_draw_arc

#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)

#include "deluxe/deluxe.c"

static const char *const kDeluxeOverrides[] = {"*transparent: False",
                                               "*doubleBuffer: False", NULL};

const HackEntry deluxe_hack = {"Deluxe", deluxe_defaults, deluxe_init,
                               deluxe_draw, deluxe_free, NULL,
                               kDeluxeOverrides};
