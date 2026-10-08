#include "runner/start_pick.h"

#include <strings.h>

int start_find_hack(const HackEntry *const *hacks, int count, const char *name) {
  for (int i = 0; i < count; i++)
    if (strcasecmp(hacks[i]->name, name) == 0) return i;
  return -1;
}

int start_pick_index(const HackEntry *const *hacks, int count,
                     const char *forced_name, uint32_t random) {
  if (forced_name) {
    int forced = start_find_hack(hacks, count, forced_name);
    if (forced >= 0) return forced;
  }
  return (int)(random % (uint32_t)count);
}
