#include <string.h>
#include <unity.h>

#include "runner/overlay.h"

#define NAME_MS 5000

static Overlay ov;

void setUp(void) { overlay_init(&ov, NAME_MS); }
void tearDown(void) {}

void test_nothing_is_shown_before_a_hack_starts(void) {
  TEST_ASSERT_FALSE(overlay_name_visible(&ov, 0));
  TEST_ASSERT_FALSE(overlay_fps_visible(&ov));
}

void test_the_name_shows_for_exactly_the_name_time_after_a_start(void) {
  overlay_hack_started(&ov, 1000);
  TEST_ASSERT_TRUE(overlay_name_visible(&ov, 1000));
  TEST_ASSERT_TRUE(overlay_name_visible(&ov, 5999));
  TEST_ASSERT_FALSE(overlay_name_visible(&ov, 6000));
}

void test_switching_hacks_restarts_the_name_time(void) {
  overlay_hack_started(&ov, 0);
  overlay_hack_started(&ov, 4000);
  TEST_ASSERT_TRUE(overlay_name_visible(&ov, 8999));
  TEST_ASSERT_FALSE(overlay_name_visible(&ov, 9000));
}

void test_the_name_time_survives_the_millisecond_counter_wrapping(void) {
  overlay_hack_started(&ov, 0xFFFFFFFFu - 1000);
  TEST_ASSERT_TRUE(overlay_name_visible(&ov, 2000));
  TEST_ASSERT_FALSE(overlay_name_visible(&ov, 4000));
}

void test_fps_starts_off_and_a_tap_toggles_it(void) {
  TEST_ASSERT_FALSE(overlay_fps_visible(&ov));
  overlay_toggle_fps(&ov);
  TEST_ASSERT_TRUE(overlay_fps_visible(&ov));
  overlay_toggle_fps(&ov);
  TEST_ASSERT_FALSE(overlay_fps_visible(&ov));
}

void test_fps_stays_on_when_a_new_hack_starts(void) {
  overlay_toggle_fps(&ov);
  overlay_hack_started(&ov, 100);
  TEST_ASSERT_TRUE(overlay_fps_visible(&ov));
}

void test_fps_text_is_a_placeholder_until_a_second_of_frames(void) {
  char buf[16];
  overlay_hack_started(&ov, 0);
  for (uint32_t t = 0; t < 900; t += 100) overlay_frame(&ov, t);
  overlay_fps_text(&ov, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("-- fps", buf);
}

void test_fps_text_reports_frames_per_second_over_a_second(void) {
  char buf[16];
  overlay_hack_started(&ov, 0);
  for (uint32_t t = 100; t <= 1000; t += 100) overlay_frame(&ov, t);
  overlay_fps_text(&ov, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("10.0 fps", buf);
}

void test_fps_text_keeps_one_decimal_for_slow_hacks(void) {
  char buf[16];
  overlay_hack_started(&ov, 0);
  overlay_frame(&ov, 500);
  overlay_frame(&ov, 1500);
  overlay_fps_text(&ov, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("1.3 fps", buf);
}

void test_a_new_hack_forgets_the_previous_hacks_fps(void) {
  char buf[16];
  overlay_hack_started(&ov, 0);
  for (uint32_t t = 100; t <= 1000; t += 100) overlay_frame(&ov, t);
  overlay_hack_started(&ov, 1100);
  overlay_fps_text(&ov, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("-- fps", buf);
}

void test_fps_text_never_overruns_a_small_buffer(void) {
  char buf[4];
  overlay_hack_started(&ov, 0);
  overlay_frame(&ov, 1000);
  overlay_fps_text(&ov, buf, sizeof buf);
  TEST_ASSERT_EQUAL_UINT(3, strlen(buf));
}

void test_a_redraw_is_wanted_when_the_name_expires(void) {
  overlay_hack_started(&ov, 0);
  overlay_drawn(&ov, 100);
  TEST_ASSERT_FALSE(overlay_wants_redraw(&ov, 4999));
  TEST_ASSERT_TRUE(overlay_wants_redraw(&ov, 5000));
  overlay_drawn(&ov, 5000);
  TEST_ASSERT_FALSE(overlay_wants_redraw(&ov, 9000));
}

void test_a_redraw_is_wanted_when_fps_is_toggled_either_way(void) {
  overlay_drawn(&ov, 0);
  overlay_toggle_fps(&ov);
  TEST_ASSERT_TRUE(overlay_wants_redraw(&ov, 0));
  overlay_drawn(&ov, 0);
  overlay_toggle_fps(&ov);
  TEST_ASSERT_TRUE(overlay_wants_redraw(&ov, 0));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_nothing_is_shown_before_a_hack_starts);
  RUN_TEST(test_the_name_shows_for_exactly_the_name_time_after_a_start);
  RUN_TEST(test_switching_hacks_restarts_the_name_time);
  RUN_TEST(test_the_name_time_survives_the_millisecond_counter_wrapping);
  RUN_TEST(test_fps_starts_off_and_a_tap_toggles_it);
  RUN_TEST(test_fps_stays_on_when_a_new_hack_starts);
  RUN_TEST(test_fps_text_is_a_placeholder_until_a_second_of_frames);
  RUN_TEST(test_fps_text_reports_frames_per_second_over_a_second);
  RUN_TEST(test_fps_text_keeps_one_decimal_for_slow_hacks);
  RUN_TEST(test_a_new_hack_forgets_the_previous_hacks_fps);
  RUN_TEST(test_fps_text_never_overruns_a_small_buffer);
  RUN_TEST(test_a_redraw_is_wanted_when_the_name_expires);
  RUN_TEST(test_a_redraw_is_wanted_when_fps_is_toggled_either_way);
  return UNITY_END();
}
