#ifndef RUNNER_HACK_ENTRY_H
#define RUNNER_HACK_ENTRY_H

#include "x11shim/xshim.h"

#ifdef __cplusplus
extern "C" {
#endif

struct xscreensaver_function_table;

/* Mirrors the callbacks in xscreensaver's xscreensaver_function_table. For a
 * hack built on xlockmore.h the callbacks above are unused: `xsft` points at
 * the hack's own table, which the runner completes with its setup_cb. */
typedef struct HackEntry {
  const char *name;
  const char *const *defaults;
  void *(*init)(Display *, Window);
  unsigned long (*draw)(Display *, Window, void *closure); /* delay in us */
  void (*free)(Display *, Window, void *closure);
  struct xscreensaver_function_table *xsft;
} HackEntry;

#ifdef __cplusplus
}
#endif

#endif
