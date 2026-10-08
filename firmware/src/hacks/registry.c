#include "hacks/registry.h"

extern const HackEntry pyro_hack;

const HackEntry *const g_hacks[] = {&pyro_hack};
const int g_hack_count = sizeof(g_hacks) / sizeof(g_hacks[0]);
