#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sanitizer/allocator_interface.h>
#include <unity.h>

#include "core/canvas.h"
#include "doubletime.h"
#include "screenhack.h"
#include "xlockmore.h"
#include "hacks/fast_trig.h"
#include "hacks/pacman/pacman.h"
#include "glshim/glshim.h"
#include "hacks/registry.h"
#include "runner/hack_runner.h"

static Canvas cv;

/* A hack that animates from wall time (UnknownPleasures) would draw different
 * frames each run. This clock moves a thirtieth of a second a reading. */
static double fake_now;
static double fake_clock(void) { return fake_now += 1.0 / 30.0; }

void setUp(void) {
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&cv, 466, 466, malloc));
  fake_now = 1000.0;
  xshim_set_clock(fake_clock);
}
void tearDown(void) {
  xshim_set_clock(NULL);
  canvas_free(&cv);
}

static int non_black(void) {
  int n = 0;
  for (long i = 0; i < (long)cv.w * cv.h; i++) n += cv.px[i] != 0;
  return n;
}

/* Starting a push at a fixed point in the panel's refresh costs up to a refresh
 * of latency a frame, so only a hack whose thin lines show the seam asks for it. */
void test_only_unknownpleasures_asks_for_refresh_synced_pushes(void) {
  int asking = 0;
  for (int i = 0; i < g_hack_count; i++) {
    if (!g_hacks[i]->sync_to_refresh) continue;
    asking++;
    TEST_ASSERT_EQUAL_STRING("UnknownPleasures", g_hacks[i]->name);
  }
  TEST_ASSERT_EQUAL_INT(1, asking);
}

void test_registry_lists_hacks_in_order(void) {
  static const char *const expected[] = {"Pyro", "HyperCube", "XSpirograph",
                                         "Petri", "Helix",
                                         "Rorschach", "Pedal", "Coral",
                                         "Squiral", "Critical", "CloudLife",
                                         "WhirlWindWarp", "Flame", "Hopalong",
                                         "Vines",
                                         "Sierpinski",
                                         "FadePlot",
                                         "Thornbird",
                                         "Spiral",
                                         "Sphere",
                                         "Discrete",
                                         "Galaxy",
                                         "Drift",
                                         "Lightning",
                                         "Maze",
                                         "Blaster",
                                         "Substrate",
                                         "Pacman",
                                         "Braid",
                                         "Mountain",
                                         "Epicycle",
                                         "Kaleidescope",
                                         "Gears",
                                         "Morph3D",
                                         "CubicGrid",
                                         "Deluxe",
                                         "Forest",
                                         "Laser",
                                         "Lissie",
                                         "Rotor",
                                         "Scooter",
                                         "Grav",
                                         "Euler2D",
                                         "Anemone",
                                         "Anemotaxis",
                                         "Compass",
                                         "FuzzyFlakes",
                                         "IFS",
                                         "Interaggregate",
                                         "LMorph",
                                         "Wormhole",
                                         "Truchet",
                                         "UnknownPleasures",
                                         "Hexadrop",
                                         "StonerView",
                                         "Halo",
                                         "XMatrix",
                                         "Qix",
                                         "Fireworkx",
                                         "Starfish",
                                         "Goop",
                                         "Loop",
                                         "CWaves",
                                         "Vermiculate",
                                         "Cynosure",
                                         "Moire",
                                         "Moire2",
                                         "HexTrail",
                                         "Celtic",
#ifdef HAVE_AFTER_DARK_TOASTERS
                                         "ad-flyingtoasters",
#endif
  };
  const int n = sizeof(expected) / sizeof(expected[0]);
  TEST_ASSERT_EQUAL_INT(n, g_hack_count);
  for (int i = 0; i < n; i++) TEST_ASSERT_EQUAL_STRING(expected[i], g_hacks[i]->name);
}

/* Two quick button presses (or a press with the 90 s rotation) switch away
 * from a hack before it has drawn once. */
void test_every_hack_can_be_stopped_before_its_first_frame(void) {
  for (int i = 0; i < g_hack_count; i++) {
    HackRunner *r = runner_create(&cv);
    TEST_ASSERT_EQUAL_INT(0, runner_start(r, i));
    runner_destroy(r);
  }
}

static int index_of(const char *name) {
  for (int i = 0; i < g_hack_count; i++)
    if (strcmp(g_hacks[i]->name, name) == 0) return i;
  return -1;
}

static long count_pixels(uint16_t colour) {
  long n = 0;
  for (long i = 0; i < (long)cv.w * cv.h; i++) n += cv.px[i] == colour;
  return n;
}

/* Real screenhack.c paints the window in the hack's background colour before
 * its init runs, and Substrate (`.background: white`) relies on it. */
void test_a_hack_starts_on_the_background_colour_it_asks_for(void) {
  HackRunner *r = runner_create(&cv);
  TEST_ASSERT_EQUAL_INT(0, runner_start(r, index_of("Substrate")));
  TEST_ASSERT_TRUE(count_pixels(0xFFFF) > (long)cv.w * cv.h * 99 / 100);
  runner_destroy(r);
}

/* The display outlives a hack, so a window background it set must not carry
 * over: Hexadrop's colour once turned Pyro's XClearWindow into a solid fill. */
void test_a_window_background_does_not_outlive_the_hack_that_set_it(void) {
  HackRunner *r = runner_create(&cv);
  TEST_ASSERT_EQUAL_INT(0, runner_start(r, index_of("Hexadrop")));
  for (int f = 0; f < 5; f++) runner_step(r);
  TEST_ASSERT_EQUAL_INT(0, runner_start(r, index_of("Pyro")));
  for (int f = 0; f < 5; f++) runner_step(r);
  TEST_ASSERT_TRUE(count_pixels(0) > (long)cv.w * cv.h * 9 / 10);
  runner_destroy(r);
}

/* True once at least 20 pixels differ from the canvas as the hack started. */
static int draws_within(HackRunner *r, int frames) {
  const size_t n = (size_t)cv.w * cv.h;
  uint16_t *start = (uint16_t *)malloc(n * sizeof(*start));
  TEST_ASSERT_NOT_NULL(start);
  memcpy(start, cv.px, n * sizeof(*start));
  int seen = 0;
  for (int f = 0; f < frames && !seen; f++) {
    runner_step(r);
    long differing = 0;
    for (size_t i = 0; i < n; i++) differing += cv.px[i] != start[i];
    seen = differing >= 20;
  }
  free(start);
  return seen;
}

static void *idle_init(Display *dpy, Window w) {
  (void)dpy;
  (void)w;
  return NULL;
}

static unsigned long idle_draw(Display *dpy, Window w, void *closure) {
  (void)dpy;
  (void)w;
  (void)closure;
  return 10000;
}

static void idle_free(Display *dpy, Window w, void *closure) {
  (void)dpy;
  (void)w;
  (void)closure;
}

/* A hack that makes pixmaps and never frees them: Pacman's scale_pixmap
 * overwrites the unscaled handle, and its free skips several. A real X server
 * frees a client's resources when it disconnects; the runner reuses one display
 * across hacks, so it must release what a stopped hack left behind. */
static void *leaky_init(Display *dpy, Window w) {
  XCreatePixmap(dpy, w, 100, 100, 16);
  XCreatePixmap(dpy, w, 64, 64, 1);
  return NULL;
}

void test_the_runner_releases_pixmaps_a_stopped_hack_left_behind(void) {
  static const HackEntry leaky = {"Leaky", NULL, leaky_init, idle_draw, idle_free, NULL, NULL};
  const HackEntry *const hacks[] = {&leaky};
  HackRunner *r = runner_create_with(&cv, hacks, 1);
  for (int i = 0; i < 3; i++) {
    runner_start(r, 0);
    runner_step(r);
  }
  const size_t before = __sanitizer_get_current_allocated_bytes();
  for (int i = 0; i < 40; i++) {
    runner_start(r, 0);
    runner_step(r);
  }
  const size_t after = __sanitizer_get_current_allocated_bytes();
  runner_destroy(r);
  TEST_ASSERT_TRUE_MESSAGE(after < before + 4 * 1024, "pixmaps piled up across restarts");
}

