#include "hacks/registry.h"

/* A hack built on xlockmore.h defines a function table, not a HackEntry. */
#define XLOCKMORE_HACK_WITH(NAME, CLASS, OVERRIDES)                      \
  extern struct xscreensaver_function_table NAME##_xscreensaver_function_table; \
  static const HackEntry NAME##_hack = {                                 \
      CLASS, NULL, NULL, NULL, NULL,                                     \
      &NAME##_xscreensaver_function_table, OVERRIDES}
#define XLOCKMORE_HACK(NAME, CLASS) XLOCKMORE_HACK_WITH(NAME, CLASS, NULL)

/* Galaxy's `count: -5` picks 2-5 galaxies, and each star is pulled by every
 * galaxy. -3 caps it at 3. Counts of -2 or above skip the hack's cleanup on
 * restart and leak its star buffers. */
static const char *const kGalaxyOverrides[] = {"*count: -3", NULL};

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
extern const HackEntry maze_hack;
XLOCKMORE_HACK(hopalong, "Hopalong");
XLOCKMORE_HACK(vines, "Vines");
XLOCKMORE_HACK(sierpinski, "Sierpinski");
XLOCKMORE_HACK(fadeplot, "FadePlot");
XLOCKMORE_HACK(thornbird, "Thornbird");
XLOCKMORE_HACK(spiral, "Spiral");
XLOCKMORE_HACK(sphere, "Sphere");
XLOCKMORE_HACK(discrete, "Discrete");
XLOCKMORE_HACK_WITH(galaxy, "Galaxy", kGalaxyOverrides);
XLOCKMORE_HACK(drift, "Drift");
XLOCKMORE_HACK(lightning, "Lightning");

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
    &discrete_hack,
    &galaxy_hack,
    &drift_hack,
    &lightning_hack,
    &maze_hack};
const int g_hack_count = sizeof(g_hacks) / sizeof(g_hacks[0]);
