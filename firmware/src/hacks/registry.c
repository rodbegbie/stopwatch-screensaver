#include "hacks/registry.h"

extern const HackEntry pyro_hack;
extern const HackEntry hypercube_hack;
extern const HackEntry xspirograph_hack;
extern const HackEntry petri_hack;
extern const HackEntry helix_hack;
extern const HackEntry rorschach_hack;
extern const HackEntry pedal_hack;
extern const HackEntry coral_hack;
extern const HackEntry squiral_hack;
extern const HackEntry critical_hack;
extern const HackEntry cloudlife_hack;
extern const HackEntry whirlwindwarp_hack;

const HackEntry *const g_hacks[] = {
    &pyro_hack,     &hypercube_hack,   &xspirograph_hack,   &petri_hack,
    &helix_hack,    &rorschach_hack,   &pedal_hack,         &coral_hack,
    &squiral_hack,  &critical_hack,    &cloudlife_hack,     &whirlwindwarp_hack};
const int g_hack_count = sizeof(g_hacks) / sizeof(g_hacks[0]);
