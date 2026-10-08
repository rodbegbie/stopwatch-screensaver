#include "runner/hack_runner.h"

#include <stdlib.h>

#include "hacks/registry.h"
#include "screenhackI.h"
#include "x11shim/xshim.h"

#define RUNNER_WINDOW ((Window)1)

struct HackRunner {
  Canvas *canvas;
  Display *dpy;
  const HackEntry *const *hacks;
  int count;
  int index;
  void *closure;
  int running;
};

HackRunner *runner_create_with(Canvas *canvas, const HackEntry *const *hacks,
                               int count) {
  HackRunner *r = (HackRunner *)calloc(1, sizeof(*r));
  if (!r) return NULL;
  r->canvas = canvas;
  r->dpy = xshim_open_display(canvas);
  r->hacks = hacks;
  r->count = count;
  if (!r->dpy) {
    free(r);
    return NULL;
  }
  return r;
}

HackRunner *runner_create(Canvas *canvas) {
  return runner_create_with(canvas, g_hacks, g_hack_count);
}

typedef struct {
  const char *const *defaults;
  void *(*init)(Display *, Window);
  unsigned long (*draw)(Display *, Window, void *);
  void (*free)(Display *, Window, void *);
  void *setup_arg; /* xlockmore_init's hidden third argument; NULL otherwise */
  int has_setup_arg;
} Callbacks;

/* An xlockmore hack's table is empty until its setup_cb has run, and that
 * allocates, so run it once rather than on every button press. */
static Callbacks callbacks_for(const HackEntry *e) {
  struct xscreensaver_function_table *t = e->xsft;
  if (!t) return (Callbacks){e->defaults, e->init, e->draw, e->free, NULL, 0};
  if (!t->init_cb && t->setup_cb) t->setup_cb(t, t->setup_arg);
  return (Callbacks){t->defaults, t->init_cb, t->draw_cb, t->free_cb,
                     t->setup_arg, 1};
}

static void stop(HackRunner *r) {
  if (!r->running) return;
  callbacks_for(r->hacks[r->index]).free(r->dpy, RUNNER_WINDOW, r->closure);
  r->closure = NULL;
  r->running = 0;
}

void runner_destroy(HackRunner *r) {
  if (!r) return;
  stop(r);
  xshim_close_display(r->dpy);
  free(r);
}

int runner_start(HackRunner *r, int index) {
  if (index < 0 || index >= r->count) return -1;
  stop(r);
  r->index = index;
  canvas_clear(r->canvas, 0);
  const Callbacks cb = callbacks_for(r->hacks[index]);
  xshim_set_defaults(cb.defaults);
  xshim_set_overrides(r->hacks[index]->overrides);
  if (cb.has_setup_arg) {
    /* Like screenhack.c: init_cb is declared with two arguments, but the
     * xlockmore one takes its function table as a third. */
    void *(*init3)(Display *, Window, void *) =
        (void *(*)(Display *, Window, void *))cb.init;
    r->closure = init3(r->dpy, RUNNER_WINDOW, cb.setup_arg);
  } else {
    r->closure = cb.init(r->dpy, RUNNER_WINDOW);
  }
  r->running = 1;
  return 0;
}

unsigned long runner_step(HackRunner *r) {
  if (!r->running) return RUNNER_MAX_DELAY_US;
  unsigned long d =
      callbacks_for(r->hacks[r->index]).draw(r->dpy, RUNNER_WINDOW, r->closure);
  if (d < RUNNER_MIN_DELAY_US) return RUNNER_MIN_DELAY_US;
  if (d > RUNNER_MAX_DELAY_US) return RUNNER_MAX_DELAY_US;
  return d;
}

unsigned long runner_remaining_delay_us(unsigned long requested_us,
                                        unsigned long spent_us) {
  return requested_us > spent_us ? requested_us - spent_us : 0;
}

int runner_next(HackRunner *r) {
  runner_start(r, (r->index + 1) % r->count);
  return r->index;
}

int runner_prev(HackRunner *r) {
  runner_start(r, (r->index + r->count - 1) % r->count);
  return r->index;
}

int runner_index(const HackRunner *r) { return r->index; }