/* Substrate starts on a white canvas, so "20 non-black pixels" was true before
 * it drew anything. A hack that never draws must not pass, whatever colour its
 * background is. */
void test_a_hack_that_never_draws_on_white_is_not_drawing(void) {
  static const char *const white[] = {".background: white", NULL};
  static const HackEntry idle = {"Idle", white, idle_init, idle_draw, idle_free,
                                 NULL};
  const HackEntry *const hacks[] = {&idle};
  HackRunner *r = runner_create_with(&cv, hacks, 1);
  TEST_ASSERT_EQUAL_INT(0, runner_start(r, 0));
  TEST_ASSERT_FALSE(draws_within(r, 50));
  runner_destroy(r);
}

void test_every_hack_draws_something_within_2000_frames(void) {
  for (int i = 0; i < g_hack_count; i++) {
    HackRunner *r = runner_create(&cv);
    TEST_ASSERT_EQUAL_INT(0, runner_start(r, i));
    TEST_ASSERT_TRUE_MESSAGE(draws_within(r, 2000), g_hacks[i]->name);
    runner_destroy(r);
  }
}

void test_every_hack_runs_3000_frames_cleanly_with_sane_delays(void) {
  for (int i = 0; i < g_hack_count; i++) {
    HackRunner *r = runner_create(&cv);
    runner_start(r, i);
    for (int f = 0; f < 3000; f++) {
      unsigned long d = runner_step(r);
      TEST_ASSERT_TRUE_MESSAGE(d >= RUNNER_MIN_DELAY_US && d <= RUNNER_MAX_DELAY_US,
                               g_hacks[i]->name);
    }
    runner_destroy(r);
  }
}

void test_cycling_through_all_hacks_100_times_is_asan_clean(void) {
  HackRunner *r = runner_create(&cv);
  runner_start(r, 0);
  for (int i = 0; i < 100; i++) {
    for (int s = 0; s < 5; s++) runner_step(r);
    runner_next(r);
  }
  TEST_ASSERT_EQUAL_INT(100 % g_hack_count, runner_index(r));
  runner_destroy(r);
}

/* With `cycles: 1` Galaxy restarts every 5 frames, and with some counts its
 * restart leaks the star rectangle buffers. Each restart picks 1500-3000 stars
 * a galaxy at random (MAX_STARS is a #define, so it cannot be pinned), which
 * moves live memory by up to about 400 KB. About 160 restarts spread that
 * slack over each one, so the test catches leaks of roughly 2.5 KB a restart
 * or more; the count-2 leak was 48 KB or more. */
void test_galaxy_restarts_do_not_leak_with_its_registered_overrides(void) {
  const HackEntry *galaxy = NULL;
  for (int i = 0; i < g_hack_count; i++)
    if (strcmp(g_hacks[i]->name, "Galaxy") == 0) galaxy = g_hacks[i];
  TEST_ASSERT_NOT_NULL(galaxy);

  const char *merged[8] = {"*cycles: 1"};
  int n = 1;
  for (const char *const *o = galaxy->overrides; o && *o && n < 7; o++)
    merged[n++] = *o;
  HackEntry entry = *galaxy;
  entry.overrides = merged;
  const HackEntry *const hacks[] = {&entry};

  HackRunner *r = runner_create_with(&cv, hacks, 1);
  runner_start(r, 0);
  for (int f = 0; f < 100; f++) runner_step(r);
  const size_t before = __sanitizer_get_current_allocated_bytes();
  for (int f = 0; f < 800; f++) runner_step(r);
  const size_t after = __sanitizer_get_current_allocated_bytes();
  runner_destroy(r);
  TEST_ASSERT_TRUE_MESSAGE(after < before + 400 * 1024, "allocated bytes grew");
}

/* Maze starts a new maze every cycle, allocating a sets table, an edge list
 * and corner tables that the next cycle must free. Which of them is live when
 * we sample depends on the generator it picked, and can hold up to about
 * 52 KB, so the check allows 60 KB of slack: a leak of a few KB a cycle shows
 * up after the dozens of cycles run here. */
void test_maze_cycles_do_not_leak(void) {
  int maze = -1;
  for (int i = 0; i < g_hack_count; i++)
    if (strcmp(g_hacks[i]->name, "Maze") == 0) maze = i;
  TEST_ASSERT_TRUE(maze >= 0);

  HackRunner *r = runner_create(&cv);
  runner_start(r, maze);
  for (int f = 0; f < 5000; f++) runner_step(r);
  const size_t before = __sanitizer_get_current_allocated_bytes();
  for (int f = 0; f < 60000; f++) runner_step(r);
  const size_t after = __sanitizer_get_current_allocated_bytes();
  runner_destroy(r);
  TEST_ASSERT_TRUE_MESSAGE(after < before + 60 * 1024, "allocated bytes grew");
}

/* With `maxCycles: 3` Substrate rebuilds its crack grid and pixel map every
 * 3 frames. Each is about 1.7 MB (3.5 MB on the host, where `unsigned long` is
 * 8 bytes), so a leak of either shows within a few restarts. */
void test_substrate_restarts_do_not_leak(void) {
  const HackEntry *substrate = NULL;
  for (int i = 0; i < g_hack_count; i++)
    if (strcmp(g_hacks[i]->name, "Substrate") == 0) substrate = g_hacks[i];
  TEST_ASSERT_NOT_NULL(substrate);

  const char *const overrides[] = {"*maxCycles: 3", NULL};
  HackEntry entry = *substrate;
  entry.overrides = overrides;
  const HackEntry *const hacks[] = {&entry};

  HackRunner *r = runner_create_with(&cv, hacks, 1);
  runner_start(r, 0);
  for (int f = 0; f < 10; f++) runner_step(r);
  const size_t before = __sanitizer_get_current_allocated_bytes();
  for (int f = 0; f < 60; f++) runner_step(r);
  const size_t after = __sanitizer_get_current_allocated_bytes();
  runner_destroy(r);
  TEST_ASSERT_TRUE_MESSAGE(after < before + 256 * 1024, "allocated bytes grew");
}

/* FNV-1a over the frame as ordinary RGB565 bytes, low byte first. */
static uint64_t frame_hash(void) {
  uint64_t h = 0xcbf29ce484222325ull;
  for (long i = 0; i < (long)cv.w * cv.h; i++) {
    uint16_t v = px_swap(cv.px[i]);
    h = (h ^ (v & 0xFF)) * 0x100000001b3ull;
    h = (h ^ (v >> 8)) * 0x100000001b3ull;
  }
  return h;
}

/* Substrate blends each crack's colour into the picture by pulling the red,
 * green and blue bits out of pixel values itself, assuming ordinary RGB565.
 * Canvas pixels are byte-swapped, so its wrapper swaps wherever a pixel crosses
 * between Substrate and the shim. Seeded, 200 frames must give exactly the
 * picture drawn before pixels were byte-swapped (a swap missed anywhere gives
 * a different one). The hash comes from the dump tool on that build. If a
 * libm update changes it, regenerate it the same way after checking the
 * frames by eye. */
void test_substrate_draws_what_it_drew_before_pixels_were_swapped(void) {
  const HackEntry *substrate = NULL;
  for (int i = 0; i < g_hack_count; i++)
    if (strcmp(g_hacks[i]->name, "Substrate") == 0) substrate = g_hacks[i];
  TEST_ASSERT_NOT_NULL(substrate);
  const HackEntry *const hacks[] = {substrate};

  srandom(1);
  HackRunner *r = runner_create_with(&cv, hacks, 1);
  runner_start(r, 0);
  for (int f = 0; f < 200; f++) runner_step(r);
  runner_destroy(r);
  TEST_ASSERT_EQUAL_UINT64(0xb0001ae2c830b47cull, frame_hash());
}

