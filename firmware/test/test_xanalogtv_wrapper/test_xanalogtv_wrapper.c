#include <stdlib.h>
#include <string.h>
#include <sanitizer/allocator_interface.h>
#include <unity.h>

#include "core/canvas.h"
#include "doubletime.h"
#include "runner/hack_entry.h"
#include "x11shim/xshim.h"

/* XAnalogTV is built through hacks/xanalogtv_tv.c; the test links the registered
 * copy, so it sees what the board runs. */
extern const HackEntry xanalogtv_hack;
extern int analogtv_line_step;
extern int analogtv_view_size;
extern long xanalogtv_loads_finished; /* picture loads finished since the program started */

static Canvas cv;
static Display *dpy;
static double g_now;

static double fake_clock(void) { return g_now; }

void setUp(void) {
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&cv, 466, 466, malloc));
  dpy = xshim_open_display(&cv);
  xshim_set_defaults(xanalogtv_hack.defaults);
  g_now = 1000.0;
  xshim_set_clock(fake_clock);
}
void tearDown(void) {
  xshim_set_clock(NULL);
  xshim_set_defaults(NULL);
  xshim_close_display(dpy);
  canvas_free(&cv);
}

static void lit_pixels(int *inside, int *outside) {
  *inside = *outside = 0;
  for (int y = 0; y < cv.h; y++)
    for (int x = 0; x < cv.w; x++) {
      if (!cv.px[y * cv.w + x]) continue;
      if (x >= 68 && x < 398 && y >= 68 && y < 398) ++*inside;
      else ++*outside;
    }
}

/* Upstream powers the set up over its first seconds, by the clock: a picture with
 * the clock stuck at its start is a dot, so the wrapper must hand analogtv the
 * shim's clock. Channel 0 is always the colour bars, and once the set is up the
 * view is full of them. */
void test_the_picture_fills_the_view_once_the_clock_has_run_the_power_up(void) {
  void *st = xanalogtv_hack.init(dpy, XSHIM_WINDOW);
  int inside = 0, outside = 0;
  for (int i = 0; i < 40; i++) {
    g_now += 0.25;
    xanalogtv_hack.draw(dpy, XSHIM_WINDOW, st);
  }
  lit_pixels(&inside, &outside);
  TEST_ASSERT_TRUE_MESSAGE(inside > 20000, "the picture should fill the view");
  xanalogtv_hack.free(dpy, XSHIM_WINDOW, st);
}

void test_nothing_is_drawn_outside_the_view(void) {
  void *st = xanalogtv_hack.init(dpy, XSHIM_WINDOW);
  int inside = 0, outside = 0;
  for (int i = 0; i < 40; i++) {
    g_now += 0.25;
    xanalogtv_hack.draw(dpy, XSHIM_WINDOW, st);
  }
  lit_pixels(&inside, &outside);
  TEST_ASSERT_EQUAL_INT_MESSAGE(0, outside, "the view's edge is the picture's edge");
  xanalogtv_hack.free(dpy, XSHIM_WINDOW, st);
}

/* Every channel's picture is loaded as the callback runs inside the call, and the
 * callback aborts if one arrives while none is wanted. A long run through every
 * channel (a change every few seconds) must survive and keep drawing. */
void test_a_run_through_every_channel_keeps_drawing(void) {
  void *st = xanalogtv_hack.init(dpy, XSHIM_WINDOW);
  int blank_frames = 0;
  for (int i = 0; i < 400; i++) {
    g_now += 0.25;
    xanalogtv_hack.draw(dpy, XSHIM_WINDOW, st);
    if (i > 80) {
      int inside, outside;
      lit_pixels(&inside, &outside);
      blank_frames += inside < 1000;
    }
  }
  TEST_ASSERT_TRUE_MESSAGE(blank_frames < 20, "the screen went dark for too long");
  xanalogtv_hack.free(dpy, XSHIM_WINDOW, st);
}

/* Upstream asks for each channel's picture and marks the channel "not loaded"
 * after asking, so the load must finish later, from a draw: finished inside the
 * call, it would fill another channel and leave this one blank (static). Every
 * load asked for finishes, one a draw, and then they stop. */
