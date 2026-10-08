#include <pthread.h>
#include <stdint.h>
#include <unity.h>

#include "runner/button_latch.h"

#define POLL_MS 5

static ButtonLatch latch;

/* Samples the button every POLL_MS from `from` up to (not including) `to`. */
static void hold(bool down, uint32_t from, uint32_t to) {
  for (uint32_t t = from; (uint32_t)(to - t) != 0 && (uint32_t)(to - t) < 0x80000000u;
       t += POLL_MS)
    button_latch_sample(&latch, down, t);
}

void setUp(void) {
  button_latch_init(&latch, 30);
  button_latch_sample(&latch, false, 0);
}
void tearDown(void) {}

void test_a_button_left_alone_gives_nothing_to_take(void) {
  hold(false, 5, 1000);
  TEST_ASSERT_FALSE(button_latch_take(&latch, NULL));
}

void test_a_click_is_one_press_taken_once(void) {
  hold(false, 5, 100);
  hold(true, 100, 250);
  hold(false, 250, 400);
  TEST_ASSERT_EQUAL_UINT32(1, button_latch_pending(&latch));
  TEST_ASSERT_TRUE(button_latch_take(&latch, NULL));
  TEST_ASSERT_FALSE(button_latch_take(&latch, NULL));
}

void test_a_press_only_counts_once_it_has_settled(void) {
  hold(true, 100, 125);
  TEST_ASSERT_EQUAL_UINT32(0, button_latch_pending(&latch));
  hold(true, 125, 135);
  TEST_ASSERT_EQUAL_UINT32(1, button_latch_pending(&latch));
}

void test_a_long_press_is_still_one_press(void) {
  hold(true, 100, 5000);
  TEST_ASSERT_EQUAL_UINT32(1, button_latch_pending(&latch));
}

void test_a_glitch_shorter_than_the_settle_time_is_ignored(void) {
  hold(true, 100, 120);
  hold(false, 120, 400);
  TEST_ASSERT_EQUAL_UINT32(0, button_latch_pending(&latch));
}

void test_an_old_glitch_does_not_shorten_the_settle_time_of_a_later_press(void) {
  hold(true, 100, 120);
  hold(false, 120, 400);
  hold(true, 400, 425);
  TEST_ASSERT_EQUAL_UINT32(0, button_latch_pending(&latch));
  hold(true, 425, 435);
  uint32_t began = 0;
  TEST_ASSERT_TRUE(button_latch_take(&latch, &began));
  TEST_ASSERT_EQUAL_UINT32(400, began);
}

void test_bounce_on_press_is_one_press(void) {
  hold(true, 100, 105);
  hold(false, 105, 110);
  hold(true, 110, 115);
  hold(false, 115, 120);
  hold(true, 120, 300);
  hold(false, 300, 400);
  TEST_ASSERT_EQUAL_UINT32(1, button_latch_pending(&latch));
}

void test_bounce_on_release_is_not_a_second_press(void) {
  hold(true, 100, 250);
  TEST_ASSERT_TRUE(button_latch_take(&latch, NULL));
  hold(false, 250, 255);
  hold(true, 255, 260);
  hold(false, 260, 265);
  hold(true, 265, 270);
  hold(false, 270, 500);
  TEST_ASSERT_FALSE(button_latch_take(&latch, NULL));
}

void test_two_separate_clicks_are_two_presses(void) {
  hold(true, 100, 250);
  hold(false, 250, 400);
  hold(true, 400, 550);
  hold(false, 550, 700);
  TEST_ASSERT_EQUAL_UINT32(2, button_latch_pending(&latch));
}

void test_taking_collapses_several_pending_presses_into_one_event(void) {
  hold(true, 100, 250);
  hold(false, 250, 400);
  hold(true, 400, 550);
  hold(false, 550, 700);
  TEST_ASSERT_TRUE(button_latch_take(&latch, NULL));
  TEST_ASSERT_EQUAL_UINT32(0, button_latch_pending(&latch));
  TEST_ASSERT_FALSE(button_latch_take(&latch, NULL));
}

void test_a_press_after_a_take_is_a_new_event(void) {
  hold(true, 100, 250);
  TEST_ASSERT_TRUE(button_latch_take(&latch, NULL));
  hold(false, 250, 400);
  hold(true, 400, 550);
  TEST_ASSERT_TRUE(button_latch_take(&latch, NULL));
}

