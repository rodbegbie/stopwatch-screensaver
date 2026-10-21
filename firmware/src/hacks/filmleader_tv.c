/* FilmLeader: filmleader.c drawn through the shared analogtv build
 * (analogtv_tv.c). The source is the build directory's copy
 * (tools/filmleader_patch.py): upstream draws the leader into a 712 pixel wide
 * 16:9 pixmap and analogtv stretches it over the window, which on a square panel
 * squashes the rings into ovals and costs 3 MB of pixmap and image each frame.
 * The patch makes the pixmap square and FILMLEADER_CANVAS pixels wide.
 *
 * Upstream's font sizes are in canvas pixels (170, 50 and 36 for a canvas 400
 * high), so the registry's overrides scale them to the canvas. The font is the
 * Xft stand-in's one face (Special Elite), the nearest size it holds: 140 for the
 * big number, 24 and 20 for the text.
 *
 * Like Pong, the picture is drawn through a centred square view, every Nth
 * line. The countdown is paced by the clock, not by frames, so a slow picture
 * only makes it coarser. */
#include "screenhack.h"

#include "x11shim/xft.h"

#include "analogtv.h"
#include "analogtv_tv.h"
#include "doubletime.h"

#ifndef FILMLEADER_CANVAS
#define FILMLEADER_CANVAS 356 /* half of upstream's 712 */
#endif
#ifndef FILMLEADER_LINE_STEP
#define FILMLEADER_LINE_STEP 3
#endif
#ifndef FILMLEADER_VIEW
#define FILMLEADER_VIEW 330 /* the biggest square inside the round glass; 0: the whole window */
#endif

#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)
#define filmleader_init filmleader_init_upstream
#define filmleader_free filmleader_free_upstream
#include "filmleader_patched.c"
#undef filmleader_free
#undef filmleader_init

static void *filmleader_init(Display *dpy, Window window) {
  analogtv_line_step = FILMLEADER_LINE_STEP;
  analogtv_view_size = FILMLEADER_VIEW;
  return filmleader_init_upstream(dpy, window);
}

/* Upstream's free forgets the three fonts it opened. */
static void filmleader_free(Display *dpy, Window window, void *closure) {
  struct state *st = (struct state *)closure;
  XftFontClose(dpy, st->font);
  XftFontClose(dpy, st->font2);
  XftFontClose(dpy, st->font3);
  filmleader_free_upstream(dpy, window, closure);
}

static const char *const filmleader_overrides[] = {
    "*numberFont: Helvetica Bold 140",
    "*numberFont2: Helvetica 24",
    "*numberFont3: Helvetica 20",
    0,
};

const HackEntry filmleader_hack = {"FilmLeader", filmleader_defaults, filmleader_init,
                                   filmleader_draw, filmleader_free, NULL,
                                   filmleader_overrides, 0, 0, FILMLEADER_VIEW == 0};
