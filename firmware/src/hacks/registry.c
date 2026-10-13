#include "hacks/registry.h"

/* A hack built on xlockmore.h defines a function table, not a HackEntry. */
#define XLOCKMORE_HACK_FULL(NAME, CLASS, OVERRIDES, SYNC)                \
  extern struct xscreensaver_function_table NAME##_xscreensaver_function_table; \
  static const HackEntry NAME##_hack = {                                 \
      CLASS, NULL, NULL, NULL, NULL,                                     \
      &NAME##_xscreensaver_function_table, OVERRIDES, SYNC}
#define XLOCKMORE_HACK_WITH(NAME, CLASS, OVERRIDES) \
  XLOCKMORE_HACK_FULL(NAME, CLASS, OVERRIDES, 0)
#define XLOCKMORE_HACK(NAME, CLASS) XLOCKMORE_HACK_WITH(NAME, CLASS, NULL)
/* Pushes start at a fixed point in the panel's refresh (see HackEntry). */
#define XLOCKMORE_HACK_SYNCED(NAME, CLASS, OVERRIDES) \
  XLOCKMORE_HACK_FULL(NAME, CLASS, OVERRIDES, 1)

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
extern const HackEntry blaster_hack;
extern const HackEntry critical_hack;
extern const HackEntry cloudlife_hack;
extern const HackEntry whirlwindwarp_hack;
extern const HackEntry flame_hack;
extern const HackEntry maze_hack;
XLOCKMORE_HACK(hopalong, "Hopalong");
/* Loop asks for 100 ms between generations, which held it to 8.7 fps with a
 * step of 3.5-7 ms; 50 ms evolves the colony about twice as fast. */
static const char *const kLoopOverrides[] = {"*delay: 50000", NULL};
XLOCKMORE_HACK_WITH(loop, "Loop", kLoopOverrides);
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
XLOCKMORE_HACK(pacman, "Pacman");
XLOCKMORE_HACK(braid, "Braid");
XLOCKMORE_HACK(mountain, "Mountain");
extern const HackEntry substrate_hack;
extern const HackEntry epicycle_hack;
extern const HackEntry kaleidescope_hack;
extern const HackEntry deluxe_hack;
/* The xlockmore framework reads ncolors and cycles; Gears defines neither, and
 * nothing in Gears uses them. Its own 30 ms pause held a light layout to 11 to
 * 13 fps once the dirty rectangle made a frame 44 to 63 ms; 10 ms removes it. */
static const char *const kGearsOverrides[] = {"*ncolors: 64", "*cycles: 0",
                                              "*delay: 10000", NULL};
XLOCKMORE_HACK_WITH(gears, "Gears", kGearsOverrides);
/* The hack asks for a 40 ms pause after each frame, which held it to 10 fps
 * once a frame took 57 ms; 10 ms gives 14.6 fps. */
static const char *const kMorph3dOverrides[] = {"*ncolors: 64", "*cycles: 0",
                                                "*delay: 10000", NULL};
XLOCKMORE_HACK_WITH(morph3d, "Morph3D", kMorph3dOverrides);
/* The grid is ticks cubed points a frame: the default 30 (27,000) ran at 5.6
 * to 6 fps. 20 gives 8,000. */
static const char *const kCubicGridOverrides[] = {"*ncolors: 64", "*cycles: 0",
                                                  "*ticks: 20", NULL};
XLOCKMORE_HACK_WITH(cubicgrid, "CubicGrid", kCubicGridOverrides);
static const char *const kStonerViewOverrides[] = {"*ncolors: 64", "*cycles: 0",
                                                   "*count: 1", "*delay: 10000",
                                                   NULL};
XLOCKMORE_HACK_WITH(stonerview, "StonerView", kStonerViewOverrides);
XLOCKMORE_HACK(forest, "Forest");
XLOCKMORE_HACK(laser, "Laser");
XLOCKMORE_HACK(lissie, "Lissie");
XLOCKMORE_HACK(rotor, "Rotor");
XLOCKMORE_HACK(scooter, "Scooter");
XLOCKMORE_HACK(grav, "Grav");
/* 1024 particles took a 53 ms step in single precision; 512 keep the flow
 * readable and the frame rate nearer 15. */
