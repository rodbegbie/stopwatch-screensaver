/* Stand-in for xscreensaver's hacks/screenhackI.h, which the xlockmore
 * framework includes. Not derived from xscreensaver code. Field order
 * follows xscreensaver's struct so the framework's initialisers line up. */
#ifndef XSHIM_SCREENHACKI_H
#define XSHIM_SCREENHACKI_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "erase.h"
#include "fps.h"
#include "yarandom.h"
#include "x11shim/xshim.h"

struct xscreensaver_function_table {
  const char *progclass;
  const char *const *defaults;
  const XrmOptionDescRec *options;

  void (*setup_cb)(struct xscreensaver_function_table *, void *);
  void *setup_arg;

  void *(*init_cb)(Display *, Window);
  unsigned long (*draw_cb)(Display *, Window, void *);
  void (*reshape_cb)(Display *, Window, void *, unsigned int w,
                     unsigned int h);
  Bool (*event_cb)(Display *, Window, void *, XEvent *);
  void (*free_cb)(Display *, Window, void *);
  void (*fps_cb)(Display *, Window, fps_state *, void *);
  void (*fps_free)(fps_state *);

  Visual *(*pick_visual_hook)(Screen *);
  Bool (*validate_visual_hook)(Screen *, const char *, Visual *);
};

/* The runner reaches each table through HackEntry, so there is nothing to
 * link. */
#define XSCREENSAVER_LINK(tab)

#endif
