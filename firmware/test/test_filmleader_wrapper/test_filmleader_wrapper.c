#include <stdlib.h>
#include <string.h>
#include <sanitizer/allocator_interface.h>
#include <unity.h>

#include "core/canvas.h"
#include "doubletime.h"
#include "runner/hack_entry.h"
#include "x11shim/xshim.h"

/* FilmLeader is built through hacks/filmleader_tv.c; the test links the registered
 * copy, so it sees what the board runs. */
extern const HackEntry filmleader_hack;
extern int analogtv_line_step;
extern int analogtv_view_size;

static Canvas cv;
static Display *dpy;
static double g_now;

static double fake_clock(void) { return g_now; }

void setUp(void) {
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&cv, 466, 466, malloc));
  dpy = xshim_open_display(&cv);
  xshim_set_defaults(filmleader_hack.defaults);
  g_now = 1000.0;
  xshim_set_clock(fake_clock);
}
void tearDown(void) {
  xshim_set_clock(NULL);
  xshim_set_defaults(NULL);
  xshim_close_display(dpy);
  canvas_free(&cv);
}

static void run_frames(void *st, int frames, double seconds_each) {
  for (int i = 0; i < frames; i++) {
    g_now += seconds_each;
    filmleader_hack.draw(dpy, XSHIM_WINDOW, st);
  }
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

/* The leader's pale blue background fills the view once the set has powered up
 * (upstream leaves 18 seconds on the clock for it). */
void test_the_leader_fills_a_centred_square_view_and_nothing_else(void) {
  void *st = filmleader_hack.init(dpy, XSHIM_WINDOW);
  run_frames(st, 12, 0.5);
  int inside, outside;
  lit_pixels(&inside, &outside);
  TEST_ASSERT_TRUE_MESSAGE(inside > 60000, "the leader should fill the view");
  TEST_ASSERT_EQUAL_INT_MESSAGE(0, outside, "nothing may be drawn outside the view");
  filmleader_hack.free(dpy, XSHIM_WINDOW, st);
}

/* analogtv reads these two, and the last hack to start leaves its values behind. */
void test_init_sets_the_shared_line_step_and_view(void) {
  analogtv_line_step = 1;
  analogtv_view_size = 0;
  void *st = filmleader_hack.init(dpy, XSHIM_WINDOW);
  TEST_ASSERT_EQUAL_INT(3, analogtv_line_step);
  TEST_ASSERT_EQUAL_INT(330, analogtv_view_size);
  filmleader_hack.free(dpy, XSHIM_WINDOW, st);
}

/* Upstream's font sizes are for a canvas 400 high; ours is 356, so the number is
 * asked for at 140, not 170 (a size the font would not hold anyway). */
void test_the_font_sizes_are_scaled_to_the_canvas(void) {
  int number = 0;
  for (const char *const *o = filmleader_hack.overrides; *o; o++)
    if (!strncmp(*o, "*numberFont:", 12)) number = atoi(strrchr(*o, ' '));
  TEST_ASSERT_EQUAL_INT(140, number);
}

/* Upstream's free forgets the three fonts it opened. */
void test_a_start_and_stop_gives_back_everything_it_took(void) {
  for (int i = 0; i < 2; i++) {
    void *st = filmleader_hack.init(dpy, XSHIM_WINDOW);
    run_frames(st, 2, 0.5);
    filmleader_hack.free(dpy, XSHIM_WINDOW, st);
  }
  const size_t before = __sanitizer_get_current_allocated_bytes();
  for (int i = 0; i < 5; i++) {
    void *st = filmleader_hack.init(dpy, XSHIM_WINDOW);
    run_frames(st, 2, 0.5);
    filmleader_hack.free(dpy, XSHIM_WINDOW, st);
  }
  const size_t after = __sanitizer_get_current_allocated_bytes();
  TEST_ASSERT_EQUAL_UINT64_MESSAGE(before, after, "a start left memory behind");
}

void test_the_entry_overlaps_its_push_only_with_the_whole_window_view(void) {
  TEST_ASSERT_EQUAL_INT(0, filmleader_hack.repaints_all); /* the default view is smaller than the window */
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_the_leader_fills_a_centred_square_view_and_nothing_else);
  RUN_TEST(test_init_sets_the_shared_line_step_and_view);
  RUN_TEST(test_the_font_sizes_are_scaled_to_the_canvas);
  RUN_TEST(test_a_start_and_stop_gives_back_everything_it_took);
  RUN_TEST(test_the_entry_overlaps_its_push_only_with_the_whole_window_view);
  return UNITY_END();
}
