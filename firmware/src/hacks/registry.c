#include "hacks/registry.h"

/* A hack built on xlockmore.h defines a function table, not a HackEntry. */
#define XLOCKMORE_HACK(NAME, CLASS)                                      \
  extern struct xscreensaver_function_table NAME##_xscreensaver_function_table; \
  static const HackEntry NAME##_hack = {                                 \
      CLASS, NULL, NULL, NULL, NULL,                                     \
      &NAME##_xscreensaver_function_table}

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
extern const HackEntry flame_hack;
XLOCKMORE_HACK(hopalong, "Hopalong");
XLOCKMORE_HACK(vines, "Vines");
XLOCKMORE_HACK(sierpinski, "Sierpinski");
XLOCKMORE_HACK(fadeplot, "FadePlot");
XLOCKMORE_HACK(thornbird, "Thornbird");
XLOCKMORE_HACK(spiral, "Spiral");
XLOCKMORE_HACK(sphere, "Sphere");
XLOCKMORE_HACK(discrete, "Discrete");

const HackEntry *const g_hacks[] = {
    &pyro_hack,     &hypercube_hack, &xspirograph_hack,   &petri_hack,
    &helix_hack,    &rorschach_hack, &pedal_hack,         &coral_hack,
    &squiral_hack,  &critical_hack,  &cloudlife_hack,     &whirlwindwarp_hack,
    &flame_hack,    &hopalong_hack,
    &vines_hack,
    &sierpinski_hack,
    &fadeplot_hack,
    &thornbird_hack,
    &spiral_hack,
    &sphere_hack,
    &discrete_hack};
const int g_hack_count = sizeof(g_hacks) / sizeof(g_hacks[0]);
