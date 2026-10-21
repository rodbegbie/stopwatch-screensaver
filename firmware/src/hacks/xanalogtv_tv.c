/* XAnalogTV: the unmodified xanalogtv.c, drawn through the shared analogtv build
 * (analogtv_tv.c). It is built with HAVE_JWXYZ, which turns off its Xt resource
 * hack (`hack_resources`) that only passes `-no-desktop` to a grabber we do not
 * have.
 *
 * Upstream loads each channel's picture with `load_image_async`, which hands
 * back a grabbed desktop or an image file. The shim finishes a load at once and
 * paints the grab image the firmware was given (xshim_set_grab_image) over the
 * target, so the callback runs inside the call. Channels that take a "random
 * image" therefore show that one picture, with different ghosts and noise; one
 * channel in five is a test card, and channel 0 is always the colour bars.
 *
 * The picture is drawn through a centred square view (the biggest square inside
 * the round glass) and every Nth line, as Pong does, so a picture takes about
 * as long as Pong's: see pong_tv.c. The channel changes every five seconds, so
 * the picture rate matters little. */
#include "screenhack.h"

#include "analogtv.h"
#include "analogtv_tv.h"
#include "doubletime.h"
#include "grabclient.h"

#ifndef XANALOGTV_LINE_STEP
#define XANALOGTV_LINE_STEP 3
#endif
#ifndef XANALOGTV_VIEW
#define XANALOGTV_VIEW 330 /* the biggest square inside the round glass; 0: the whole window */
#endif

/* The colour bars print the machine's name; the board has none worth showing. */
static int xanalogtv_gethostname(char *name, size_t len) {
  snprintf(name, len, "StopWatch");
  return 0;
}

/* Upstream times its power-up and channel changes with gettimeofday. Reading the
 * shim's clock instead lets a test drive them: on the host a frame takes no
 * time, so the picture would stay in its power-up for the whole run. */
static int xanalogtv_gettimeofday(struct timeval *tv, void *tz) {
  const double now = double_time();
  (void)tz;
  tv->tv_sec = (time_t)now;
  tv->tv_usec = (suseconds_t)((now - (double)tv->tv_sec) * 1e6);
  return 0;
}

typedef void (*xanalogtv_loaded_cb)(Screen *, Window, Drawable, const char *, XRectangle *,
                                    void *);

/* Upstream's loader finishes later, from the main loop, and its caller marks a
 * channel "not loaded yet" after asking for the image: a callback that ran inside
 * the call would load into the wrong channel and leave one blank, which is static
 * on the screen. So the load waits, one at a time as upstream does, and finishes
 * at the start of the next draw. */
typedef struct {
  Screen *screen;
  Window window;
  Drawable target;
  xanalogtv_loaded_cb done;
  void *closure;
} xanalogtv_load;

static xanalogtv_load xanalogtv_pending;
long xanalogtv_loads_finished; /* picture loads finished since the program started: for the tests */

static void xanalogtv_load_image_async(Screen *screen, Window window, Drawable target,
                                       xanalogtv_loaded_cb done, void *closure) {
  xanalogtv_pending.screen = screen;
  xanalogtv_pending.window = window;
  xanalogtv_pending.target = target;
  xanalogtv_pending.done = done;
  xanalogtv_pending.closure = closure;
}

static void xanalogtv_finish_pending_load(void) {
  if (!xanalogtv_pending.done) return;
  const xanalogtv_load load = xanalogtv_pending;
  XRectangle geometry = {0, 0, 0, 0};
  xanalogtv_pending.done = NULL;
  load_image_async_simple(NULL, load.screen, load.window, load.target, NULL, &geometry);
  load.done(load.screen, load.window, load.target, NULL, &geometry, load.closure);
  xanalogtv_loads_finished++;
}

#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)
#define HAVE_JWXYZ
#define gethostname xanalogtv_gethostname
#define gettimeofday xanalogtv_gettimeofday
#define load_image_async xanalogtv_load_image_async
#define xanalogtv_init xanalogtv_init_upstream
#define xanalogtv_draw xanalogtv_draw_upstream
#define xanalogtv_free xanalogtv_free_upstream
#include "xanalogtv/xanalogtv.c"
#undef xanalogtv_free
#undef xanalogtv_draw
#undef xanalogtv_init
#undef load_image_async
#undef gettimeofday
#undef gethostname
#undef HAVE_JWXYZ

static void *xanalogtv_init(Display *dpy, Window window) {
  analogtv_line_step = XANALOGTV_LINE_STEP;
  analogtv_view_size = XANALOGTV_VIEW;
  xanalogtv_pending.done = NULL;
  struct state *st = xanalogtv_init_upstream(dpy, window);
  /* The test cards are only read while the channels are set up, at the end of
   * init; upstream keeps them as 32-bit images (1.4 MB at the size we ship)
   * until it is freed, and the board's PSRAM has no room for that and the
   * first picture load. Its free skips a card that is not there. */
  for (int i = 0; i < countof(st->test_patterns); i++) {
    if (st->test_patterns[i]) XDestroyImage(st->test_patterns[i]);
    st->test_patterns[i] = NULL;
  }
  return st;
}

static unsigned long xanalogtv_draw(Display *dpy, Window window, void *closure) {
  xanalogtv_finish_pending_load();
  return xanalogtv_draw_upstream(dpy, window, closure);
}

/* A hack freed before its load finished still owns the pixmap it asked for. */
static void xanalogtv_free(Display *dpy, Window window, void *closure) {
  if (xanalogtv_pending.done) {
    XFreePixmap(dpy, xanalogtv_pending.target);
    xanalogtv_pending.done = NULL;
  }
  xanalogtv_free_upstream(dpy, window, closure);
}

const HackEntry xanalogtv_hack = {"XAnalogTV", xanalogtv_defaults, xanalogtv_init,
                                  xanalogtv_draw, xanalogtv_free, NULL, NULL,
                                  0, 0, XANALOGTV_VIEW == 0};
