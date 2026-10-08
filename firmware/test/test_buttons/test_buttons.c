#include <stdint.h>
#include <unity.h>

#include "runner/button_latch.h"

static ButtonLatch latch;

static void press(uint32_t ms) { button_latch_edge(&latch, true, ms); }
static void release(uint32_t ms) { button_latch_edge(&latch, false, ms); }

void setUp(void) { button_latch_init(&latch, 30); }
void tearDown(void) {}

void test_no_edge_means_nothing_to_take(void) {
  TEST_ASSERT_FALSE(button_latch_take(&latch, NULL));
}

void test_a_press_is_taken_once(void) {
  press(1000);
  TEST_ASSERT_TRUE(button_latch_take(&latch, NULL));
  TEST_ASSERT_FALSE(button_latch_take(&latch, NULL));
}

void test_a_press_at_time_zero_is_accepted(void) {
  press(0);
  TEST_ASSERT_EQUAL_UINT32(1, button_latch_pending(&latch));
}

void test_a_release_on_its_own_is_not_a_press(void) {
  release(1000);
  TEST_ASSERT_EQUAL_UINT32(0, button_latch_pending(&latch));
}

void test_a_click_is_one_press_even_though_the_release_comes_later(void) {
  press(100);
  release(180);
  TEST_ASSERT_EQUAL_UINT32(1, button_latch_pending(&latch));
}

void test_a_bounce_on_release_is_not_a_second_press(void) {
  press(100);
  TEST_ASSERT_TRUE(button_latch_take(&latch, NULL));
  release(250);
  press(252);
  release(253);
  TEST_ASSERT_FALSE(button_latch_take(&latch, NULL));
}

void test_a_bounce_on_press_counts_once(void) {
  press(100);
  release(101);
  press(102);
  press(104);
  TEST_ASSERT_EQUAL_UINT32(1, button_latch_pending(&latch));
}

void test_a_second_real_click_is_a_second_press(void) {
  press(100);
  release(150);
  press(200);
  release(240);
  TEST_ASSERT_EQUAL_UINT32(2, button_latch_pending(&latch));
}

void test_a_press_must_follow_a_quiet_gap_after_the_last_edge(void) {
  press(100);
  release(150);
  press(179);
  TEST_ASSERT_EQUAL_UINT32(1, button_latch_pending(&latch));
  release(190);
  press(230);
  TEST_ASSERT_EQUAL_UINT32(2, button_latch_pending(&latch));
}

void test_taking_collapses_several_pending_presses_into_one_event(void) {
  press(100);
  release(150);
  press(500);
  release(550);
  press(900);
  TEST_ASSERT_TRUE(button_latch_take(&latch, NULL));
  TEST_ASSERT_EQUAL_UINT32(0, button_latch_pending(&latch));
  TEST_ASSERT_FALSE(button_latch_take(&latch, NULL));
}

void test_a_press_after_a_take_is_a_new_event(void) {
  press(100);
  release(160);
  TEST_ASSERT_TRUE(button_latch_take(&latch, NULL));
  press(400);
  TEST_ASSERT_TRUE(button_latch_take(&latch, NULL));
}

void test_the_quiet_gap_survives_the_millisecond_counter_wrapping(void) {
  press(0xFFFFFFF0u);
  release(0x00000000u);
  press(0x0000000Au);
  TEST_ASSERT_EQUAL_UINT32(1, button_latch_pending(&latch));
  release(0x0000000Cu);
  press(0x00000040u);
  TEST_ASSERT_EQUAL_UINT32(2, button_latch_pending(&latch));
}

void test_take_reports_when_the_first_pending_press_arrived(void) {
  press(4000);
  release(4050);
  press(4500);
  uint32_t first = 0;
  TEST_ASSERT_TRUE(button_latch_take(&latch, &first));
  TEST_ASSERT_EQUAL_UINT32(4000, first);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_no_edge_means_nothing_to_take);
  RUN_TEST(test_a_press_is_taken_once);
  RUN_TEST(test_a_press_at_time_zero_is_accepted);
  RUN_TEST(test_a_release_on_its_own_is_not_a_press);
  RUN_TEST(test_a_click_is_one_press_even_though_the_release_comes_later);
  RUN_TEST(test_a_bounce_on_release_is_not_a_second_press);
  RUN_TEST(test_a_bounce_on_press_counts_once);
  RUN_TEST(test_a_second_real_click_is_a_second_press);
  RUN_TEST(test_a_press_must_follow_a_quiet_gap_after_the_last_edge);
  RUN_TEST(test_taking_collapses_several_pending_presses_into_one_event);
  RUN_TEST(test_a_press_after_a_take_is_a_new_event);
  RUN_TEST(test_the_quiet_gap_survives_the_millisecond_counter_wrapping);
  RUN_TEST(test_take_reports_when_the_first_pending_press_arrived);
  return UNITY_END();
}