/* A hack can be blank at any one frame (Lightning, Pedal), so fold in a frame
 * hash every `frames / 4` frames. */
static uint64_t hash_after(int index, int frames) {
  srandom(1);
  HackRunner *r = runner_create(&cv);
  runner_start(r, index);
  uint64_t chain = 0xcbf29ce484222325ull;
  for (int f = 1; f <= frames; f++) {
    runner_step(r);
    if (f % (frames / 4) == 0) chain = (chain ^ frame_hash()) * 0x100000001b3ull;
  }
  runner_destroy(r);
  return chain;
}

/* Frame hashes taken on main before the shim learned line width, arcs and
 * writable pixmaps. A hack that sets no width above 1 must still draw exactly
 * this. If a libm update changes one, regenerate it after checking the frames
 * by eye. */
static const uint64_t kBaseline[] = {
    0xc09cdcfa9b3be138ull, /* Pyro */
    0x63c3673ca39d844full, /* HyperCube */
    0xceb9caf334de881eull, /* XSpirograph: re-pinned for the eraser dwell and wipe (#86) */
    0x9b337f3f1305e6d1ull, /* Petri */
    0x0577bf85b67da573ull, /* Helix: re-pinned for the eraser (#86) */
    0xaeed5737b8ef81e7ull, /* Rorschach: re-pinned for the eraser (#86) */
    0xc1db63631e6d26ecull, /* Pedal: re-pinned for the eraser (#86) */
    0, /* Coral reads the clock: not deterministic, so not pinned */
    0xdabd390a15578da4ull, /* Squiral */
    0x19c5b220193ad705ull, /* Critical */
    0x80bcaaef5d53f475ull, /* CloudLife */
    0x9fc760a330da4c12ull, /* WhirlWindWarp */
    0x9ade65fba7afeb1aull, /* Flame */
    0x29a9c8bcad390b11ull, /* Hopalong */
    0x7cecada4d69a47beull, /* Vines: re-pinned for the eraser (#86) */
    0x81324f88137bfcf1ull, /* Sierpinski: re-pinned for the eraser (#86) */
    0x39d159495623cf2bull, /* FadePlot */
    0x574c61de3bb517afull, /* Thornbird */
    0xca8ae65ff2d713aeull, /* Spiral */
    0x9748fd2017af52e5ull, /* Sphere */
    0x03e9544d12dae5ddull, /* Discrete */
    0x4815a4e22a59c710ull, /* Galaxy */
    0x7b230f32b354922bull, /* Drift */
    0xe97dd56bf15d2b99ull, /* Lightning */
    0, /* Maze: see test_maze_frame_matches_main */
    0x1472437ef1d9a798ull, /* Blaster */
    0xb90b3ccdac009a41ull, /* Substrate */
    0x30f53883f531c363ull, /* Pacman: taken on its own branch, after looking at the frames */
    0x0f7959307446f1c1ull, /* Braid: taken after looking at the frames; re-pinned for the eraser (#86) */
    0x8ce46510f0e0360cull, /* Mountain: taken after looking at the frames */
    0xa19684045106b327ull, /* Epicycle: taken after looking at the frames */
    0xdeb3899d2483b5b7ull, /* Kaleidescope: taken after looking at the frames */
    0x41f708d6203d8140ull, /* Gears: taken after looking at the frames */
    0x5a1dd5b30e81a013ull, /* Morph3D: taken after looking at the frames */
    0xc2b1b6b7d671b4c7ull, /* CubicGrid at ticks 20: taken after looking at the frames */
    0xec0a5a874ba776a3ull, /* Deluxe, translucent, each shape blended once: taken after looking at the frames; re-taken for the stroker's shared-edge slack (#56) */
    0xc127f54362047502ull, /* Forest: taken after looking at the frames; re-taken for the stroker's shared-edge slack (#56) */
    0x3796f88202299960ull, /* Laser: taken after looking at the frames */
    0xe48eab27f8a1f05bull, /* Lissie: taken after looking at the frames */
    0x2b087cd03addae3eull, /* Rotor: taken after looking at the frames; re-taken for the stroker's shared-edge slack (#56) */
    0xc76242aaacbe8177ull, /* Scooter: taken after looking at the frames */
    0x9d5e492757aab42bull, /* Grav: taken after looking at the frames */
    0x41f6a76aad432b20ull, /* Euler2D: taken after looking at the frames */
    0x59bf16ddeccecc15ull, /* Anemone: taken after looking at the frames */
    0x1ad9e9fe2718ba88ull, /* Anemotaxis: taken after looking at the frames */
    0x3441a22a1f12785aull, /* Compass: taken after looking at the frames; re-taken for the stroker's shared-edge slack (#56) */
    0xe94fb924120c76a5ull, /* FuzzyFlakes: taken after looking at the frames */
    0x3cc7ad2eeeea2850ull, /* IFS: taken after looking at the frames */
    0x78c62badb21d8dddull, /* Interaggregate: after looking at the frames, pixels swapped */
    0x7118310d47ee4197ull, /* LMorph: taken after looking at the frames */
    0xe73572fd64eef48full, /* Wormhole: taken after looking at the frames */
    0x7dd7d7130551c261ull, /* Truchet: taken after looking at the frames */
    0x03670369cb12c2deull, /* UnknownPleasures: after looking at the frames, with the fake clock, smooth lines and the offset margin */
    0x43f9cf84967fee7cull, /* Hexadrop: taken after looking at the frames */
    0xb162a291668a076dull, /* StonerView: taken after looking at the frames (saturating additive blend) */
    0x692e426561735c0dull, /* Halo: taken after looking at the frames, with GCBackground honoured */
    0x9b1b0de461b6438bull, /* XMatrix: taken after looking at the frames, trace off */
    0x51a9a94db9bf9128ull, /* Qix: taken after looking at the frames */
    0xa25b9411b50849b8ull, /* Fireworkx: taken after looking at the frames (shells and lengths scaled to the render size) */
    0x77bfaaf3faa1cc64ull, /* Starfish: taken after looking at the frames (spline.h before the float rename) */
    0x5694978ccee06fd6ull, /* Goop: taken after looking at the frames (spline.h before the float rename) */
    0xe79bc5d8b096d29full, /* Loop: taken after looking at the frames (a fresh mode choice per start) */
    0xf21a07f42e0c50b9ull, /* CWaves: taken after looking at the frames (single precision, one row copied down) */
    0x06dba6c7177716ebull, /* Vermiculate: taken after looking at the frames */
    0x91be49ba88096f62ull, /* Cynosure: taken after looking at the frames */
    0xcfa8f679f1a10fddull, /* Moire: taken after looking at the frames (single precision) */
    0x32f706208dc0a079ull, /* Moire2: taken after looking at the frames (bit planes) */
    0x0100bedf6ddf582cull, /* HexTrail: taken after looking at the frames (count 6, single precision) */
    0x91947298c096ed6full, /* Celtic: taken after looking at the frames (single precision, graph sized to the screen, step scale 4); re-taken for the eraser (#86), which moved its frames past the stroker's shared-edge slack (#56) */
#ifdef HAVE_AFTER_DARK_TOASTERS
    0xc8300fca8f7bd151ull, /* ad-flyingtoasters (colour, light): taken after looking at the frames, 2x */
#endif
};

void test_frames_of_every_hack_but_maze_match_main(void) {
  TEST_ASSERT_EQUAL_INT((int)(sizeof kBaseline / sizeof kBaseline[0]), g_hack_count);
  for (int i = 0; i < g_hack_count; i++) {
    if (kBaseline[i] == 0) continue;
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(kBaseline[i], hash_after(i, 200),
                                     g_hacks[i]->name);
  }
}

