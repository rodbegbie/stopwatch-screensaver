#ifndef RUNNER_HACK_ENTRY_H
#define RUNNER_HACK_ENTRY_H

#include "x11shim/xshim.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Mirrors the callbacks in xscreensaver's xscreensaver_function_table. */
typedef struct HackEntry {
  const char *name;
  const char *const *defaults;
  void *(*init)(Display *, Window);
  unsigned long (*draw)(Display *, Window, void *closure); /* delay in us */
  void (*free)(Display *, Window, void *closure);
} HackEntry;

#ifdef __cplusplus
}
#endif

#endif
