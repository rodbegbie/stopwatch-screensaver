/* Shim version of xscreensaver's erase_window: the first call draws nothing
 * and returns a state, so a hack that waits after it waits on the finished
 * picture; later calls wipe the window away. Written for this project; not
 * derived from xscreensaver's utils/erase.c. */
#include <stdint.h>
#include <stdlib.h>

#include "doubletime.h"
#include "erase.h"

#define ERASE_SECONDS 1.0
#define BLIND_HEIGHT 32
#define FIZZLE_BLOCK 16
#define LINE_COUNT 160
#define LINE_WIDTH 16

struct eraser_state;
typedef void (*EraseEffect)(struct eraser_state *st, Window w, double ratio);

struct eraser_state {
  Display *dpy;
  EraseEffect effect;
  GC gc;
  int started;
  double start;
  int drawn;        /* items an incremental effect has already drawn */
  uint16_t *order;  /* fizzle: the blocks in the order they clear */
  int order_count;
  struct eraser_state *next;
};

static void venetian_blinds(eraser_state *st, Window w, double ratio) {
  const Canvas *c = st->dpy->canvas;
  const int band = (int)(ratio * BLIND_HEIGHT + 0.5);
  for (int y = 0; y < c->h; y += BLIND_HEIGHT)
    XFillRectangle(st->dpy, w, st->gc, 0, y, (unsigned)c->w, (unsigned)band);
}

static void circle_wipe(eraser_state *st, Window w, double ratio) {
  const Canvas *c = st->dpy->canvas;
  const int64_t cx = c->w / 2, cy = c->h / 2;
  const int64_t corner_sq = cx * cx + cy * cy;
  int64_t full = 0;
  while (full * full < corner_sq) full++;
  const int64_t r = (int64_t)(ratio * (double)full);
  const int64_t reach = cy > c->h - 1 - cy ? cy : c->h - 1 - cy;
  int64_t dx = r;
  for (int64_t dy = 0; dy <= r && dy <= reach; dy++) {
    while (dx * dx + dy * dy > r * r) dx--;
    const unsigned width = (unsigned)(2 * dx + 1);
    XFillRectangle(st->dpy, w, st->gc, (int)(cx - dx), (int)(cy - dy), width, 1);
    if (dy > 0)
      XFillRectangle(st->dpy, w, st->gc, (int)(cx - dx), (int)(cy + dy), width,
                     1);
  }
}

static void fizzle(eraser_state *st, Window w, double ratio) {
  const Canvas *c = st->dpy->canvas;
  const int cols = (c->w + FIZZLE_BLOCK - 1) / FIZZLE_BLOCK;
  const int rows = (c->h + FIZZLE_BLOCK - 1) / FIZZLE_BLOCK;
  if (!st->order) {
    st->order_count = cols * rows;
    st->order = (uint16_t *)malloc(sizeof(uint16_t) * (size_t)st->order_count);
    if (!st->order) {
      st->order_count = 0;
      return;
    }
    for (int i = 0; i < st->order_count; i++) st->order[i] = (uint16_t)i;
    for (int i = st->order_count - 1; i > 0; i--) {
      const int j = (int)(random() % (i + 1));
      const uint16_t t = st->order[i];
      st->order[i] = st->order[j];
      st->order[j] = t;
    }
  }
  const int target = (int)(ratio * st->order_count);
  for (; st->drawn < target; st->drawn++) {
    const int b = st->order[st->drawn];
    XFillRectangle(st->dpy, w, st->gc, (b % cols) * FIZZLE_BLOCK,
                   (b / cols) * FIZZLE_BLOCK, FIZZLE_BLOCK, FIZZLE_BLOCK);
  }
}

static void random_lines(eraser_state *st, Window w, double ratio) {
  const Canvas *c = st->dpy->canvas;
  const int target = (int)(ratio * LINE_COUNT);
  if (st->drawn >= target) return;
  XSetLineAttributes(st->dpy, st->gc, LINE_WIDTH, 0, 0, 0);
  for (; st->drawn < target; st->drawn++) {
    if (random() & 1)
      XDrawLine(st->dpy, w, st->gc, (int)(random() % c->w), 0,
                (int)(random() % c->w), c->h - 1);
    else
      XDrawLine(st->dpy, w, st->gc, 0, (int)(random() % c->h), c->w - 1,
                (int)(random() % c->h));
  }
}

static const EraseEffect k_effects[] = {venetian_blinds, circle_wipe, fizzle,
                                        random_lines};
#define EFFECT_COUNT ((int)(sizeof(k_effects) / sizeof(k_effects[0])))

static int g_forced_effect = -1;

void xshim_erase_force_effect(int index) { g_forced_effect = index; }

static GC (*g_gc_factory)(Display *, Drawable, unsigned long,
                          XGCValues *) = XCreateGC;

void xshim_erase_set_gc_factory(GC (*factory)(Display *, Drawable, unsigned long,
                                              XGCValues *)) {
  g_gc_factory = factory ? factory : XCreateGC;
}

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
  free(st->order);
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
    st->gc = g_gc_factory(dpy, w, 0, NULL);
    if (!st->gc) {
      free(st);
      XClearWindow(dpy, w);
      return NULL;
    }
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
  st->effect(st, w, ratio);
  return st;
}
