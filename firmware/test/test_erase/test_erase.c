#include <sanitizer/allocator_interface.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <unity.h>

#include "core/canvas.h"
#include "doubletime.h"
#include "erase.h"
#include "runner/hack_runner.h"
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
  xshim_erase_set_gc_factory(NULL);
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

enum { kEffects = 4 };

/* Starts a wipe and takes its first step, so the clock starts at *start. */
static eraser_state *begin_wipe(int effect, double *start) {
  srandom(1);
  xshim_erase_force_effect(effect);
  eraser_state *st = erase_window(dpy, XSHIM_WINDOW, NULL);
  st = erase_window(dpy, XSHIM_WINDOW, st);
  *start = now;
  return st;
}

static eraser_state *step_to(eraser_state *st, double start, double ratio) {
  now = start + ratio;
  return erase_window(dpy, XSHIM_WINDOW, st);
}

static double cleared_fraction(uint16_t background) {
  return (double)count_pixels(background) / (kSize * kSize);
}

void test_effect_count_is_four(void) {
  TEST_ASSERT_EQUAL_INT(kEffects, xshim_erase_effect_count());
}

void test_cleared_area_grows_and_ends_complete(void) {
  for (int e = 0; e < kEffects; e++) {
    canvas_clear(&cv, kPicture);
    double start;
    eraser_state *st = begin_wipe(e, &start);
    double before = 0.0;
    const double ratios[] = {0.25, 0.5, 0.75};
    for (int i = 0; i < 3; i++) {
      st = step_to(st, start, ratios[i]);
      TEST_ASSERT_NOT_NULL_MESSAGE(st, "wipe ended early");
      const double now_cleared = cleared_fraction(0);
      TEST_ASSERT_TRUE_MESSAGE(now_cleared > before, "cleared area did not grow");
      TEST_ASSERT_TRUE_MESSAGE(now_cleared < 1.0, "wipe finished before its end");
      before = now_cleared;
    }
    TEST_ASSERT_TRUE_MESSAGE(before >= 0.5, "wipe is too slow to clear");
    st = step_to(st, start, 1.0);
    TEST_ASSERT_NULL(st);
    TEST_ASSERT_EQUAL_INT(kSize * kSize, count_pixels(0));
  }
}

void test_steps_stay_inside_the_canvas_and_are_quick(void) {
  for (int e = 0; e < kEffects; e++) {
    canvas_clear(&cv, kPicture);
    double start;
    eraser_state *st = begin_wipe(e, &start);
    st = step_to(st, start, 0.5);
    canvas_clear_dirty(&cv);

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    st = step_to(st, start, 0.533);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    const double ms = (t1.tv_sec - t0.tv_sec) * 1e3 + (t1.tv_nsec - t0.tv_nsec) / 1e6;
    TEST_ASSERT_TRUE_MESSAGE(ms < 5.0, "a 33 ms step took 5 ms or more on the host");

    int x0, x1;
    for (int y = 0; y < kSize; y++)
      if (canvas_dirty_row(&cv, y, &x0, &x1)) {
        TEST_ASSERT_TRUE(x0 >= 0 && x1 <= kSize - 1 && x0 <= x1);
      }
    eraser_free(st);
  }
}

void test_effects_use_the_window_background(void) {
  XSetWindowBackground(dpy, XSHIM_WINDOW, 0x00F8);
  for (int e = 0; e < kEffects; e++) {
    canvas_clear(&cv, kPicture);
    double start;
    eraser_state *st = begin_wipe(e, &start);
    st = step_to(st, start, 0.5);
    TEST_ASSERT_GREATER_THAN_INT(0, count_pixels(0x00F8));
    TEST_ASSERT_EQUAL_INT(0, count_pixels(0));
    eraser_free(st);
  }
}

static uint64_t canvas_hash(void) {
  uint64_t h = 0xcbf29ce484222325ull;
  for (int i = 0; i < kSize * kSize; i++) h = (h ^ cv.px[i]) * 0x100000001b3ull;
  return h;
}

void test_the_effects_look_different(void) {
  uint64_t seen[kEffects];
  for (int e = 0; e < kEffects; e++) {
    canvas_clear(&cv, kPicture);
    double start;
    eraser_state *st = begin_wipe(e, &start);
    st = step_to(st, start, 0.5);
    seen[e] = canvas_hash();
    eraser_free(st);
    for (int p = 0; p < e; p++) TEST_ASSERT_NOT_EQUAL_UINT64(seen[p], seen[e]);
  }
}

