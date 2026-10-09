#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sanitizer/allocator_interface.h>
#include <unity.h>

#include "core/canvas.h"
#include "hacks/registry.h"
#include "runner/hack_runner.h"

static Canvas cv;

void setUp(void) { TEST_ASSERT_EQUAL_INT(0, canvas_init(&cv, 466, 466, malloc)); }
void tearDown(void) { canvas_free(&cv); }

static int non_black(void) {
  int n = 0;
  for (long i = 0; i < (long)cv.w * cv.h; i++) n += cv.px[i] != 0;
  return n;
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
                                         "Substrate"};
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
    0xd259e4a8a0989583ull, /* XSpirograph */
    0x9b337f3f1305e6d1ull, /* Petri */
    0x8ae10273e493330dull, /* Helix */
    0x335fcd65811fe911ull, /* Rorschach */
    0xa4f79500bd791711ull, /* Pedal */
    0, /* Coral reads the clock: not deterministic, so not pinned */
    0xdabd390a15578da4ull, /* Squiral */
    0x19c5b220193ad705ull, /* Critical */
    0x80bcaaef5d53f475ull, /* CloudLife */
    0x9fc760a330da4c12ull, /* WhirlWindWarp */
    0x9ade65fba7afeb1aull, /* Flame */
    0x29a9c8bcad390b11ull, /* Hopalong */
    0xc85b7a1763d1d294ull, /* Vines */
    0x3703b4d6790c0143ull, /* Sierpinski */
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
};

void test_frames_of_every_hack_but_maze_match_main(void) {
  TEST_ASSERT_EQUAL_INT((int)(sizeof kBaseline / sizeof kBaseline[0]), g_hack_count);
  for (int i = 0; i < g_hack_count; i++) {
    if (kBaseline[i] == 0) continue;
    TEST_ASSERT_EQUAL_UINT64_MESSAGE(kBaseline[i], hash_after(i, 200),
                                     g_hacks[i]->name);
  }
}

void test_maze_frame_matches_main(void) {
  TEST_ASSERT_EQUAL_UINT64(0x0693082a4399bc39ull, hash_after(index_of("Maze"), 200));
}

void test_prev_from_first_wraps_to_last_hack(void) {
  HackRunner *r = runner_create(&cv);
  runner_start(r, 0);
  TEST_ASSERT_EQUAL_INT(g_hack_count - 1, runner_prev(r));
  runner_step(r);
  runner_destroy(r);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_registry_lists_hacks_in_order);
  RUN_TEST(test_every_hack_can_be_stopped_before_its_first_frame);
  RUN_TEST(test_a_hack_starts_on_the_background_colour_it_asks_for);
  RUN_TEST(test_a_hack_that_never_draws_on_white_is_not_drawing);
  RUN_TEST(test_every_hack_draws_something_within_2000_frames);
  RUN_TEST(test_every_hack_runs_3000_frames_cleanly_with_sane_delays);
  RUN_TEST(test_cycling_through_all_hacks_100_times_is_asan_clean);
  RUN_TEST(test_galaxy_restarts_do_not_leak_with_its_registered_overrides);
  RUN_TEST(test_maze_cycles_do_not_leak);
  RUN_TEST(test_substrate_restarts_do_not_leak);
  RUN_TEST(test_substrate_draws_what_it_drew_before_pixels_were_swapped);
  RUN_TEST(test_frames_of_every_hack_but_maze_match_main);
  RUN_TEST(test_maze_frame_matches_main);
  RUN_TEST(test_prev_from_first_wraps_to_last_hack);
  return UNITY_END();
}
