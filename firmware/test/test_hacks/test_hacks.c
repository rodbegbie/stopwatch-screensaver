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
  RUN_TEST(test_prev_from_first_wraps_to_last_hack);
  return UNITY_END();
}