static int distinct_colours(void) {
  static uint8_t seen[65536 / 8];
  memset(seen, 0, sizeof seen);
  int n = 0;
  for (long i = 0; i < (long)cv.w * cv.h; i++) {
    const uint16_t v = cv.px[i];
    if (!(seen[v / 8] & (1 << (v % 8)))) {
      seen[v / 8] |= (uint8_t)(1 << (v % 8));
      n++;
    }
  }
  return n;
}

/* Starfish and Goop were once pinned drawing one or two flat colours: their
 * wrappers redefine `double` as `float`, and spline.h, included by the hack
 * after that, gave the control arrays a float layout that spline.c (built with
 * doubles) read as doubles. A hash taken from that frame proved nothing. */
void test_hacks_that_use_the_spline_draw_a_real_picture(void) {
  static const char *const names[] = {"Starfish", "Goop"};
  for (int k = 0; k < 2; k++) {
    srandom(1);
    HackRunner *r = runner_create(&cv);
    TEST_ASSERT_EQUAL_INT(0, runner_start(r, index_of(names[k])));
    for (int f = 0; f < 200; f++) runner_step(r);
    TEST_ASSERT_TRUE_MESSAGE(distinct_colours() >= 8, names[k]);
    runner_destroy(r);
  }
}

/* Loop picks squares or hexagons at random when it starts, but upstream keeps
 * the choice in a static, so the first start in a process (a boot, on the
 * device) fixed it for good. */
extern int loop_neighbors_in_use(void);

void test_loop_picks_squares_or_hexagons_again_at_every_start(void) {
  int seen4 = 0, seen6 = 0;
  HackRunner *r = runner_create(&cv);
  for (int seed = 1; seed <= 40; seed++) {
    srandom(seed);
    TEST_ASSERT_EQUAL_INT(0, runner_start(r, index_of("Loop")));
    runner_step(r);
    seen4 += loop_neighbors_in_use() == 4;
    seen6 += loop_neighbors_in_use() == 6;
  }
  runner_destroy(r);
  TEST_ASSERT_TRUE_MESSAGE(seen4 > 0 && seen6 > 0, "Loop kept the mode of its first start");
}

/* Not a check: with HACK_PINS_OUT=<file> in the environment it writes the frame
 * hash of every hack whose baseline is 0 (not pinned yet), one `Name 0x...`
 * line each, so a batch of new hacks can be pinned in one run. Look at the
 * frames before pinning them (see "Batches" in the port-hack skill). Maze and
 * Coral stay at 0 on purpose. */
void test_write_the_unpinned_hashes_when_asked(void) {
  const char *out = getenv("HACK_PINS_OUT");
  if (!out) return;
  FILE *f = fopen(out, "w");
  TEST_ASSERT_NOT_NULL(f);
  for (int i = 0; i < g_hack_count; i++) {
    if (kBaseline[i] != 0) continue;
    fprintf(f, "%s 0x%016llx\n", g_hacks[i]->name,
            (unsigned long long)hash_after(i, 200));
  }
  fclose(f);
}

/* The board's malloc(0) returns NULL where the host's returns a pointer, and
 * Loop's square mode asks for a list of rectangles for each of its eight colour
 * states, most of them empty. It took NULL for "out of memory", freed its
 * cells, and drew nothing more: a black screen at 18 fps. */
extern void *(*loop_allocator)(size_t);
static void *malloc_that_fails_for_nothing(size_t n) { return n ? malloc(n) : NULL; }

void test_loop_keeps_drawing_squares_when_malloc_of_nothing_returns_null(void) {
  int changed = 0, found = 0;
  HackRunner *r = runner_create(&cv);
  loop_allocator = malloc_that_fails_for_nothing;
  for (int seed = 1; seed <= 200 && !found; seed++) {
    srandom(seed);
    runner_start(r, index_of("Loop"));
    runner_step(r);
    found = loop_neighbors_in_use() == 4;
  }
  if (found) {
    for (int f = 0; f < 60; f++) runner_step(r);
    uint16_t *before = (uint16_t *)malloc((size_t)cv.w * cv.h * sizeof(*before));
    memcpy(before, cv.px, (size_t)cv.w * cv.h * sizeof(*before));
    for (int f = 0; f < 40; f++) runner_step(r);
    for (long i = 0; i < (long)cv.w * cv.h; i++) changed += cv.px[i] != before[i];
    free(before);
  }
  loop_allocator = malloc;
  runner_destroy(r);
  TEST_ASSERT_TRUE_MESSAGE(found, "no seed made Loop pick squares");
  TEST_ASSERT_TRUE_MESSAGE(changed > 0, "Loop stopped drawing squares");
}

/* Goop blends its blobs at 23/32 only in its default transparent mode; opaque,
 * outline and xor are drawn as they were asked for. */
extern int goop_alpha_for_mode(const char *mode);

void test_goop_blends_only_in_its_transparent_mode(void) {
  const int blend = goop_alpha_for_mode(NULL);
  TEST_ASSERT_TRUE(blend > 0 && blend < 32);
  TEST_ASSERT_EQUAL_INT(blend, goop_alpha_for_mode(""));
  TEST_ASSERT_EQUAL_INT(blend, goop_alpha_for_mode("transparent"));
  TEST_ASSERT_EQUAL_INT(blend, goop_alpha_for_mode("Transparent"));
  TEST_ASSERT_EQUAL_INT(32, goop_alpha_for_mode("opaque"));
  TEST_ASSERT_EQUAL_INT(32, goop_alpha_for_mode("outline"));
  TEST_ASSERT_EQUAL_INT(32, goop_alpha_for_mode("xor"));
  TEST_ASSERT_EQUAL_INT(32, goop_alpha_for_mode("nonsense"));
}

void test_maze_frame_matches_main(void) {
  TEST_ASSERT_EQUAL_UINT64(0x4fa5b289a8d67f2bull, hash_after(index_of("Maze"), 200));
}

/* Pacman restarts its level after the last dot is eaten or the third death,
 * several times in 60,000 frames (checked by watching the dots come back).
 * Growth measured over five seeds is exactly 0, so 512 bytes is slack, and a
 * leaked 1.3 KB level copy per restart is caught. */
void test_pacman_levels_do_not_leak(void) {
  const int pacman = index_of("Pacman");
  TEST_ASSERT_TRUE(pacman >= 0);
  srandom(1);
  HackRunner *r = runner_create(&cv);
  runner_start(r, pacman);
  for (int f = 0; f < 5000; f++) runner_step(r);
  const size_t before = __sanitizer_get_current_allocated_bytes();
  for (int f = 0; f < 60000; f++) runner_step(r);
  const size_t after = __sanitizer_get_current_allocated_bytes();
  runner_destroy(r);
  TEST_ASSERT_TRUE_MESSAGE(after < before + 512, "allocated bytes grew");
}

/* Runs Pacman on a thread whose stack was painted first, and reports how much
 * of it was touched. The board's loop task has 16 KB. `ulimit -s` cannot show
 * this: a level's depth depends on the random numbers drawn, and the first level
 * is a shallow one. */
/* The probe reads how much of a painted stack was touched, so frames must be on
 * it. AddressSanitizer's use-after-return detection (the default on Linux clang,
 * off on macOS) moves them to a heap fake stack and the probe would under-report. */
const char *__asan_default_options(void) { return "detect_stack_use_after_return=0"; }

#define STACK_PROBE_BYTES (1024 * 1024)
#define STACK_PAINT 0xA5
static int probe_seed, probe_frames, probe_hack;

static void *probe_run(void *arg) {
  (void)arg;
  srandom(probe_seed);
  HackRunner *r = runner_create(&cv);
  runner_start(r, probe_hack);
  for (int f = 0; f < probe_frames; f++) runner_step(r);
  runner_destroy(r);
  return NULL;
}

