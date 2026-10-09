#include <string.h>
#include <unity.h>

#include "runner/overlay.h"

#define NAME_MS 5000

static Overlay ov;

static void show(OverlayInfo mode) {
  while (ov.info != mode) overlay_cycle_info(&ov);
}

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

static void expect_only(bool badge, bool fps, bool battery) {
  TEST_ASSERT_EQUAL(badge, overlay_badge_visible(&ov));
  TEST_ASSERT_EQUAL(fps, overlay_fps_visible(&ov));
  TEST_ASSERT_EQUAL(battery, overlay_battery_visible(&ov));
}

void test_info_starts_as_nothing_and_taps_cycle_badge_fps_battery_nothing(void) {
  expect_only(false, false, false);
  overlay_cycle_info(&ov);
  expect_only(true, false, false);
  overlay_cycle_info(&ov);
  expect_only(false, true, false);
  overlay_cycle_info(&ov);
  expect_only(false, false, true);
  overlay_cycle_info(&ov);
  expect_only(false, false, false);
}

void test_the_chosen_readout_stays_when_a_new_hack_starts(void) {
  OverlayInfo modes[] = {INFO_NAME, INFO_FPS, INFO_BATTERY};
  for (int i = 0; i < 3; i++) {
    show(modes[i]);
    overlay_hack_started(&ov, 100);
    TEST_ASSERT_EQUAL(modes[i], ov.info);
  }
}

void test_the_hack_name_is_hidden_only_in_badge_mode(void) {
  overlay_hack_started(&ov, 0);
  TEST_ASSERT_TRUE(overlay_name_visible(&ov, 100));
  show(INFO_NAME);
  TEST_ASSERT_FALSE(overlay_name_visible(&ov, 100));
  show(INFO_FPS);
  TEST_ASSERT_TRUE(overlay_name_visible(&ov, 100));
  show(INFO_BATTERY);
  TEST_ASSERT_TRUE(overlay_name_visible(&ov, 100));
}

void test_the_hack_name_returns_when_leaving_badge_mode_within_its_time(void) {
  overlay_hack_started(&ov, 0);
  show(INFO_NAME);
  show(INFO_FPS);
  TEST_ASSERT_TRUE(overlay_name_visible(&ov, 4999));
  TEST_ASSERT_FALSE(overlay_name_visible(&ov, 5000));
}

void test_battery_text_is_a_whole_percentage(void) {
  char buf[16];
  overlay_set_battery(&ov, 91);
  overlay_battery_text(&ov, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("91%", buf);
  overlay_set_battery(&ov, 0);
  overlay_battery_text(&ov, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("0%", buf);
}

void test_battery_text_is_a_placeholder_until_a_level_is_known(void) {
  char buf[16];
  overlay_battery_text(&ov, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("--%", buf);
}

void test_battery_level_is_clamped_to_0_to_100(void) {
  char buf[16];
  overlay_set_battery(&ov, 130);
  overlay_battery_text(&ov, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("100%", buf);
}

void test_a_negative_reading_before_any_good_one_stays_unknown(void) {
  char buf[16];
  overlay_set_battery(&ov, -5);
  overlay_battery_text(&ov, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("--%", buf);
}

void test_a_failed_reading_keeps_the_last_good_level(void) {
  char buf[16];
  overlay_set_battery(&ov, 64);
  overlay_set_battery(&ov, -1);
  overlay_battery_text(&ov, buf, sizeof buf);
  TEST_ASSERT_EQUAL_STRING("64%", buf);
}

void test_battery_text_never_overruns_a_small_buffer(void) {
  char buf[3];
  overlay_set_battery(&ov, 100);
  overlay_battery_text(&ov, buf, sizeof buf);
  TEST_ASSERT_EQUAL_UINT(2, strlen(buf));
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

void test_a_redraw_is_wanted_whenever_the_info_mode_changes(void) {
  for (int i = 0; i < 4; i++) {
    overlay_drawn(&ov, 0);
    overlay_cycle_info(&ov);
    TEST_ASSERT_TRUE(overlay_wants_redraw(&ov, 0));
  }
}

void test_a_redraw_is_wanted_when_the_shown_battery_level_changes(void) {
  show(INFO_BATTERY);
  overlay_set_battery(&ov, 90);
  overlay_drawn(&ov, 0);
  overlay_set_battery(&ov, 90);
  TEST_ASSERT_FALSE(overlay_wants_redraw(&ov, 0));
  overlay_set_battery(&ov, 89);
  TEST_ASSERT_TRUE(overlay_wants_redraw(&ov, 0));
}

void test_a_battery_change_needs_no_redraw_while_it_is_not_shown(void) {
  overlay_set_battery(&ov, 90);
  overlay_drawn(&ov, 0);
  overlay_set_battery(&ov, 89);
  TEST_ASSERT_FALSE(overlay_wants_redraw(&ov, 0));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_nothing_is_shown_before_a_hack_starts);
  RUN_TEST(test_the_name_shows_for_exactly_the_name_time_after_a_start);
  RUN_TEST(test_switching_hacks_restarts_the_name_time);
  RUN_TEST(test_the_name_time_survives_the_millisecond_counter_wrapping);
  RUN_TEST(test_info_starts_as_nothing_and_taps_cycle_badge_fps_battery_nothing);
  RUN_TEST(test_the_chosen_readout_stays_when_a_new_hack_starts);
  RUN_TEST(test_the_hack_name_is_hidden_only_in_badge_mode);
  RUN_TEST(test_the_hack_name_returns_when_leaving_badge_mode_within_its_time);
  RUN_TEST(test_battery_text_is_a_whole_percentage);
  RUN_TEST(test_battery_text_is_a_placeholder_until_a_level_is_known);
  RUN_TEST(test_battery_level_is_clamped_to_0_to_100);
  RUN_TEST(test_a_negative_reading_before_any_good_one_stays_unknown);
  RUN_TEST(test_a_failed_reading_keeps_the_last_good_level);
  RUN_TEST(test_battery_text_never_overruns_a_small_buffer);
  RUN_TEST(test_fps_text_is_a_placeholder_until_a_second_of_frames);
  RUN_TEST(test_fps_text_reports_frames_per_second_over_a_second);
  RUN_TEST(test_fps_text_keeps_one_decimal_for_slow_hacks);
  RUN_TEST(test_a_new_hack_forgets_the_previous_hacks_fps);
  RUN_TEST(test_fps_text_never_overruns_a_small_buffer);
  RUN_TEST(test_a_redraw_is_wanted_when_the_name_expires);
  RUN_TEST(test_a_redraw_is_wanted_whenever_the_info_mode_changes);
  RUN_TEST(test_a_redraw_is_wanted_when_the_shown_battery_level_changes);
  RUN_TEST(test_a_battery_change_needs_no_redraw_while_it_is_not_shown);
  return UNITY_END();
}
