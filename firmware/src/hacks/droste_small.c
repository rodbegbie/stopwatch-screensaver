/* Builds the unmodified droste.c for the board.
 *
 * Upstream asks load_image_async_simple for a loader, polls it, and sets up its
 * input and output images the first time a poll finds the load finished, with
 * the picture's rectangle. The shim finishes a load at once and returns no
 * loader, so that branch never ran and the hack rendered from a NULL image. The
 * wrapper hands back a token on the first call and finishes on the next, with
 * the pixmap's size as the rectangle.
 *
 * Upstream loads a picture 2.5 times the window each way, "so that the deep
 * zoom is less pixellated": 5.4 MB as the 32-bit image XGetImage makes, with
 * 8 MB of PSRAM in all. The wrapper makes the pixmap the size of the window. */
#include <stdlib.h>

#include "screenhack.h"
#include "xshm.h"

#include "core/canvas.h"

#define LOWREZ_SCALE 2
#include "hacks/lowrez_put.h"

static char droste_loader_token;

static async_load_state *droste_load(async_load_state *state, Screen *screen,
                                     Window top_level, Drawable target,
                                     char **filename_ret, XRectangle *geometry_ret) {
  static Drawable loading;
  if (!state) {
    load_image_async_simple(NULL, screen, top_level, target, filename_ret, NULL);
    loading = target;
    return (async_load_state *)&droste_loader_token;
  }
  Window root;
  int x, y;
  unsigned int w = 0, h = 0, border, depth;
  XGetGeometry(xshim_current_display(), loading, &root, &x, &y, &w, &h, &border,
               &depth);
  if (geometry_ret) *geometry_ret = (XRectangle){0, 0, (unsigned short)w, (unsigned short)h};
  return NULL;
}
#define load_image_async_simple droste_load

static Pixmap droste_window_sized_pixmap(Display *dpy, Drawable d, unsigned int w,
                                         unsigned int h, unsigned int depth) {
  return XCreatePixmap(dpy, d, (w * 2 + 2) / 5 * LOWREZ_SCALE,
                       (h * 2 + 2) / 5 * LOWREZ_SCALE, depth);
}
#define XCreatePixmap droste_window_sized_pixmap

/* Half resolution: the hack is told the window is half the size and draws an
 * image of that, which lowrez_put.h puts on the window as 2 by 2 squares. It
 * wants ordinary RGB565 and the picture holds canvas-order pixels, so each
 * pixel is swapped on the way in. */
#define XGetWindowAttributes lowrez_get_attributes
#define XPutPixel(im, x, y, p) XPutPixel(im, x, y, px_swap((uint16_t)(p)))
static void droste_put_image(Display *dpy, Drawable d, GC gc, XImage *im, int sx, int sy,
                             int dx, int dy, unsigned int w, unsigned int h,
                             XShmSegmentInfo *shm) {
  (void)shm;
  lowrez_put_image(dpy, d, gc, im, sx, sy, dx, dy, w, h);
}
#define put_xshm_image droste_put_image

#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)

#include "droste/droste.c"

const HackEntry droste_hack = {"Droste", droste_defaults, droste_init,
                               droste_draw, droste_free, NULL, NULL};