void test_picture_loads_finish_from_draws_one_at_a_time_and_then_stop(void) {
  long total = 0;
  for (int seed = 1; seed <= 4; seed++) {
    srandom((unsigned)seed);
    const long before = xanalogtv_loads_finished;
    void *st = xanalogtv_hack.init(dpy, XSHIM_WINDOW);
    TEST_ASSERT_EQUAL_INT_MESSAGE(before, xanalogtv_loads_finished,
                                  "a load finished inside the request");
    for (int i = 0; i < 12; i++) {
      const long was = xanalogtv_loads_finished;
      g_now += 0.25;
      xanalogtv_hack.draw(dpy, XSHIM_WINDOW, st);
      TEST_ASSERT_TRUE_MESSAGE(xanalogtv_loads_finished - was <= 1, "two loads finished in one draw");
    }
    const long after_twelve = xanalogtv_loads_finished;
    total += after_twelve - before;
    for (int i = 0; i < 12; i++) {
      g_now += 0.25;
      xanalogtv_hack.draw(dpy, XSHIM_WINDOW, st);
    }
    TEST_ASSERT_EQUAL_INT_MESSAGE(after_twelve, xanalogtv_loads_finished, "loads kept coming");
    xanalogtv_hack.free(dpy, XSHIM_WINDOW, st);
  }
  TEST_ASSERT_TRUE_MESSAGE(total >= 4, "no picture was ever loaded");
}

/* analogtv reads these two, and the last hack to start leaves its values behind. */
void test_init_sets_the_shared_line_step_and_view(void) {
  analogtv_line_step = 1;
  analogtv_view_size = 0;
  void *st = xanalogtv_hack.init(dpy, XSHIM_WINDOW);
  TEST_ASSERT_EQUAL_INT(3, analogtv_line_step);
  TEST_ASSERT_EQUAL_INT(330, analogtv_view_size);
  xanalogtv_hack.free(dpy, XSHIM_WINDOW, st);
}

/* A hack freed before its first draw still holds the pixmap its first picture
 * was to load into (about 500 KB), and upstream's free knows nothing of it. */
void test_a_hack_stopped_before_its_first_draw_gives_back_its_picture_pixmap(void) {
  for (int i = 0; i < 2; i++) {
    void *st = xanalogtv_hack.init(dpy, XSHIM_WINDOW);
    xanalogtv_hack.free(dpy, XSHIM_WINDOW, st);
  }
  const size_t before = __sanitizer_get_current_allocated_bytes();
  for (int i = 0; i < 5; i++) {
    void *st = xanalogtv_hack.init(dpy, XSHIM_WINDOW);
    xanalogtv_hack.free(dpy, XSHIM_WINDOW, st);
  }
  const size_t after = __sanitizer_get_current_allocated_bytes();
  TEST_ASSERT_TRUE_MESSAGE(after < before + 4 * 1024, "a pixmap was left behind");
}

/* The three test cards are only read while the channels are set up, but upstream
 * keeps them (as 32-bit images, 480 KB each at the size we ship) until it is
 * freed: on the board that left too little PSRAM for the first picture load. */
void test_the_test_cards_are_not_kept_once_the_channels_are_set_up(void) {
  const size_t before = __sanitizer_get_current_allocated_bytes();
  void *st = xanalogtv_hack.init(dpy, XSHIM_WINDOW);
  const size_t held = __sanitizer_get_current_allocated_bytes() - before;
  xanalogtv_hack.free(dpy, XSHIM_WINDOW, st);
  /* 5.0 MB with the cards kept, 3.6 MB without. */
  TEST_ASSERT_TRUE_MESSAGE(held < 4500000, "the test cards are still held");
}

void test_the_entry_overlaps_its_push_only_with_the_whole_window_view(void) {
  TEST_ASSERT_EQUAL_INT(0, xanalogtv_hack.repaints_all); /* the default view is smaller than the window */
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_the_picture_fills_the_view_once_the_clock_has_run_the_power_up);
  RUN_TEST(test_nothing_is_drawn_outside_the_view);
  RUN_TEST(test_a_run_through_every_channel_keeps_drawing);
  RUN_TEST(test_picture_loads_finish_from_draws_one_at_a_time_and_then_stop);
  RUN_TEST(test_init_sets_the_shared_line_step_and_view);
  RUN_TEST(test_a_hack_stopped_before_its_first_draw_gives_back_its_picture_pixmap);
  RUN_TEST(test_the_test_cards_are_not_kept_once_the_channels_are_set_up);
  RUN_TEST(test_the_entry_overlaps_its_push_only_with_the_whole_window_view);
  return UNITY_END();
}