static const char *const kEuler2dOverrides[] = {"*count: 512", NULL};
XLOCKMORE_HACK_WITH(euler2d, "Euler2D", kEuler2dOverrides);
extern const HackEntry anemone_hack;
extern const HackEntry anemotaxis_hack;
extern const HackEntry compass_hack;
extern const HackEntry fuzzyflakes_hack;
extern const HackEntry ifs_hack;
extern const HackEntry interaggregate_hack;
extern const HackEntry lmorph_hack;
extern const HackEntry wormhole_hack;
extern const HackEntry truchet_hack;
extern const HackEntry hexadrop_hack;
extern const HackEntry halo_hack;
extern const HackEntry xmatrix_hack;
extern const HackEntry qix_hack;
extern const HackEntry fireworkx_hack;
extern const HackEntry starfish_hack;
extern const HackEntry goop_hack;
/* The xlockmore framework reads ncolors and cycles, which the hack does not
 * define. The override cuts its own 30 ms pause to 10 ms, as for Gears. Its
 * thin lines move a few pixels a frame, so a seam between the old and the
 * new picture made lines vanish for a frame: it asks for synced pushes. */
static const char *const kUnknownPleasuresOverrides[] = {
    "*ncolors: 64", "*cycles: 0", "*delay: 10000", NULL};
XLOCKMORE_HACK_SYNCED(unknownpleasures, "UnknownPleasures",
                      kUnknownPleasuresOverrides);

extern const HackEntry cwaves_hack;

extern const HackEntry vermiculate_hack;

extern const HackEntry cynosure_hack;

extern const HackEntry moire_hack;

extern const HackEntry moire2_hack;

/* Hextrail draws every hexagon it has built, so its step grows from 19 ms to
 * a ceiling and it restarts. The grid is count * 2 hexagons square, and a
 * smaller count means fewer, larger ones over the same area. Mean fps over a
 * 150 s capture: 5.7 at upstream's 20 (step up to 293 ms), 9.1 at 10 and 12.7
 * at 6; in single precision with the depth clear skipped, 15.3 at 6 (10.8 at
 * the worst). -DHEXTRAIL_COUNT=<n> changes it. */
#ifndef HEXTRAIL_COUNT
#define HEXTRAIL_COUNT 6
#endif
#define HEXTRAIL_STR2(x) #x
#define HEXTRAIL_STR(x) HEXTRAIL_STR2(x)
static const char *const kHexTrailOverrides[] = {
    "*ncolors: 64", "*cycles: 0", "*delay: 10000",
    "*count: " HEXTRAIL_STR(HEXTRAIL_COUNT), NULL};
XLOCKMORE_HACK_WITH(hextrail, "HexTrail", kHexTrailOverrides);
#ifdef HYPNOWHEEL_PROBE
/* Dropped from the rotation (issue #73); built only to measure it on the
 * board, as the last entry, with the overrides it had when registered. */
static const char *const kHypnowheelOverrides[] = {"*ncolors: 64", "*cycles: 0",
                                                   "*delay: 10000", NULL};
XLOCKMORE_HACK_WITH(hypnowheel, "Hypnowheel", kHypnowheelOverrides);
/* Outside the array, so tools/score_hacks.py still counts it as not ported. */
#define HYPNOWHEEL_PROBE_ENTRY &hypnowheel_hack,
#else
#define HYPNOWHEEL_PROBE_ENTRY
#endif

/* Built only when the After Dark disk image is present; kept last so every
 * other hack keeps its index. */
extern const HackEntry celtic_hack;
#ifdef HAVE_AFTER_DARK_TOASTERS
extern const HackEntry ad_flyingtoasters_hack;
#endif

extern const HackEntry eruption_hack;

extern const HackEntry kumppa_hack;

extern const HackEntry shadebobs_hack;

extern const HackEntry piecewise_hack;

extern const HackEntry boxfit_hack;

extern const HackEntry interference_hack;

extern const HackEntry binaryring_hack;

XLOCKMORE_HACK(graphstat, "GraphStat");

XLOCKMORE_HACK(etruscanvenus, "EtruscanVenus");

extern const HackEntry abstractile_hack;

extern const HackEntry attraction_hack;

extern const HackEntry greynetic_hack;

extern const HackEntry wander_hack;


extern const HackEntry popsquares_hack;


XLOCKMORE_HACK(lisa, "Lisa");

XLOCKMORE_HACK(penrose, "Penrose");

XLOCKMORE_HACK(worm, "Worm");

XLOCKMORE_HACK(triangle, "Triangle");

extern const HackEntry xrayswarm_hack;

extern const HackEntry rocks_hack;

extern const HackEntry decayscreen_hack;

extern const HackEntry xjack_hack;

