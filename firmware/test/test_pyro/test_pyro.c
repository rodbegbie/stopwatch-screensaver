#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include "core/canvas.h"
#include "hacks/registry.h"
#include "hsv.h"
#include "runner/hack_runner.h"

static Canvas cv;

void setUp(void) { TEST_ASSERT_EQUAL_INT(0, canvas_init(&cv, 466, 466, malloc)); }
void tearDown(void) { canvas_free(&cv); }

static int non_black(void) {
  int n = 0;
  for (long i = 0; i < (long)cv.w * cv.h; i++) n += cv.px[i] != 0;
  return n;
}

static void *stub_init(Display *d, Window w) { (void)d; (void)w; return calloc(1, 8); }
static void stub_free(Display *d, Window w, void *c) { (void)d; (void)w; free(c); }
static unsigned long zero_draw(Display *d, Window w, void *c) { (void)d; (void)w; (void)c; return 0; }
static unsigned long huge_draw(Display *d, Window w, void *c) { (void)d; (void)w; (void)c; return ULONG_MAX; }
static const HackEntry zero_hack = {"zero", NULL, stub_init, zero_draw, stub_free};
static const HackEntry huge_hack = {"huge", NULL, stub_init, huge_draw, stub_free};
static const HackEntry *const stubs[] = {&zero_hack, &huge_hack};

void test_pyro_is_registered(void) {
  TEST_ASSERT_TRUE(g_hack_count >= 1);
  TEST_ASSERT_EQUAL_STRING("Pyro", g_hacks[0]->name);
}

void test_pyro_draws_something_within_300_frames(void) {
  HackRunner *r = runner_create(&cv);
  TEST_ASSERT_EQUAL_INT(0, runner_start(r, 0));
  for (int i = 0; i < 300; i++) runner_step(r);
  TEST_ASSERT_TRUE(non_black() >= 50);
  runner_destroy(r);
}

void test_pyro_pixels_stay_in_canvas_for_1000_frames(void) {
  HackRunner *r = runner_create(&cv);
  runner_start(r, 0);
  for (int i = 0; i < 1000; i++) runner_step(r);
  runner_destroy(r);
}

void test_step_delay_is_clamped(void) {
  HackRunner *r = runner_create_with(&cv, stubs, 2);
  runner_start(r, 0);
  TEST_ASSERT_EQUAL_UINT32(1000, runner_step(r));
  runner_start(r, 1);
  TEST_ASSERT_EQUAL_UINT32(1000000, runner_step(r));
  runner_destroy(r);
}

void test_pyro_delay_is_in_clamp_range(void) {
  HackRunner *r = runner_create(&cv);
  runner_start(r, 0);
  unsigned long d = runner_step(r);
  TEST_ASSERT_TRUE(d >= 1000 && d <= 1000000);
  runner_destroy(r);
}

void test_switching_100_times_is_asan_clean(void) {
  HackRunner *r = runner_create(&cv);
  runner_start(r, 0);
  for (int i = 0; i < 100; i++) {
    for (int s = 0; s < 5; s++) runner_step(r);
    TEST_ASSERT_EQUAL_INT((i + 1) % g_hack_count, runner_next(r));
  }
  runner_destroy(r);
}

void test_start_clears_canvas(void) {
  HackRunner *r = runner_create(&cv);
  canvas_clear(&cv, 0xFFFF);
  runner_start(r, 0);
  TEST_ASSERT_EQUAL_INT(0, non_black());
  runner_destroy(r);
}

void test_next_and_prev_wrap(void) {
  HackRunner *r = runner_create_with(&cv, stubs, 2);
  runner_start(r, 0);
  TEST_ASSERT_EQUAL_INT(1, runner_next(r));
  TEST_ASSERT_EQUAL_INT(0, runner_next(r));
  TEST_ASSERT_EQUAL_INT(1, runner_prev(r));
  TEST_ASSERT_EQUAL_INT(1, runner_index(r));
  runner_destroy(r);
}

void test_start_rejects_bad_index(void) {
  HackRunner *r = runner_create(&cv);
  TEST_ASSERT_EQUAL_INT(-1, runner_start(r, 5));
  TEST_ASSERT_EQUAL_INT(-1, runner_start(r, -1));
  runner_destroy(r);
}

void test_hsv_to_rgb_pure_red_h0(void) {
  unsigned short r, g, b;
  hsv_to_rgb(0, 1.0, 1.0, &r, &g, &b);
  TEST_ASSERT_EQUAL_UINT16(65535, r);
  TEST_ASSERT_EQUAL_UINT16(0, g);
  TEST_ASSERT_EQUAL_UINT16(0, b);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_pyro_is_registered);
  RUN_TEST(test_pyro_draws_something_within_300_frames);
  RUN_TEST(test_pyro_pixels_stay_in_canvas_for_1000_frames);
  RUN_TEST(test_step_delay_is_clamped);
  RUN_TEST(test_pyro_delay_is_in_clamp_range);
  RUN_TEST(test_switching_100_times_is_asan_clean);
  RUN_TEST(test_start_clears_canvas);
  RUN_TEST(test_next_and_prev_wrap);
  RUN_TEST(test_start_rejects_bad_index);
  RUN_TEST(test_hsv_to_rgb_pure_red_h0);
  return UNITY_END();
}
