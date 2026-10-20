/* Builds the unmodified phosphor.c on the board.
 *
 * Upstream reads its text from a program on a pty (xscreensaver-text), and the
 * device has neither. This file swaps in a text source of its own: it `cat`s
 * a public-domain poem, picked at random, typed a character a frame
 * (assets/phosphor-poems.txt, made into phosphor_poems.h by
 * tools/make_phosphor_poems.py), then clears and starts another. The shared
 * textclient.h stand-in stays as it is (XMatrix treats its NULL as "no text").
 *
 * The font is the builtin 6x10 one (images/gen/6x10font_png.h, a blob), and
 * upstream compiles in the Xft types whatever the font, so xft.h comes first
 * as it does for XJack. */
#include "screenhack.h"

#include "textclient.h"
#include "x11shim/pixmap.h"
#include "x11shim/xft.h"

typedef unsigned long Time;
#ifndef FALSE
#define FALSE 0
#endif

#include "hacks/phosphor_poems.h"
#include "hacks/phosphor_text.h"

static text_data *phosphor_text_open(Display *dpy) {
  (void)dpy;
  return phosphor_text_create(kPoems, POEM_COUNT);
}

static void phosphor_text_close(text_data *td) { free(td); }

static void phosphor_text_reshape(text_data *td, int pix_w, int pix_h, int char_w,
                                  int char_h, int max_lines) {
  (void)td, (void)pix_w, (void)pix_h, (void)char_w, (void)char_h, (void)max_lines;
}

static int phosphor_text_getc(text_data *td) { return phosphor_text_next(td); }

static Bool phosphor_text_puts(text_data *td, const char *s) {
  (void)td, (void)s;
  return False;
}

static Bool phosphor_text_putc_event(text_data *td, XKeyEvent *event) {
  (void)td, (void)event;
  return False;
}

#define textclient_open phosphor_text_open
#define textclient_close phosphor_text_close
#define textclient_reshape phosphor_text_reshape
#define textclient_getc phosphor_text_getc
#define textclient_puts phosphor_text_puts
#define textclient_putc_event phosphor_text_putc_event

/* Upstream fades a glyph's glow by filling its cell through a clip mask, and
 * the shim's XFillRectangle ignores the mask (changing that would move every
 * hack that sets one), so this file asks for the clipped fill itself. */
static int phosphor_fill_rect(Display *dpy, Drawable d, GC gc, int x, int y,
                              unsigned int w, unsigned int h) {
  if (xshim_fill_rect_clipped(dpy, d, gc, x, y, w, h)) return 0;
  return XFillRectangle(dpy, d, gc, x, y, w, h);
}
#define XFillRectangle phosphor_fill_rect

/* capture_font_bits runs twice and stores its GCs in the state both times, so
 * the first pair's three are never freed. Every GC the hack makes goes on this
 * list, and what upstream's free leaves is freed after it. */
#define MAX_GCS 64
static GC phosphor_gcs[MAX_GCS];

static GC phosphor_create_gc(Display *dpy, Drawable d, unsigned long mask,
                             XGCValues *values) {
  GC gc = XCreateGC(dpy, d, mask, values);
  for (int i = 0; i < MAX_GCS; i++)
    if (!phosphor_gcs[i]) {
      phosphor_gcs[i] = gc;
      break;
    }
  return gc;
}

static int phosphor_free_gc(Display *dpy, GC gc) {
  for (int i = 0; i < MAX_GCS; i++)
    if (phosphor_gcs[i] == gc) phosphor_gcs[i] = NULL;
  return XFreeGC(dpy, gc);
}
#define XCreateGC phosphor_create_gc
#define XFreeGC phosphor_free_gc

/* phosphor.c registers itself at its end; this file does it instead. */
#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)

#include "phosphor/phosphor.c"

/* Upstream's free leaves the tty's grid, the Xft handle, the GCs above and
 * three of its four glyph sets' structs (it frees only `chars`); the glyph
 * pixmaps are the display's and the runner frees them. */
static void phosphor_free_all(Display *dpy, Window window, void *closure) {
  p_state *state = (p_state *)closure;
  void *grid = state->tty->grid;
  XftDraw *xftdraw = state->xftdraw;
  p_char *kept[3 * 256];
  for (int i = 0; i < countof(state->chars); i++) {
    kept[3 * i] = state->ichars[i];
    kept[3 * i + 1] = state->schars[i];
    kept[3 * i + 2] = state->sichars[i];
  }
  phosphor_free(dpy, window, closure);
  for (int i = 0; i < 3 * 256; i++) free(kept[i]);
  free(grid);
  XftDrawDestroy(xftdraw);
  for (int i = 0; i < MAX_GCS; i++)
    if (phosphor_gcs[i]) {
      XFreeGC(dpy, phosphor_gcs[i]);
      phosphor_gcs[i] = NULL;
    }
}

/* Upstream's scale of 6 gives a 12 by 7 grid on 466 pixels. A cache of every
 * fade of every glyph is about 20 MB at scale 3 against 8 MB of PSRAM. */
static const char *const phosphor_overrides[] = {"*phosphorScale: 3",
                                                 "*cache: False", 0};

const HackEntry phosphor_hack = {"Phosphor",     phosphor_defaults, phosphor_init,
                                 phosphor_draw,  phosphor_free_all, NULL,
                                 phosphor_overrides};
