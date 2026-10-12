/* Shim version of xscreensaver's erase_window: the first call draws nothing
 * and returns a state, so a hack that waits after it waits on the finished
 * picture; later calls wipe the window away. Written for this project; not
 * derived from xscreensaver's utils/erase.c. */
#include <stdlib.h>

#include "doubletime.h"
#include "erase.h"

#define ERASE_SECONDS 1.0
#define BLIND_HEIGHT 32

typedef void (*EraseEffect)(Display *dpy, Window w, GC gc, double ratio);

struct eraser_state {
  Display *dpy;
  EraseEffect effect;
  GC gc;
  int started;
  double start;
  struct eraser_state *next;
};

static void venetian_blinds(Display *dpy, Window w, GC gc, double ratio) {
  const int band = (int)(ratio * BLIND_HEIGHT + 0.5);
  for (int y = 0; y < dpy->canvas->h; y += BLIND_HEIGHT)
    XFillRectangle(dpy, w, gc, 0, y, (unsigned)dpy->canvas->w,
                   (unsigned)band);
}

static const EraseEffect k_effects[] = {venetian_blinds};
#define EFFECT_COUNT ((int)(sizeof(k_effects) / sizeof(k_effects[0])))

static int g_forced_effect = -1;

void xshim_erase_force_effect(int index) { g_forced_effect = index; }

int xshim_erase_effect_count(void) { return EFFECT_COUNT; }

void eraser_free(eraser_state *st) {
  if (!st) return;
  for (eraser_state **p = &st->dpy->erasers; *p; p = &(*p)->next) {
    if (*p == st) {
      *p = st->next;
      break;
    }
  }
  XFreeGC(st->dpy, st->gc);
  free(st);
}

void xshim_release_erasers(Display *dpy) {
  while (dpy->erasers) eraser_free(dpy->erasers);
}

eraser_state *erase_window(Display *dpy, Window w, eraser_state *st) {
  if (!st) {
    st = (eraser_state *)calloc(1, sizeof(*st));
    if (!st) {
      XClearWindow(dpy, w);
      return NULL;
    }
    const int pick = g_forced_effect >= 0 && g_forced_effect < EFFECT_COUNT
                         ? g_forced_effect
                         : (int)(random() % EFFECT_COUNT);
    st->dpy = dpy;
    st->effect = k_effects[pick];
    st->gc = XCreateGC(dpy, w, 0, NULL);
    st->next = dpy->erasers;
    dpy->erasers = st;
    return st;
  }

  const double now = double_time();
  if (!st->started) {
    st->started = 1;
    st->start = now;
    return st;
  }

  const double ratio = (now - st->start) / ERASE_SECONDS;
  if (ratio >= 1.0) {
    XClearWindow(dpy, w);
    eraser_free(st);
    return NULL;
  }
  XSetForeground(dpy, st->gc, dpy->window_background);
  st->effect(dpy, w, st->gc, ratio);
  return st;
}