static long stack_used_by(int hack, int seed, int frames) {
  void *mem = NULL;
  if (posix_memalign(&mem, 16384, STACK_PROBE_BYTES) != 0) return -1;
  unsigned char *stack = (unsigned char *)mem;
  memset(stack, STACK_PAINT, STACK_PROBE_BYTES);
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  if (pthread_attr_setstack(&attr, stack, STACK_PROBE_BYTES) != 0) return -2;
  probe_hack = hack;
  probe_seed = seed;
  probe_frames = frames;
  pthread_t t;
  if (pthread_create(&t, &attr, probe_run, NULL) != 0) return -3;
  pthread_join(t, NULL);
  long untouched = 0;
  while (untouched < STACK_PROBE_BYTES && stack[untouched] == STACK_PAINT) untouched++;
  free(stack);
  return STACK_PROBE_BYTES - untouched;
}

/* Pacman has two recursions, both patched out by the build: the level
 * generator (up to 315 KB: a 1.3 KB copy of the level in each of ~200 frames)
 * and the ghosts' route search (453 levels, about 22 KB on the device). With
 * both gone it uses about 12 KB here. This build's frames are inflated by
 * AddressSanitizer (the device's frames are smaller), so staying under the loop
 * task's 16 KB here means staying under it there. */
void test_pacman_stays_within_the_loop_task_stack(void) {
  const int pacman = index_of("Pacman");
  TEST_ASSERT_TRUE(pacman >= 0);
  long worst = 0;
  for (int seed = 1; seed <= 3; seed++) {
    const long used = stack_used_by(pacman, seed, 20000);
    TEST_ASSERT_TRUE_MESSAGE(used >= 0, "could not run the probe");
    if (used > worst) worst = used;
  }
  TEST_ASSERT_TRUE_MESSAGE(worst < 16 * 1024, "Pacman used 16 KB of stack or more");
}

/* Pacman loads and scales its sprite sheet in init, so start and stop is where
 * pixmaps would leak. Growth over 40 restarts is measured at exactly 0, so 512
 * bytes is slack, and a leaked GC per restart is caught. */
void test_pacman_start_and_stop_do_not_leak(void) {
  const int pacman = index_of("Pacman");
  TEST_ASSERT_TRUE(pacman >= 0);
  HackRunner *r = runner_create(&cv);
  for (int i = 0; i < 3; i++) {
    runner_start(r, pacman);
    runner_step(r);
  }
  const size_t before = __sanitizer_get_current_allocated_bytes();
  for (int i = 0; i < 40; i++) {
    runner_start(r, pacman);
    runner_step(r);
    runner_step(r);
  }
  const size_t after = __sanitizer_get_current_allocated_bytes();
  runner_destroy(r);
  TEST_ASSERT_TRUE_MESSAGE(after < before + 512, "allocated bytes grew");
}

/* Bytes still allocated once a runner has run `entry` for `frames` frames from
 * a fresh start and been destroyed. A hack's live state can be any size (Loop's
 * is 650 KB as squares and 1.7 MB as hexagons, by chance), so reading while it
 * runs measures that, not a leak; what survives destroy is what was leaked. The
 * seed is fixed so each reading follows the same path. `cycles: 1` makes an
 * xlockmore hack restart on every frame. */
static size_t held_after_running(const HackEntry *entry, int frames, int cycles_one) {
  const char *merged[2] = {"*cycles: 1", NULL};
  HackEntry fast = *entry;
  if (cycles_one) fast.overrides = merged;
  const HackEntry *const hacks[] = {&fast};
  srandom(1);
  HackRunner *r = runner_create_with(&cv, hacks, 1);
  runner_start(r, 0);
  for (int f = 0; f < frames; f++) runner_step(r);
  runner_destroy(r);
  return __sanitizer_get_current_allocated_bytes();
}

static size_t held_after_starting(int hack, int starts) {
  srandom(1);
  HackRunner *r = runner_create(&cv);
  for (int i = 0; i < starts; i++) {
    runner_start(r, hack);
    runner_step(r);
  }
  runner_destroy(r);
  return __sanitizer_get_current_allocated_bytes();
}

/* An xlockmore hack starts a new picture every `cycles` frames by calling its
 * init again, and its state belongs to xlockmore's MI_INIT. With `cycles: 1` it
 * restarts on every frame, so this catches a restart that allocates (a plain
 * screenhack has no `cycles`, so it is skipped for those), and start and stop
 * catches state the framework fails to free. */
#define LEAK_SLACK_BYTES 128

static void check_restarts_and_stops_do_not_leak(const char *name, int xlockmore) {
  const int hack = index_of(name);
  TEST_ASSERT_TRUE_MESSAGE(hack >= 0, name);

  if (xlockmore) {
    const size_t early = held_after_running(g_hacks[hack], 20, 1);
    const size_t late = held_after_running(g_hacks[hack], 320, 1);
    TEST_ASSERT_TRUE_MESSAGE(late < early + LEAK_SLACK_BYTES,
                             "allocated bytes grew over in-hack restarts");
  }

  /* What is held at rest should be the same however many times it was started.
   * Several readings, not one before and one after: state kept at rest but sized
   * by a random choice (a table for the mode last picked) matches by luck in
   * a single pair, and shows as a spread across these. */
  static const int starts[] = {3, 4, 5, 6, 7, 8, 43};
  size_t lowest = (size_t)-1, highest = 0;
  for (size_t k = 0; k < sizeof starts / sizeof starts[0]; k++) {
    const size_t held = held_after_starting(hack, starts[k]);
    if (held < lowest) lowest = held;
    if (held > highest) highest = held;
  }
  TEST_ASSERT_TRUE_MESSAGE(highest < lowest + LEAK_SLACK_BYTES,
                           "bytes held at rest changed with the number of starts");
}

void test_braid_restarts_and_stops_do_not_leak(void) {
  check_restarts_and_stops_do_not_leak("Braid", 1);
}

void test_mountain_restarts_and_stops_do_not_leak(void) {
  check_restarts_and_stops_do_not_leak("Mountain", 1);
}

void test_deluxe_stops_do_not_leak(void) {
  check_restarts_and_stops_do_not_leak("Deluxe", 0);
}

void test_epicycle_stops_do_not_leak(void) {
  check_restarts_and_stops_do_not_leak("Epicycle", 0);
}

void test_kaleidescope_stops_do_not_leak(void) {
  check_restarts_and_stops_do_not_leak("Kaleidescope", 0);
}
/* Braid calls sin and cos for every segment, and the S3's libm does them in
 * software. fast_sinf and fast_cosf must agree with libm to 2e-6 (a pixel is
 * a few hundred times that) over the angles a hack uses, with the argument
 * rounded to float first since that is all the hack passes. */
void test_fast_sin_and_cos_stay_within_2e6_of_libm(void) {
  double worst = 0;
  for (double x = -60.0; x <= 60.0; x += 0.00037) {
    const float f = (float)x;
    const double es = fabs((double)fast_sinf(f) - sin((double)f));
    const double ec = fabs((double)fast_cosf(f) - cos((double)f));
    if (es > worst) worst = es;
    if (ec > worst) worst = ec;
  }
  TEST_ASSERT_TRUE_MESSAGE(worst < 2e-6, "fast sin or cos is 2e-6 or more off libm");
}

/* Pacman's ghosts find their way home with a depth-first search (find_home)
 * that recurses up to 453 levels. The build rewrites it with an explicit stack,
 * so this pins what the ghosts do over 30,000 frames: every ghost's position
 * every 100 frames, and the frame every 1,000, taken with the original
 * recursion. `trips` counts the frames a ghost was following a route home, so
 * the pin cannot pass without the search having run. */
