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
                                         "Lightning"};
  const int n = sizeof(expected) / sizeof(expected[0]);
  TEST_ASSERT_EQUAL_INT(n, g_hack_count);
  for (int i = 0; i < n; i++) TEST_ASSERT_EQUAL_STRING(expected[i], g_hacks[i]->name);
}

void test_every_hack_draws_something_within_2000_frames(void) {
  for (int i = 0; i < g_hack_count; i++) {
    HackRunner *r = runner_create(&cv);
    TEST_ASSERT_EQUAL_INT(0, runner_start(r, i));
    int seen = 0;
    for (int f = 0; f < 2000 && !seen; f++) {
      runner_step(r);
      seen = non_black() >= 20;
    }
    TEST_ASSERT_TRUE_MESSAGE(seen, g_hacks[i]->name);
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

/* Galaxy restarts every 4 * cycles frames, and with some counts its restart
 * leaks the star rectangle buffers. Shorten the cycle to see many restarts. */
void test_galaxy_restarts_do_not_leak_with_its_registered_overrides(void) {
  const HackEntry *galaxy = NULL;
  for (int i = 0; i < g_hack_count; i++)
    if (strcmp(g_hacks[i]->name, "Galaxy") == 0) galaxy = g_hacks[i];
  TEST_ASSERT_NOT_NULL(galaxy);

  const char *merged[8] = {"*cycles: 5"};
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
  for (int f = 0; f < 400; f++) runner_step(r);
  const size_t after = __sanitizer_get_current_allocated_bytes();
  runner_destroy(r);
  TEST_ASSERT_TRUE_MESSAGE(after < before + 4096, "allocated bytes grew");
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
  RUN_TEST(test_every_hack_draws_something_within_2000_frames);
  RUN_TEST(test_every_hack_runs_3000_frames_cleanly_with_sane_delays);
  RUN_TEST(test_cycling_through_all_hacks_100_times_is_asan_clean);
  RUN_TEST(test_galaxy_restarts_do_not_leak_with_its_registered_overrides);
  RUN_TEST(test_prev_from_first_wraps_to_last_hack);
  return UNITY_END();
}
