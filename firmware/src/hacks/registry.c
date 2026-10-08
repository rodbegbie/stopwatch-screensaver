#include "hacks/registry.h"

extern const HackEntry pyro_hack;
extern const HackEntry hypercube_hack;
extern const HackEntry xspirograph_hack;
extern const HackEntry petri_hack;
extern const HackEntry helix_hack;

const HackEntry *const g_hacks[] = {&pyro_hack,        &hypercube_hack,
                                    &xspirograph_hack, &petri_hack,
                                    &helix_hack};
const int g_hack_count = sizeof(g_hacks) / sizeof(g_hacks[0]);
