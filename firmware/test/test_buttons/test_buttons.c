#include <stdint.h>
#include <unity.h>

#include "runner/button_latch.h"

static ButtonLatch latch;

void setUp(void) { button_latch_init(&latch, 30); }
void tearDown(void) {}

void test_no_press_means_nothing_to_take(void) {
  TEST_ASSERT_FALSE(button_latch_take(&latch, NULL));
}

void test_a_press_is_taken_once(void) {
  button_latch_press(&latch, 1000);
  TEST_ASSERT_TRUE(button_latch_take(&latch, NULL));
  TEST_ASSERT_FALSE(button_latch_take(&latch, NULL));
}

void test_a_press_at_time_zero_is_accepted(void) {
  button_latch_press(&latch, 0);
  TEST_ASSERT_EQUAL_UINT32(1, button_latch_pending(&latch));
}

void test_presses_closer_than_the_debounce_window_count_once(void) {
  button_latch_press(&latch, 100);
  button_latch_press(&latch, 105);
  button_latch_press(&latch, 129);
  TEST_ASSERT_EQUAL_UINT32(1, button_latch_pending(&latch));
}

void test_presses_further_apart_than_the_window_count_separately(void) {
  button_latch_press(&latch, 100);
  button_latch_press(&latch, 140);
  TEST_ASSERT_EQUAL_UINT32(2, button_latch_pending(&latch));
}

void test_taking_collapses_several_pending_presses_into_one_event(void) {
  button_latch_press(&latch, 100);
  button_latch_press(&latch, 500);
  button_latch_press(&latch, 900);
  TEST_ASSERT_TRUE(button_latch_take(&latch, NULL));
  TEST_ASSERT_EQUAL_UINT32(0, button_latch_pending(&latch));
  TEST_ASSERT_FALSE(button_latch_take(&latch, NULL));
}

void test_a_press_after_a_take_is_a_new_event(void) {
  button_latch_press(&latch, 100);
  TEST_ASSERT_TRUE(button_latch_take(&latch, NULL));
  button_latch_press(&latch, 400);
  TEST_ASSERT_TRUE(button_latch_take(&latch, NULL));
}

void test_a_bounce_just_after_a_take_is_still_ignored(void) {
  button_latch_press(&latch, 100);
  TEST_ASSERT_TRUE(button_latch_take(&latch, NULL));
  button_latch_press(&latch, 120);
  TEST_ASSERT_FALSE(button_latch_take(&latch, NULL));
}

void test_the_debounce_window_survives_the_millisecond_counter_wrapping(void) {
  button_latch_press(&latch, 0xFFFFFFF0u);
  button_latch_press(&latch, 0x00000000u);
  TEST_ASSERT_EQUAL_UINT32(1, button_latch_pending(&latch));
  button_latch_press(&latch, 0x00000010u);
  TEST_ASSERT_EQUAL_UINT32(2, button_latch_pending(&latch));
}

void test_take_reports_when_the_first_pending_press_arrived(void) {
  button_latch_press(&latch, 4000);
  button_latch_press(&latch, 4500);
  uint32_t first = 0;
  TEST_ASSERT_TRUE(button_latch_take(&latch, &first));
  TEST_ASSERT_EQUAL_UINT32(4000, first);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_no_press_means_nothing_to_take);
  RUN_TEST(test_a_press_is_taken_once);
  RUN_TEST(test_a_press_at_time_zero_is_accepted);
  RUN_TEST(test_presses_closer_than_the_debounce_window_count_once);
  RUN_TEST(test_presses_further_apart_than_the_window_count_separately);
  RUN_TEST(test_taking_collapses_several_pending_presses_into_one_event);
  RUN_TEST(test_a_press_after_a_take_is_a_new_event);
  RUN_TEST(test_a_bounce_just_after_a_take_is_still_ignored);
  RUN_TEST(test_the_debounce_window_survives_the_millisecond_counter_wrapping);
  RUN_TEST(test_take_reports_when_the_first_pending_press_arrived);
  return UNITY_END();
}