/* A hack in the style of Coral and Maze: it keeps its eraser in its closure
 * and never calls eraser_free. */
static void *leaky_init(Display *d, Window w) {
  (void)d, (void)w;
  return calloc(1, sizeof(eraser_state *));
}
static unsigned long leaky_draw(Display *d, Window w, void *closure) {
  eraser_state **st = (eraser_state **)closure;
  *st = erase_window(d, w, *st);
  return 1000;
}
static void leaky_free(Display *d, Window w, void *closure) {
  (void)d, (void)w;
  free(closure);
}

void test_stopping_mid_wipe_frees_a_state_the_hack_never_frees(void) {
  static const HackEntry entry = {"Leaky", NULL,       leaky_init, leaky_draw,
                                  leaky_free, NULL};
  const HackEntry *const hacks[] = {&entry};
  HackRunner *r = runner_create_with(&cv, hacks, 1);
  TEST_ASSERT_NOT_NULL(r);
  size_t after_warmup = 0;
  for (int i = 0; i < 55; i++) {
    TEST_ASSERT_EQUAL_INT(0, runner_start(r, 0));
    runner_step(r);
    runner_step(r);
    if (i == 4) after_warmup = __sanitizer_get_current_allocated_bytes();
  }
  TEST_ASSERT_EQUAL_UINT64(after_warmup,
                           __sanitizer_get_current_allocated_bytes());
  runner_destroy(r);
}

void test_eraser_free_then_release_does_not_double_free(void) {
  eraser_state *st = erase_window(dpy, XSHIM_WINDOW, NULL);
  eraser_free(st);
  xshim_release_erasers(dpy);
}

void test_release_frees_every_live_state_on_the_display(void) {
  erase_window(dpy, XSHIM_WINDOW, NULL);
  erase_window(dpy, XSHIM_WINDOW, NULL);
  xshim_release_erasers(dpy);
}

static GC no_gc(Display *d, Drawable w, unsigned long mask, XGCValues *v) {
  (void)d, (void)w, (void)mask, (void)v;
  return NULL;
}

void test_a_failed_gc_allocation_clears_at_once_and_leaves_no_state(void) {
  xshim_erase_set_gc_factory(no_gc);
  eraser_state *st = erase_window(dpy, XSHIM_WINDOW, NULL);
  xshim_erase_set_gc_factory(NULL);
  TEST_ASSERT_NULL(st);
  TEST_ASSERT_EQUAL_INT(kSize * kSize, count_pixels(0));
  TEST_ASSERT_NULL(dpy->erasers);
}

void test_the_circle_reaches_the_top_and_bottom_rows(void) {
  double start;
  eraser_state *st = begin_wipe(1, &start);
  st = step_to(st, start, 0.99);
  TEST_ASSERT_NOT_NULL(st);
  TEST_ASSERT_EQUAL_HEX16(0, cv.px[kSize / 2]);
  TEST_ASSERT_EQUAL_HEX16(0, cv.px[(kSize - 1) * kSize + kSize / 2]);
  eraser_free(st);
}

/* tearDown closes the display and checks for leaks. */
void test_closing_the_display_frees_a_state_still_live(void) {
  TEST_ASSERT_NOT_NULL(erase_window(dpy, XSHIM_WINDOW, NULL));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_a_failed_gc_allocation_clears_at_once_and_leaves_no_state);
  RUN_TEST(test_the_circle_reaches_the_top_and_bottom_rows);
  RUN_TEST(test_effect_count_is_four);
  RUN_TEST(test_cleared_area_grows_and_ends_complete);
  RUN_TEST(test_steps_stay_inside_the_canvas_and_are_quick);
  RUN_TEST(test_effects_use_the_window_background);
  RUN_TEST(test_the_effects_look_different);
  RUN_TEST(test_closing_the_display_frees_a_state_still_live);
  RUN_TEST(test_stopping_mid_wipe_frees_a_state_the_hack_never_frees);
  RUN_TEST(test_eraser_free_then_release_does_not_double_free);
  RUN_TEST(test_release_frees_every_live_state_on_the_display);
  RUN_TEST(test_first_call_draws_nothing_and_returns_a_state);
  RUN_TEST(test_dwell_is_not_counted);
  RUN_TEST(test_wipe_ends_one_second_after_its_first_step);
  RUN_TEST(test_a_late_step_finishes_cleanly);
  RUN_TEST(test_uses_the_window_background);
  RUN_TEST(test_a_second_wipe_starts_fresh);
  RUN_TEST(test_a_step_marks_rows_dirty);
  return UNITY_END();
}
