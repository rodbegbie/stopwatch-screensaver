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
  /* Device-specific resources that beat the hack's own defaults. */
  const char *const *overrides;
  /* Nonzero to start each push at a fixed point in the panel's refresh, so the
   * seam between the old and new picture lands where it does no harm (see
   * main.cpp). It costs up to one refresh of latency a frame, so a hack asks
   * only if thin lines moving a few pixels a frame show the seam. The wait
   * happens in present(); an overlay redraw made later, while the loop waits
   * out the hack's delay, is not counted against that delay, so a synced hack
   * with a long delay could overrun it. Unknown Pleasures never does. */
  int sync_to_refresh;
  /* Nonzero to keep the data cache's 32-byte lines (main.cpp sets 64 for the
   * rest): a hack that makes many short PSRAM writes runs slower with 64. */
  int dcache_line32;
} HackEntry;

#ifdef __cplusplus
}
#endif

#endif
