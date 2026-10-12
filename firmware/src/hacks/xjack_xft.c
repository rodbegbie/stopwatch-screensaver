/* Builds the unmodified xjack.c with the Xft stand-in already included, and
 * with its paper colour as the window background.
 *
 * Upstream's screenhack.h pulls Xft in; ours does not, because declaring its
 * types and functions there would change what every hack sees. Upstream's
 * screenhack.c also sets the window background from the `background`
 * resource, and XJack's XClearWindow relies on it: the shim clears to black
 * unless asked, which left black type on a black page.
 *
 * xjack_init sizes the page before it sets its 40 pixel margins, and xscreensaver
 * follows init with a reshape that sizes it again. Without that the page is 80
 * pixels too tall: lines typed at the bottom are clipped by the window, then
 * scroll up half drawn. This file sends the reshape. */
#include "screenhack.h"

#include "x11shim/xft.h"

/* xjack.c registers itself at its end; this file registers it instead. */
#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)

#include "xjack/xjack.c"

static void *xjack_init_on_paper(Display *dpy, Window window) {
  XSetWindowBackground(dpy, window, xshim_background_pixel());
  struct state *st = xjack_init(dpy, window);
  xjack_reshape(dpy, window, st, st->xgwa.width, st->xgwa.height);
  return st;
}

/* Upstream's 12 is for a desktop; the panel's pixels are small. The sizes the
 * font was baked at are in firmware/font_atlas.py. */
#ifndef XJACK_FONT_PX
#define XJACK_FONT_PX 16
#endif
#define XJACK_STR_(x) #x
#define XJACK_STR(x) XJACK_STR_(x)

static const char *const xjack_overrides[] = {
    "*font2: Special Elite " XJACK_STR(XJACK_FONT_PX), 0};

const HackEntry xjack_hack = {"XJack",      xjack_defaults, xjack_init_on_paper,
                              xjack_draw,   xjack_free,     NULL,
                              xjack_overrides};