void test_pacman_ghosts_take_the_same_routes_home(void) {
  const int pacman = index_of("Pacman");
  TEST_ASSERT_TRUE(pacman >= 0);
  srandom(1);
  HackRunner *r = runner_create(&cv);
  runner_start(r, pacman);
  uint64_t chain = 0xcbf29ce484222325ull;
  long trips = 0;
  for (int f = 1; f <= 30000; f++) {
    runner_step(r);
    const pacmangamestruct *pp = &pacman_games[0];
    for (unsigned g = 0; g < pp->nghosts; g++) {
      if (pp->ghosts[g].home_count > 0) trips++;
      if (f % 100 == 0) {
        chain = (chain ^ (uint64_t)(pp->ghosts[g].row * 4099 + pp->ghosts[g].col)) * 0x100000001b3ull;
      }
    }
    if (f % 1000 == 0) chain = (chain ^ frame_hash()) * 0x100000001b3ull;
  }
  runner_destroy(r);
  TEST_ASSERT_TRUE_MESSAGE(trips > 0, "no ghost ever followed a route home");
  TEST_ASSERT_EQUAL_UINT64(2287115883820344180ull, chain);
}

void test_prev_from_first_wraps_to_last_hack(void) {
  HackRunner *r = runner_create(&cv);
  runner_start(r, 0);
  TEST_ASSERT_EQUAL_INT(g_hack_count - 1, runner_prev(r));
  runner_step(r);
  runner_destroy(r);
}

static void start_step_and_stop_gl_hack(int index, const char *name) {
  HackRunner *r = runner_create(&cv);
  runner_start(r, index);
  for (int f = 0; f < 5; f++) runner_step(r);
  TEST_ASSERT_TRUE_MESSAGE(glshim_is_open(), name);
  runner_destroy(r);
  /* Checked here, not left to the next start: opening a context closes the
   * old one, which would hide a missing release hook. */
  TEST_ASSERT_FALSE_MESSAGE(glshim_is_open(), name);
}

/* The GL context (a 434 KB z-buffer on the board) is freed by the shim's
 * release hook when the hack stops, and every display list goes with it:
 * without the hook each visit to Gears would leave about 1 to 2 MB behind.
 * Measure while stopped, not while running: a running Gears holds 0.9 to
 * 2.5 MB depending on its random layout. Upstream's free_gears never frees
 * its `bp->gears` array, so each start still leaks 40 to 1,500 bytes (13 KB
 * over these 40 starts, with srandom(1)); 64 KB is well above that and far
 * below one leaked context. */
void test_gears_start_and_stop_do_not_leak(void) {
  const int gears = index_of("Gears");
  TEST_ASSERT_TRUE(gears >= 0);
  srandom(1);
  for (int i = 0; i < 3; i++) start_step_and_stop_gl_hack(gears, "Gears");
  const size_t before = __sanitizer_get_current_allocated_bytes();
  for (int i = 0; i < 40; i++) start_step_and_stop_gl_hack(gears, "Gears");
  const size_t after = __sanitizer_get_current_allocated_bytes();
  TEST_ASSERT_TRUE_MESSAGE(after < before + 64 * 1024, "allocated bytes grew");
}

/* Morph3D builds no display lists and keeps one small struct, so the only
 * thing that can grow is the GL context itself: 64 KB is far below one. */
void test_morph3d_start_and_stop_do_not_leak(void) {
  const int morph3d = index_of("Morph3D");
  TEST_ASSERT_TRUE(morph3d >= 0);
  srandom(1);
  for (int i = 0; i < 3; i++) start_step_and_stop_gl_hack(morph3d, "Morph3D");
  const size_t before = __sanitizer_get_current_allocated_bytes();
  for (int i = 0; i < 40; i++) start_step_and_stop_gl_hack(morph3d, "Morph3D");
  const size_t after = __sanitizer_get_current_allocated_bytes();
  TEST_ASSERT_TRUE_MESSAGE(after < before + 64 * 1024, "allocated bytes grew");
}

/* CubicGrid builds one display list of 27,000 points and frees it on stop;
 * what could leak is that list and the GL context. */
void test_cubicgrid_start_and_stop_do_not_leak(void) {
  const int cubicgrid = index_of("CubicGrid");
  TEST_ASSERT_TRUE(cubicgrid >= 0);
  srandom(1);
  for (int i = 0; i < 3; i++) start_step_and_stop_gl_hack(cubicgrid, "CubicGrid");
  const size_t before = __sanitizer_get_current_allocated_bytes();
  for (int i = 0; i < 40; i++) start_step_and_stop_gl_hack(cubicgrid, "CubicGrid");
  const size_t after = __sanitizer_get_current_allocated_bytes();
  TEST_ASSERT_TRUE_MESSAGE(after < before + 64 * 1024, "allocated bytes grew");
}

/* CubicGrid draws ticks cubed points: 27,000 at its default of 30, which ran
 * at 5.6 to 6 fps on the board. The registry cuts the grid to 20 (8,000). */
void test_cubicgrid_registers_a_smaller_grid(void) {
  const int cubicgrid = index_of("CubicGrid");
  TEST_ASSERT_TRUE(cubicgrid >= 0);
  int found = 0;
  const char *const *o = g_hacks[cubicgrid]->overrides;
  for (; o && *o; o++) found |= strcmp(*o, "*ticks: 20") == 0;
  TEST_ASSERT_TRUE_MESSAGE(found, "CubicGrid does not override *ticks");
}

/* Morph3D asks for a 40 ms pause after every frame. With the dirty rectangle a
 * frame takes about 57 ms, so the pause alone held it to 10 fps; 10 ms gives
 * 14.6. */
void test_morph3d_registers_a_shorter_pause(void) {
  const int morph3d = index_of("Morph3D");
  TEST_ASSERT_TRUE(morph3d >= 0);
  int found = 0;
  const char *const *o = g_hacks[morph3d]->overrides;
  for (; o && *o; o++) found |= strcmp(*o, "*delay: 10000") == 0;
  TEST_ASSERT_TRUE_MESSAGE(found, "Morph3D does not override *delay");
}

/* Gears asks for a 30 ms pause after every frame. With the dirty rectangle a
 * light layout takes 44 to 63 ms, so the pause alone held it to 11 to 13 fps. */
void test_gears_registers_a_shorter_pause(void) {
  const int gears = index_of("Gears");
  TEST_ASSERT_TRUE(gears >= 0);
  int found = 0;
  const char *const *o = g_hacks[gears]->overrides;
  for (; o && *o; o++) found |= strcmp(*o, "*delay: 10000") == 0;
  TEST_ASSERT_TRUE_MESSAGE(found, "Gears does not override *delay");
}

/* UnknownPleasures holds 40 rows of two display lists each, and the context. */
void test_unknownpleasures_start_and_stop_do_not_leak(void) {
  const int unk = index_of("UnknownPleasures");
  TEST_ASSERT_TRUE(unk >= 0);
  srandom(1);
  for (int i = 0; i < 3; i++) start_step_and_stop_gl_hack(unk, "UnknownPleasures");
  const size_t before = __sanitizer_get_current_allocated_bytes();
  for (int i = 0; i < 40; i++) start_step_and_stop_gl_hack(unk, "UnknownPleasures");
  const size_t after = __sanitizer_get_current_allocated_bytes();
  TEST_ASSERT_TRUE_MESSAGE(after < before + 64 * 1024, "allocated bytes grew");
}

/* A tripwire, not a proof. On the host this runs unoptimised under
 * AddressSanitizer, which inflates frames several times over: Gears measures
 * 21 KB here against a 2.4 KB high-water mark on the board (the loop task has
 * 16 KB). The bound only catches a sudden recursion or a large local array. The
 * real check is on the device. Three seeds give different layouts. */