void test_take_reports_when_the_press_began(void) {
  hold(false, 5, 1000);
  hold(true, 1000, 1200);
  uint32_t began = 0;
  TEST_ASSERT_TRUE(button_latch_take(&latch, &began));
  TEST_ASSERT_EQUAL_UINT32(1000, began);
}

void test_a_button_held_at_start_is_not_a_press(void) {
  ButtonLatch held_at_boot;
  button_latch_init(&held_at_boot, 30);
  for (uint32_t t = 0; t < 500; t += POLL_MS)
    button_latch_sample(&held_at_boot, true, t);
  TEST_ASSERT_EQUAL_UINT32(0, button_latch_pending(&held_at_boot));
  for (uint32_t t = 500; t < 700; t += POLL_MS)
    button_latch_sample(&held_at_boot, false, t);
  for (uint32_t t = 700; t < 900; t += POLL_MS)
    button_latch_sample(&held_at_boot, true, t);
  TEST_ASSERT_EQUAL_UINT32(1, button_latch_pending(&held_at_boot));
}

void test_the_settle_time_survives_the_millisecond_counter_wrapping(void) {
  hold(false, 5, 0xFFFFFF00u);
  hold(false, 0xFFFFFF00u, 0xFFFFFFF0u);
  hold(true, 0xFFFFFFF0u, 0x00000050u);
  TEST_ASSERT_EQUAL_UINT32(1, button_latch_pending(&latch));
}

/* The device samples on one core and takes on another. The press that begins
 * latest must never be reported with an earlier press's time. */
enum { STRESS_PRESSES = 3000000 };
static ButtonLatch shared;
static volatile int producer_done;

static void *stress_producer(void *unused) {
  (void)unused;
  uint32_t t = 1000;
  for (int i = 0; i < STRESS_PRESSES; i++) {
    button_latch_sample(&shared, true, t);
    button_latch_sample(&shared, true, t + 30);
    button_latch_sample(&shared, false, t + 31);
    button_latch_sample(&shared, false, t + 61);
    t += 100;
  }
  producer_done = 1;
  return NULL;
}

void test_took_press_times_never_go_backwards_when_sampling_races_with_take(void) {
  button_latch_init(&shared, 30);
  button_latch_sample(&shared, false, 0);
  producer_done = 0;
  pthread_t producer;
  TEST_ASSERT_EQUAL_INT(0, pthread_create(&producer, NULL, stress_producer, NULL));
  uint32_t last = 0, began = 0;
  long stale = 0, takes = 0;
  for (;;) {
    int finished = producer_done;
    if (button_latch_take(&shared, &began)) {
      takes++;
      if (began <= last) stale++;
      last = began;
    } else if (finished) {
      break;
    }
    for (volatile int spin = 0; spin < 40; spin++) {}
  }
  pthread_join(producer, NULL);
  TEST_ASSERT_TRUE(takes > 1000);
  TEST_ASSERT_EQUAL_INT(0, stale);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_took_press_times_never_go_backwards_when_sampling_races_with_take);
  RUN_TEST(test_a_button_left_alone_gives_nothing_to_take);
  RUN_TEST(test_a_click_is_one_press_taken_once);
  RUN_TEST(test_a_press_only_counts_once_it_has_settled);
  RUN_TEST(test_a_long_press_is_still_one_press);
  RUN_TEST(test_a_glitch_shorter_than_the_settle_time_is_ignored);
  RUN_TEST(test_an_old_glitch_does_not_shorten_the_settle_time_of_a_later_press);
  RUN_TEST(test_bounce_on_press_is_one_press);
  RUN_TEST(test_bounce_on_release_is_not_a_second_press);
  RUN_TEST(test_two_separate_clicks_are_two_presses);
  RUN_TEST(test_taking_collapses_several_pending_presses_into_one_event);
  RUN_TEST(test_a_press_after_a_take_is_a_new_event);
  RUN_TEST(test_take_reports_when_the_press_began);
  RUN_TEST(test_a_button_held_at_start_is_not_a_press);
  RUN_TEST(test_the_settle_time_survives_the_millisecond_counter_wrapping);
  return UNITY_END();
}
