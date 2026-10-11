#include <sanitizer/allocator_interface.h>
#include <stdlib.h>
#include <unity.h>

#include "core/canvas.h"
#include "doubletime.h"
#include "erase.h"
#include "x11shim/xshim.h"

enum { kSize = 466 };
static const uint16_t kPicture = 0x1234;

static Canvas cv;
static Display *dpy;
static double now;
static size_t live_before;

static double test_clock(void) { return now; }

void setUp(void) {
  live_before = __sanitizer_get_current_allocated_bytes();
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&cv, kSize, kSize, malloc));
  dpy = xshim_open_display(&cv);
  now = 1000.0;
  xshim_set_clock(test_clock);
  xshim_erase_force_effect(0);
  canvas_clear(&cv, kPicture);
}

void tearDown(void) {
  xshim_erase_force_effect(-1);
  xshim_set_clock(NULL);
  xshim_close_display(dpy);
  canvas_free(&cv);
  TEST_ASSERT_EQUAL_UINT64_MESSAGE(live_before,
                                   __sanitizer_get_current_allocated_bytes(),
                                   "test leaked memory");
}

static int count_pixels(uint16_t color) {
  int n = 0;
  for (int i = 0; i < kSize * kSize; i++) n += cv.px[i] == color;
  return n;
}

/* Runs a wipe the way a hack drives it: a first call, a dwell, then steps
 * every `interval` seconds. Returns the seconds from the first step to the
 * call that returned NULL. */
static double run_wipe(double interval) {
  eraser_state *st = erase_window(dpy, XSHIM_WINDOW, NULL);
  TEST_ASSERT_NOT_NULL(st);
  now += 5.0;
  st = erase_window(dpy, XSHIM_WINDOW, st);
  const double first_step = now;
  int guard = 0;
  while (st) {
    now += interval;
    st = erase_window(dpy, XSHIM_WINDOW, st);
    TEST_ASSERT_TRUE_MESSAGE(++guard < 10000, "wipe never ended");
  }
  return now - first_step;
}

void test_first_call_draws_nothing_and_returns_a_state(void) {
  eraser_state *st = erase_window(dpy, XSHIM_WINDOW, NULL);
  TEST_ASSERT_NOT_NULL(st);
  TEST_ASSERT_EQUAL_INT(kSize * kSize, count_pixels(kPicture));
  eraser_free(st);
}

void test_dwell_is_not_counted(void) {
  eraser_state *st = erase_window(dpy, XSHIM_WINDOW, NULL);
  now += 5.0;
  eraser_state *again = erase_window(dpy, XSHIM_WINDOW, st);
  TEST_ASSERT_NOT_NULL(again);
  TEST_ASSERT_EQUAL_INT(kSize * kSize, count_pixels(kPicture));
  eraser_free(again);
}

void test_wipe_ends_one_second_after_its_first_step(void) {
  const double intervals[] = {0.010, 0.033};
  for (int i = 0; i < 2; i++) {
    canvas_clear(&cv, kPicture);
    const double took = run_wipe(intervals[i]);
    TEST_ASSERT_TRUE_MESSAGE(took >= 1.0 - 1e-6, "ended early");
    TEST_ASSERT_TRUE_MESSAGE(took <= 1.0 + intervals[i] + 1e-6, "ended late");
    TEST_ASSERT_EQUAL_INT(kSize * kSize, count_pixels(0));
  }
}

void test_a_late_step_finishes_cleanly(void) {
  eraser_state *st = erase_window(dpy, XSHIM_WINDOW, NULL);
  st = erase_window(dpy, XSHIM_WINDOW, st);
  now += 10.0;
  st = erase_window(dpy, XSHIM_WINDOW, st);
  TEST_ASSERT_NULL(st);
  TEST_ASSERT_EQUAL_INT(kSize * kSize, count_pixels(0));
}

void test_uses_the_window_background(void) {
  XSetWindowBackground(dpy, XSHIM_WINDOW, 0x00F8);
  eraser_state *st = erase_window(dpy, XSHIM_WINDOW, NULL);
  st = erase_window(dpy, XSHIM_WINDOW, st);
  now += 0.5;
  st = erase_window(dpy, XSHIM_WINDOW, st);
  TEST_ASSERT_NOT_NULL(st);
  TEST_ASSERT_GREATER_THAN_INT(0, count_pixels(0x00F8));
  TEST_ASSERT_EQUAL_INT(0, count_pixels(0));
  eraser_free(st);

  canvas_clear(&cv, kPicture);
  run_wipe(0.033);
  TEST_ASSERT_EQUAL_INT(kSize * kSize, count_pixels(0x00F8));
}

void test_a_second_wipe_starts_fresh(void) {
  run_wipe(0.033);
  canvas_clear(&cv, kPicture);
  eraser_state *st = erase_window(dpy, XSHIM_WINDOW, NULL);
  TEST_ASSERT_NOT_NULL(st);
  TEST_ASSERT_EQUAL_INT(kSize * kSize, count_pixels(kPicture));
  eraser_free(st);
}

void test_a_step_marks_rows_dirty(void) {
  eraser_state *st = erase_window(dpy, XSHIM_WINDOW, NULL);
  st = erase_window(dpy, XSHIM_WINDOW, st);
  canvas_clear_dirty(&cv);
  now += 0.5;
  st = erase_window(dpy, XSHIM_WINDOW, st);
  TEST_ASSERT_NOT_NULL(st);
  int dirty_rows = 0, x0, x1;
  for (int y = 0; y < kSize; y++) dirty_rows += canvas_dirty_row(&cv, y, &x0, &x1);
  TEST_ASSERT_GREATER_THAN_INT(0, dirty_rows);
  TEST_ASSERT_LESS_THAN_INT(kSize * kSize, count_pixels(kPicture) + 1);
  eraser_free(st);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_first_call_draws_nothing_and_returns_a_state);
  RUN_TEST(test_dwell_is_not_counted);
  RUN_TEST(test_wipe_ends_one_second_after_its_first_step);
  RUN_TEST(test_a_late_step_finishes_cleanly);
  RUN_TEST(test_uses_the_window_background);
  RUN_TEST(test_a_second_wipe_starts_fresh);
  RUN_TEST(test_a_step_marks_rows_dirty);
  return UNITY_END();
}