void test_gears_stack_use_on_the_host_has_not_run_away(void) {
  const int gears = index_of("Gears");
  TEST_ASSERT_TRUE(gears >= 0);
  long worst = 0;
  for (int seed = 1; seed <= 3; seed++) {
    const long used = stack_used_by(gears, seed, 300);
    TEST_ASSERT_TRUE_MESSAGE(used >= 0, "could not run the probe");
    if (used > worst) worst = used;
  }
  TEST_ASSERT_TRUE_MESSAGE(worst < 32 * 1024, "Gears used 32 KB of host stack or more");
}

void test_forest_stops_do_not_leak(void) {
  check_restarts_and_stops_do_not_leak("Forest", 1);
}

void test_laser_stops_do_not_leak(void) {
  check_restarts_and_stops_do_not_leak("Laser", 1);
}

void test_lissie_stops_do_not_leak(void) {
  check_restarts_and_stops_do_not_leak("Lissie", 1);
}

void test_rotor_stops_do_not_leak(void) {
  check_restarts_and_stops_do_not_leak("Rotor", 1);
}

void test_scooter_stops_do_not_leak(void) {
  check_restarts_and_stops_do_not_leak("Scooter", 1);
}

void test_grav_stops_do_not_leak(void) {
  check_restarts_and_stops_do_not_leak("Grav", 1);
}

void test_euler2d_stops_do_not_leak(void) {
  check_restarts_and_stops_do_not_leak("Euler2D", 1);
}

void test_anemone_stops_do_not_leak(void) {
  check_restarts_and_stops_do_not_leak("Anemone", 0);
}

void test_anemotaxis_stops_do_not_leak(void) {
  check_restarts_and_stops_do_not_leak("Anemotaxis", 0);
}

void test_compass_stops_do_not_leak(void) {
  check_restarts_and_stops_do_not_leak("Compass", 0);
}

void test_fuzzyflakes_stops_do_not_leak(void) {
  check_restarts_and_stops_do_not_leak("FuzzyFlakes", 0);
}

void test_ifs_stops_do_not_leak(void) {
  check_restarts_and_stops_do_not_leak("IFS", 0);
}

void test_interaggregate_stops_do_not_leak(void) {
  check_restarts_and_stops_do_not_leak("Interaggregate", 0);
}

void test_lmorph_stops_do_not_leak(void) {
  check_restarts_and_stops_do_not_leak("LMorph", 0);
}

void test_wormhole_stops_do_not_leak(void) {
  check_restarts_and_stops_do_not_leak("Wormhole", 0);
}

/* Truchet holds an 866 by 866 pixmap (1.5 MB) for as long as it runs. */
void test_truchet_stops_do_not_leak(void) {
  check_restarts_and_stops_do_not_leak("Truchet", 0);
}

/* Each of these runs below 15 fps at upstream's defaults on the board: Euler2D
 * moves 1024 particles, Anemone grows 128 arms, FuzzyFlakes drops a flake
 * density of 5, and LMorph pauses 70 ms between frames. */
static int overrides_contain(const char *hack, const char *wanted) {
  const int i = index_of(hack);
  if (i < 0) return 0;
  const char *const *o = g_hacks[i]->overrides;
  for (; o && *o; o++)
    if (strcmp(*o, wanted) == 0) return 1;
  return 0;
}

void test_slow_hacks_register_their_lighter_settings(void) {
  TEST_ASSERT_TRUE_MESSAGE(overrides_contain("Euler2D", "*count: 512"), "Euler2D *count");
  TEST_ASSERT_TRUE_MESSAGE(overrides_contain("Anemone", "*arms: 64"), "Anemone *arms");
  TEST_ASSERT_TRUE_MESSAGE(overrides_contain("FuzzyFlakes", "*density: 3"),
                           "FuzzyFlakes *density");
  TEST_ASSERT_TRUE_MESSAGE(overrides_contain("LMorph", "*delay: 10000"), "LMorph *delay");
  TEST_ASSERT_TRUE_MESSAGE(overrides_contain("Loop", "*delay: 50000"), "Loop *delay");
}

void test_hexadrop_stops_do_not_leak(void) {
  check_restarts_and_stops_do_not_leak("Hexadrop", 0);
}

void test_stonerview_start_and_stop_do_not_leak(void) {
  const int sv = index_of("StonerView");
  TEST_ASSERT_TRUE(sv >= 0);
  srandom(1);
  for (int i = 0; i < 3; i++) start_step_and_stop_gl_hack(sv, "StonerView");
  const size_t before = __sanitizer_get_current_allocated_bytes();
  for (int i = 0; i < 40; i++) start_step_and_stop_gl_hack(sv, "StonerView");
  const size_t after = __sanitizer_get_current_allocated_bytes();
  TEST_ASSERT_TRUE_MESSAGE(after <= before + 4096, "StonerView leaks per start");
}

void test_halo_stops_do_not_leak(void) {
  check_restarts_and_stops_do_not_leak("Halo", 0);
}

void test_xmatrix_stops_do_not_leak(void) {
  check_restarts_and_stops_do_not_leak("XMatrix", 0);
}

void test_qix_stops_do_not_leak(void) {
  check_restarts_and_stops_do_not_leak("Qix", 0);
}

void test_fireworkx_stops_do_not_leak(void) {
  check_restarts_and_stops_do_not_leak("Fireworkx", 0);
}

void test_starfish_stops_do_not_leak(void) {
  check_restarts_and_stops_do_not_leak("Starfish", 0);
}

void test_goop_stops_do_not_leak(void) {
  check_restarts_and_stops_do_not_leak("Goop", 0);
}

void test_loop_stops_do_not_leak(void) {
  check_restarts_and_stops_do_not_leak("Loop", 1);
}

void test_cwaves_stops_do_not_leak(void) {
  check_restarts_and_stops_do_not_leak("CWaves", 0);
}

void test_vermiculate_stops_do_not_leak(void) {
  check_restarts_and_stops_do_not_leak("Vermiculate", 0);
}

void test_cynosure_stops_do_not_leak(void) {
  check_restarts_and_stops_do_not_leak("Cynosure", 0);
}

void test_moire_stops_do_not_leak(void) {
  check_restarts_and_stops_do_not_leak("Moire", 0);
}

void test_moire2_stops_do_not_leak(void) {
  check_restarts_and_stops_do_not_leak("Moire2", 0);
}

void test_hextrail_stops_do_not_leak(void) {
  check_restarts_and_stops_do_not_leak("HexTrail", 1);
}

void test_celtic_stops_do_not_leak(void) {
  check_restarts_and_stops_do_not_leak("Celtic", 0);
}

/* Celtic builds a graph, its edge couples and a spline segment for every
 * directed edge, and upstream picks margins of up to 900 pixels outside the
 * screen: unpatched, one pattern held 1.1 MB on the host and the board has
 * 312 KB of free internal heap. The limit is about a third of that, in bytes
 * held on the host, where 64-bit pointers make a pattern somewhat bigger than
 * it is on the board. The worst of 150 patterns is now 125 KB. */
#define CELTIC_HEAP_LIMIT_BYTES 150000ull

static size_t celtic_pattern_bytes(int hack, unsigned seed) {
  srandom(seed);
  HackRunner *r = runner_create(&cv);
  const size_t before = __sanitizer_get_current_allocated_bytes();
  runner_start(r, hack);
  runner_step(r);
  const size_t during = __sanitizer_get_current_allocated_bytes();
  runner_destroy(r);
  return during - before;
}

void test_celtic_patterns_leave_most_of_the_boards_heap_free(void) {
  const int celtic = index_of("Celtic");
  TEST_ASSERT_TRUE(celtic >= 0);
  size_t worst = 0;
  unsigned worst_seed = 0;
  for (unsigned seed = 1; seed <= 150; seed++) {
    const size_t bytes = celtic_pattern_bytes(celtic, seed);
    if (bytes > worst) {
      worst = bytes;
      worst_seed = seed;
    }
  }
  char message[64];
  snprintf(message, sizeof message, "worst pattern is seed %u", worst_seed);
  TEST_ASSERT_LESS_THAN_UINT64_MESSAGE(CELTIC_HEAP_LIMIT_BYTES, worst, message);
}

