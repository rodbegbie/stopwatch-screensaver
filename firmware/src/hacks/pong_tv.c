/* Builds the unmodified pong.c and analogtv.c as one translation unit. analogtv.c
 * is built from the build directory's copy (tools/analogtv_patch.py): its ghost
 * filter is in float, because the taps are doubles and that made the signal
 * passes 209 of the 356 ms step, and its line loop can draw every Nth line.
 *
 * analogtv.c does not include screenhack.h (it takes `frand` from there), and
 * packs its pixels from the visual's masks as ordinary RGB565. The canvas holds
 * each pixel's bytes swapped, so the colour tables it fills in analogtv_allocate
 * are swapped once here: an OR of swapped values is the swap of the OR, so every
 * pixel it writes afterwards lands in canvas order with no work per pixel.
 *
 * Two ways to do less work per frame, both measured on the board (see
 * docs/porting-assessment): `pong_line_step` draws every Nth line of the picture
 * over the rows of the lines it skips, and `pong_view_size` tells analogtv the
 * window is a smaller square and draws it in the middle of the canvas. 330 (the
 * default) is the largest square inside the round glass, so the paddles are not
 * cut by the bezel; on the board it ran at 7.2 fps against 6.4 for the whole
 * window at step 2, and step 3 (the default) took it to 8.0. With the whole window as the view the picture repaints
 * every pixel, which lets the push overlap the next step; a smaller view leaves
 * the rest of the canvas alone, so it cannot. */
#include "screenhack.h"

#include "core/canvas.h"
#include "thread_util.h"
#include "xshm.h"

#ifndef PONG_LINE_STEP
#define PONG_LINE_STEP 3
#endif
#ifndef PONG_VIEW
#define PONG_VIEW 330 /* the biggest square inside the round glass; 0: the whole window */
#endif
#ifndef PONG_NOISE
#define PONG_NOISE "0.02" /* upstream's 0.04, dialled back */
#endif

#ifndef PONG_TICKS
#define PONG_TICKS 3 /* game steps for each picture drawn */
#endif

int pong_ticks = PONG_TICKS;
int pong_line_step = PONG_LINE_STEP;
int pong_view_size = PONG_VIEW;
long pong_lines_drawn; /* lines drawn since the program started: for the tests */
static int pong_view_xo, pong_view_yo;

static Status pong_window_attributes(Display *dpy, Window w, XWindowAttributes *a) {
  const Status s = XGetWindowAttributes(dpy, w, a);
  pong_view_xo = pong_view_yo = 0;
  if (pong_view_size > 0 && pong_view_size < a->width && pong_view_size < a->height) {
    pong_view_xo = (a->width - pong_view_size) / 2;
    pong_view_yo = (a->height - pong_view_size) / 2;
    a->width = a->height = pong_view_size;
  }
  return s;
}

static void pong_put_xshm_image(Display *dpy, Drawable d, GC gc, XImage *im, int src_x,
                                int src_y, int dest_x, int dest_y, unsigned int w,
                                unsigned int h, XShmSegmentInfo *shm) {
  put_xshm_image(dpy, d, gc, im, src_x, src_y, dest_x + pong_view_xo,
                 dest_y + pong_view_yo, w, h, shm);
}

static int pong_clear_area(Display *dpy, Window win, int x, int y, unsigned int w,
                           unsigned int h, Bool exposures) {
  return XClearArea(dpy, win, x + pong_view_xo, y + pong_view_yo, w, h, exposures);
}

#define XGetWindowAttributes pong_window_attributes
#define put_xshm_image pong_put_xshm_image
#define XClearArea pong_clear_area
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

/* Game steps and pictures. pong_draw moves the game, paints it into the signal
 * and then asks analogtv to draw the picture, which is nearly all of the time it
 * takes. At 8 pictures a second the game ran at a quarter of its pace (upstream
 * steps 30 times a second), so the picture is only drawn on every pong_ticks-th
 * call, and a call that draws nothing asks for no wait: the game steps run back
 * to back, nearer upstream's pace, and the picture shows where it has got to. */
static int pong_tick;
static int pong_drew;

static void pong_analogtv_draw(analogtv *it, double noiselevel,
                               const analogtv_reception *const *recs,
                               unsigned rec_count) {
  pong_drew = ++pong_tick >= pong_ticks;
  if (!pong_drew) return;
  pong_tick = 0;
  analogtv_draw(it, noiselevel, recs, rec_count);
}

/* pong.c registers itself at its end; this file registers it instead, so the
 * entry can carry the overrides and the overlap flag. */
#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)
#define analogtv_draw pong_analogtv_draw
#define pong_init pong_init_upstream
#define pong_draw pong_draw_upstream
#include "pong/pong.c"
#undef pong_draw
#undef pong_init
#undef analogtv_draw

static void *pong_init(Display *dpy, Window window) {
  pong_tick = pong_ticks - 1; /* the first call draws */
  return pong_init_upstream(dpy, window);
}

static unsigned long pong_draw(Display *dpy, Window window, void *closure) {
  const unsigned long delay = pong_draw_upstream(dpy, window, closure);
  return pong_drew ? delay : 0;
}

static const char *const pong_overrides[] = {"*noise: " PONG_NOISE, 0};

const HackEntry pong_hack = {"Pong", pong_defaults, pong_init, pong_draw,
                             pong_free, NULL, pong_overrides, 0, 0, PONG_VIEW == 0};
