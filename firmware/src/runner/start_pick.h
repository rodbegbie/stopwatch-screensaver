#ifndef RUNNER_START_PICK_H
#define RUNNER_START_PICK_H

#include <stdint.h>

#include "runner/hack_entry.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Index of the hack called name (case-insensitive, whole name), or -1. */
int start_find_hack(const HackEntry *const *hacks, int count, const char *name);

/* Which hack to start on: the one named by forced_name if it exists (a
 * build-time choice, for examining one hack), otherwise random % count. */
int start_pick_index(const HackEntry *const *hacks, int count,
                     const char *forced_name, uint32_t random);

#ifdef __cplusplus
}
#endif

#endif