#ifdef HAVE_AFTER_DARK_TOASTERS
/* The runner waits the returned delay after the push, so the hack subtracts
 * the time its own step took to keep to 30 steps a second. */
static double step_cost;
static int step_reads;
static double step_clock(void) {
  return fake_now + (step_reads++ % 2 ? step_cost : 0.0);
}

static unsigned long toasters_delay_with_step_taking(double seconds) {
  const HackEntry *e = g_hacks[index_of("ad-flyingtoasters")];
  Display *dpy = xshim_open_display(&cv);
  void *st = e->init(dpy, XSHIM_WINDOW);
  TEST_ASSERT_NOT_NULL(st);
  step_cost = seconds;
  step_reads = 0;
  xshim_set_clock(step_clock);
  const unsigned long delay = e->draw(dpy, XSHIM_WINDOW, st);
  xshim_set_clock(NULL);
  e->free(dpy, XSHIM_WINDOW, st);
  xshim_close_display(dpy);
  return delay;
}

void test_toasters_delay_is_a_thirtieth_of_a_second_less_the_step(void) {
  TEST_ASSERT_UINT_WITHIN(1, 23333, toasters_delay_with_step_taking(0.010));
  TEST_ASSERT_EQUAL_UINT(0, toasters_delay_with_step_taking(0.040));
}

/* Each start builds five sprite images and frees them at stop. */
void test_restart_does_not_leak(void) {
  const int hack = index_of("ad-flyingtoasters");
  TEST_ASSERT_TRUE(hack >= 0);
  const size_t early = held_after_starting(hack, 3);
  const size_t late = held_after_starting(hack, 53);
  TEST_ASSERT_TRUE_MESSAGE(late < early + LEAK_SLACK_BYTES,
                           "bytes held at rest grew over 50 more starts");
}
#endif

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_registry_lists_hacks_in_order);
  RUN_TEST(test_only_unknownpleasures_asks_for_refresh_synced_pushes);
  RUN_TEST(test_every_hack_can_be_stopped_before_its_first_frame);
  RUN_TEST(test_a_hack_starts_on_the_background_colour_it_asks_for);
  RUN_TEST(test_a_hack_that_never_draws_on_white_is_not_drawing);
  RUN_TEST(test_a_window_background_does_not_outlive_the_hack_that_set_it);
  RUN_TEST(test_the_runner_releases_pixmaps_a_stopped_hack_left_behind);
  RUN_TEST(test_every_hack_draws_something_within_2000_frames);
  RUN_TEST(test_every_hack_runs_3000_frames_cleanly_with_sane_delays);
  RUN_TEST(test_cycling_through_all_hacks_100_times_is_asan_clean);
  RUN_TEST(test_galaxy_restarts_do_not_leak_with_its_registered_overrides);
  RUN_TEST(test_maze_cycles_do_not_leak);
  RUN_TEST(test_substrate_restarts_do_not_leak);
  RUN_TEST(test_substrate_draws_what_it_drew_before_pixels_were_swapped);
  RUN_TEST(test_frames_of_every_hack_but_maze_match_main);
  RUN_TEST(test_hacks_that_use_the_spline_draw_a_real_picture);
  RUN_TEST(test_loop_picks_squares_or_hexagons_again_at_every_start);
  RUN_TEST(test_write_the_unpinned_hashes_when_asked);
  RUN_TEST(test_loop_keeps_drawing_squares_when_malloc_of_nothing_returns_null);
  RUN_TEST(test_goop_blends_only_in_its_transparent_mode);
  RUN_TEST(test_maze_frame_matches_main);
  RUN_TEST(test_pacman_levels_do_not_leak);
  RUN_TEST(test_pacman_start_and_stop_do_not_leak);
  RUN_TEST(test_pacman_stays_within_the_loop_task_stack);
  RUN_TEST(test_pacman_ghosts_take_the_same_routes_home);
  RUN_TEST(test_braid_restarts_and_stops_do_not_leak);
  RUN_TEST(test_mountain_restarts_and_stops_do_not_leak);
  RUN_TEST(test_deluxe_stops_do_not_leak);
  RUN_TEST(test_epicycle_stops_do_not_leak);
  RUN_TEST(test_kaleidescope_stops_do_not_leak);
  RUN_TEST(test_gears_start_and_stop_do_not_leak);
  RUN_TEST(test_gears_registers_a_shorter_pause);
  RUN_TEST(test_morph3d_start_and_stop_do_not_leak);
  RUN_TEST(test_morph3d_registers_a_shorter_pause);
  RUN_TEST(test_cubicgrid_start_and_stop_do_not_leak);
  RUN_TEST(test_unknownpleasures_start_and_stop_do_not_leak);
  RUN_TEST(test_hexadrop_stops_do_not_leak);
  RUN_TEST(test_stonerview_start_and_stop_do_not_leak);
  RUN_TEST(test_halo_stops_do_not_leak);
  RUN_TEST(test_xmatrix_stops_do_not_leak);
  RUN_TEST(test_cubicgrid_registers_a_smaller_grid);
  RUN_TEST(test_forest_stops_do_not_leak);
  RUN_TEST(test_laser_stops_do_not_leak);
  RUN_TEST(test_lissie_stops_do_not_leak);
  RUN_TEST(test_rotor_stops_do_not_leak);
  RUN_TEST(test_scooter_stops_do_not_leak);
  RUN_TEST(test_grav_stops_do_not_leak);
  RUN_TEST(test_euler2d_stops_do_not_leak);
  RUN_TEST(test_anemone_stops_do_not_leak);
  RUN_TEST(test_anemotaxis_stops_do_not_leak);
  RUN_TEST(test_compass_stops_do_not_leak);
  RUN_TEST(test_fuzzyflakes_stops_do_not_leak);
  RUN_TEST(test_ifs_stops_do_not_leak);
  RUN_TEST(test_interaggregate_stops_do_not_leak);
  RUN_TEST(test_lmorph_stops_do_not_leak);
  RUN_TEST(test_wormhole_stops_do_not_leak);
  RUN_TEST(test_truchet_stops_do_not_leak);
  RUN_TEST(test_slow_hacks_register_their_lighter_settings);
  RUN_TEST(test_qix_stops_do_not_leak);
  RUN_TEST(test_fireworkx_stops_do_not_leak);
  RUN_TEST(test_starfish_stops_do_not_leak);
  RUN_TEST(test_goop_stops_do_not_leak);
  RUN_TEST(test_loop_stops_do_not_leak);
  RUN_TEST(test_cwaves_stops_do_not_leak);
  RUN_TEST(test_vermiculate_stops_do_not_leak);
  RUN_TEST(test_cynosure_stops_do_not_leak);
  RUN_TEST(test_moire_stops_do_not_leak);
  RUN_TEST(test_moire2_stops_do_not_leak);
  RUN_TEST(test_hextrail_stops_do_not_leak);
  RUN_TEST(test_celtic_stops_do_not_leak);
  RUN_TEST(test_celtic_patterns_leave_most_of_the_boards_heap_free);
#ifdef HAVE_AFTER_DARK_TOASTERS
  RUN_TEST(test_toasters_delay_is_a_thirtieth_of_a_second_less_the_step);
  RUN_TEST(test_restart_does_not_leak);
#endif
  RUN_TEST(test_gears_stack_use_on_the_host_has_not_run_away);
  RUN_TEST(test_fast_sin_and_cos_stay_within_2e6_of_libm);
  RUN_TEST(test_prev_from_first_wraps_to_last_hack);
  return UNITY_END();
}
