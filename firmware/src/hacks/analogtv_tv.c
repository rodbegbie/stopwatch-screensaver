/* Builds the unmodified analogtv.c once, for every hack that draws through it. It is
 * built from the build directory's copy (tools/analogtv_patch.py): its ghost
 * filter is in float, because the taps are doubles and that made the signal
 * passes 209 of Pong's 356 ms step, and its line loop can draw every Nth line.
 *
 * analogtv.c does not include screenhack.h (it takes `frand` from there), and
 * packs its pixels from the visual's masks as ordinary RGB565. The canvas holds
 * each pixel's bytes swapped, so the colour tables it fills in analogtv_allocate
 * are swapped once here: an OR of swapped values is the swap of the OR, so every
 * pixel it writes afterwards lands in canvas order with no work per pixel.
 *
 * Two ways to do less work per frame, both measured on the board (see
 * docs/porting-assessment): `analogtv_line_step` draws every Nth line of the
 * picture over the rows of the lines it skips, and `analogtv_view_size` tells
 * analogtv the window is a smaller square and draws it in the middle of the
 * canvas. Both default to upstream's behaviour; a hack sets them in its init
 * (hacks/pong_tv.c). With a view smaller than the window the picture leaves the
 * rest of the canvas alone, so the hack cannot overlap its push with the next
 * step. */
#include "screenhack.h"

#include "core/canvas.h"
#include "thread_util.h"
#include "xshm.h"

#include "analogtv_tv.h"
#include "x11shim/query_color.h"

int analogtv_line_step = 1;
int analogtv_view_size;
long analogtv_lines_drawn;
static int analogtv_view_xo, analogtv_view_yo;

static Status analogtv_window_attributes(Display *dpy, Window w, XWindowAttributes *a) {
  const Status s = XGetWindowAttributes(dpy, w, a);
  analogtv_view_xo = analogtv_view_yo = 0;
  if (analogtv_view_size > 0 && analogtv_view_size < a->width &&
      analogtv_view_size < a->height) {
    analogtv_view_xo = (a->width - analogtv_view_size) / 2;
    analogtv_view_yo = (a->height - analogtv_view_size) / 2;
    a->width = a->height = analogtv_view_size;
  }
  return s;
}

static void analogtv_put_xshm_image(Display *dpy, Drawable d, GC gc, XImage *im, int src_x,
                                    int src_y, int dest_x, int dest_y, unsigned int w,
                                    unsigned int h, XShmSegmentInfo *shm) {
  put_xshm_image(dpy, d, gc, im, src_x, src_y, dest_x + analogtv_view_xo,
                 dest_y + analogtv_view_yo, w, h, shm);
}

static int analogtv_clear_area(Display *dpy, Window win, int x, int y, unsigned int w,
                               unsigned int h, Bool exposures) {
  return XClearArea(dpy, win, x + analogtv_view_xo, y + analogtv_view_yo, w, h, exposures);
}

#define XGetWindowAttributes analogtv_window_attributes
#define put_xshm_image analogtv_put_xshm_image
#define XClearArea analogtv_clear_area
#define analogtv_allocate analogtv_allocate_upstream
#include "analogtv_patched.c"
#undef analogtv_allocate
#undef XClearArea
#undef put_xshm_image
#undef XGetWindowAttributes

analogtv *analogtv_allocate(Display *dpy, Window window);

analogtv *analogtv_allocate(Display *dpy, Window window) {
  analogtv *it = analogtv_allocate_upstream(dpy, window);
  if (it && !it->use_cmap) {
    for (int i = 0; i < ANALOGTV_CV_MAX; i++) {
      it->red_values[i] = px_swap((uint16_t)it->red_values[i]);
      it->green_values[i] = px_swap((uint16_t)it->green_values[i]);
      it->blue_values[i] = px_swap((uint16_t)it->blue_values[i]);
    }
  }
  return it;
}