/* Pipes (issue #44, built by pipes_gl.c) builds every display list of a screen
 * when the screen starts: upstream's 5 systems of up to 500 pieces is about
 * 22 MB on the board. 3 of 150 left 1.9 MB of PSRAM free (2 of 200 left 2.4,
 * 1 of 100 4.6), at about 60 fps. The fixed camera lets the wrapper draw only
 * the newest piece each frame. Its DEFAULTS has no ncolors, which the
 * xlockmore framework reads. -DPIPES_CYCLES=<systems> and -DPIPES_SIZE=<pieces>
 * change the screen. */
#ifndef PIPES_CYCLES
#define PIPES_CYCLES 3
#endif
#ifndef PIPES_SIZE
#define PIPES_SIZE 150
#endif
static const char *const kPipesOverrides[] = {
    "*ncolors: 64", "*cycles: " HEXTRAIL_STR(PIPES_CYCLES),
    "*size: " HEXTRAIL_STR(PIPES_SIZE), "*rotatepipes: False", NULL};
XLOCKMORE_HACK_WITH(pipes, "Pipes", kPipesOverrides);

extern const HackEntry marbling_hack;

extern const HackEntry deco_hack;

extern const HackEntry imsmap_hack;

extern const HackEntry metaballs_hack;

extern const HackEntry munch_hack;

extern const HackEntry ccurve_hack;

extern const HackEntry binaryhorizon_hack;

extern const HackEntry rotzoomer_hack;

extern const HackEntry twang_hack;

extern const HackEntry nerverot_hack;

XLOCKMORE_HACK(flow, "Flow");

/* Upstream draws a red and a blue starfield for 3D glasses (`use3d: True`) and
 * ORs them where they overlap, which the shim does not do. Without glasses,
 * the single-colour mode cycles through the palette. The framework reads
 * `cycles`, which Bouboule does not define. */
static const char *const kBoubouleOverrides[] = {"*use3d: False", "*cycles: 0",
                                                 NULL};
XLOCKMORE_HACK_WITH(bouboule, "Bouboule", kBoubouleOverrides);

XLOCKMORE_HACK(ant, "Ant");

extern const HackEntry hyperball_hack;

XLOCKMORE_HACK(demon, "Demon");

extern const HackEntry droste_hack;

XLOCKMORE_HACK(polyominoes, "Polyominoes");

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
    &maze_hack,
    &blaster_hack,
    &substrate_hack,
    &pacman_hack,
    &braid_hack,
    &mountain_hack,
    &epicycle_hack,
    &kaleidescope_hack,
    &gears_hack,
    &morph3d_hack,
    &cubicgrid_hack,
    &deluxe_hack,
    &forest_hack,
    &laser_hack,
    &lissie_hack,
    &rotor_hack,
    &scooter_hack,
    &grav_hack,
    &euler2d_hack,
    &anemone_hack,
    &anemotaxis_hack,
    &compass_hack,
    &fuzzyflakes_hack,
    &ifs_hack,
    &interaggregate_hack,
    &lmorph_hack,
    &wormhole_hack,
    &truchet_hack,
    &unknownpleasures_hack,
    &hexadrop_hack,
    &stonerview_hack,
    &halo_hack,
    &xmatrix_hack,
    &qix_hack,
    &fireworkx_hack,
    &starfish_hack,
    &goop_hack,
    &loop_hack,
    &cwaves_hack,
    &vermiculate_hack,
    &cynosure_hack,
    &moire_hack,
    &moire2_hack,
    &hextrail_hack,
    &celtic_hack,
    &eruption_hack,
    &kumppa_hack,
    &shadebobs_hack,
    &piecewise_hack,
    &boxfit_hack,
    &interference_hack,
    &binaryring_hack,
    &graphstat_hack,
    &etruscanvenus_hack,
    &abstractile_hack,
    &attraction_hack,
    &greynetic_hack,
    &wander_hack,
    &popsquares_hack,
    &lisa_hack,
    &penrose_hack,
    &worm_hack,
    &triangle_hack,
    &xrayswarm_hack,
    &rocks_hack,
    &decayscreen_hack,
    &xjack_hack,
    &pipes_hack,
    &marbling_hack,
    &deco_hack,
    &imsmap_hack,
    &metaballs_hack,
    &munch_hack,
    &ccurve_hack,
    &binaryhorizon_hack,
    &rotzoomer_hack,
    &twang_hack,
    &nerverot_hack,
    &flow_hack,
    &bouboule_hack,
    &ant_hack,
    &hyperball_hack,
    &demon_hack,
    &droste_hack,
    &polyominoes_hack,
#ifdef HAVE_AFTER_DARK_TOASTERS
    &ad_flyingtoasters_hack,
#endif
    HYPNOWHEEL_PROBE_ENTRY
};
const int g_hack_count = sizeof(g_hacks) / sizeof(g_hacks[0]);
