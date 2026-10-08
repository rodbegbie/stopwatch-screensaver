#ifndef HACKS_REGISTRY_H
#define HACKS_REGISTRY_H

#include "runner/hack_entry.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const HackEntry *const g_hacks[];
extern const int g_hack_count;

#ifdef __cplusplus
}
#endif

#endif
