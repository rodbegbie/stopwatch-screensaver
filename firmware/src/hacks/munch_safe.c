/* Builds the unmodified munch.c and frees what it forgets: every restart
 * inside the hack asks for the "mismunch" resource and drops the string
 * (init frees its own copy).
 *
 * The wrapper remembers the latest string handed out. When upstream frees it,
 * the memory is forgotten; when upstream asks again with one still held, that
 * one is freed first, and the hack's free releases the last. */
#include <stdlib.h>

#include "screenhack.h"

static char *munch_held;

static char *munch_get_string_resource(Display *dpy, const char *name,
                                       const char *cls) {
  free(munch_held);
  munch_held = get_string_resource(dpy, name, cls);
  return munch_held;
}

static void munch_release(void *p) {
  if (p == munch_held) munch_held = NULL;
  free(p);
}

#define get_string_resource munch_get_string_resource
#define free(p) munch_release(p)

#undef XSCREENSAVER_MODULE
#define XSCREENSAVER_MODULE(CLASS, PREFIX)
#define munch_free munch_upstream_free

#include "munch/munch.c"

#undef munch_free
#undef free
#undef get_string_resource

static void munch_free(Display *dpy, Window window, void *closure) {
  munch_upstream_free(dpy, window, closure);
  free(munch_held);
  munch_held = NULL;
}

const HackEntry munch_hack = {"Munch", munch_defaults, munch_init, munch_draw,
                              munch_free, NULL, NULL};
