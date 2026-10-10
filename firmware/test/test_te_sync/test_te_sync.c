#include <stdint.h>
#include <unity.h>

#include "runner/te_sync.h"

#define PERIOD 16819u /* the panel's measured TE period, in microseconds */

static TeSync s;

void setUp(void) { te_sync_init(&s, 1100, 100000); }
void tearDown(void) {}

/* Two rising edges one period apart teach it the period. */
static void learn_from(uint32_t first_rise) {
  te_sync_edge(&s, first_rise, 1);
  te_sync_edge(&s, first_rise + 1100, 0);
  te_sync_edge(&s, first_rise + PERIOD, 1);
}

void test_without_any_edge_it_never_waits(void) {
  TEST_ASSERT_EQUAL_UINT32(0, te_sync_wait_us(&s, 5000));
}

void test_one_edge_is_not_enough_to_know_the_period(void) {
  te_sync_edge(&s, 1000, 1);
  TEST_ASSERT_EQUAL_UINT32(0, te_sync_wait_us(&s, 1200));
}

void test_the_period_is_the_gap_between_rising_edges(void) {
  learn_from(10000);
  TEST_ASSERT_EQUAL_UINT32(PERIOD, te_sync_period_us(&s));
}

void test_falling_edges_do_not_count_as_periods(void) {
  te_sync_edge(&s, 10000, 1);
  te_sync_edge(&s, 11100, 0);
  TEST_ASSERT_EQUAL_UINT32(0, te_sync_period_us(&s));
}

void test_a_falling_edge_a_whole_period_later_is_still_not_a_rising_one(void) {
  te_sync_edge(&s, 10000, 1);
  te_sync_edge(&s, 10000 + PERIOD, 0);
  TEST_ASSERT_EQUAL_UINT32(0, te_sync_period_us(&s));
  TEST_ASSERT_EQUAL_UINT32(0, te_sync_wait_us(&s, 10000 + PERIOD + 100));
}

void test_it_waits_for_the_offset_after_the_latest_rising_edge(void) {
  learn_from(10000);
  const uint32_t rise = 10000 + PERIOD;
  TEST_ASSERT_EQUAL_UINT32(1100, te_sync_wait_us(&s, rise));
  TEST_ASSERT_EQUAL_UINT32(600, te_sync_wait_us(&s, rise + 500));
  TEST_ASSERT_EQUAL_UINT32(0, te_sync_wait_us(&s, rise + 1100));
}

void test_past_the_offset_it_waits_for_the_next_period(void) {
  learn_from(10000);
  const uint32_t rise = 10000 + PERIOD;
  TEST_ASSERT_EQUAL_UINT32(PERIOD - 2000 + 1100, te_sync_wait_us(&s, rise + 2000));
  TEST_ASSERT_EQUAL_UINT32(1100 - 1, te_sync_wait_us(&s, rise + PERIOD + 1));
}

void test_the_wait_never_exceeds_one_period(void) {
  learn_from(10000);
  const uint32_t rise = 10000 + PERIOD;
  for (uint32_t t = 0; t < 3 * PERIOD; t += 137)
    TEST_ASSERT_TRUE(te_sync_wait_us(&s, rise + t) < PERIOD);
}

void test_a_later_offset_waits_longer(void) {
  te_sync_init(&s, 5000, 100000);
  learn_from(10000);
  TEST_ASSERT_EQUAL_UINT32(5000, te_sync_wait_us(&s, 10000 + PERIOD));
}

void test_an_offset_longer_than_the_period_wraps_into_it(void) {
  te_sync_init(&s, PERIOD + 300, 100000);
  learn_from(10000);
  TEST_ASSERT_EQUAL_UINT32(300, te_sync_wait_us(&s, 10000 + PERIOD));
}

void test_the_microsecond_counter_wrapping_does_not_confuse_it(void) {
  const uint32_t first = 0xFFFFFFFFu - 5000u;
  learn_from(first); /* the second rise lands after the counter wrapped */
  const uint32_t rise = first + PERIOD;
  TEST_ASSERT_TRUE(rise < first);
  TEST_ASSERT_EQUAL_UINT32(PERIOD, te_sync_period_us(&s));
  TEST_ASSERT_EQUAL_UINT32(600, te_sync_wait_us(&s, rise + 500));
}

void test_when_the_edges_stop_it_stops_waiting(void) {
  learn_from(10000);
  const uint32_t rise = 10000 + PERIOD;
  TEST_ASSERT_TRUE(te_sync_wait_us(&s, rise + 90000) < PERIOD); /* still waiting */
  TEST_ASSERT_EQUAL_UINT32(0, te_sync_wait_us(&s, rise + 100001));
  TEST_ASSERT_EQUAL_UINT32(0, te_sync_wait_us(&s, rise + 5000000));
}

void test_a_glitch_edge_is_not_taken_for_a_period(void) {
  learn_from(10000);
  const uint32_t rise = 10000 + PERIOD;
  te_sync_edge(&s, rise + 300, 1); /* far too soon after the last rise */
  TEST_ASSERT_EQUAL_UINT32(PERIOD, te_sync_period_us(&s));
  TEST_ASSERT_EQUAL_UINT32(1100, te_sync_wait_us(&s, rise));
}

void test_a_missed_edge_does_not_double_the_period(void) {
  learn_from(10000);
  const uint32_t rise = 10000 + PERIOD;
  te_sync_edge(&s, rise + 2 * PERIOD, 1); /* one pulse never reached us */
  TEST_ASSERT_EQUAL_UINT32(PERIOD, te_sync_period_us(&s));
}

void test_the_period_follows_a_slow_drift(void) {
  uint32_t t = 10000;
  te_sync_edge(&s, t, 1);
  for (int i = 0; i < 400; i++) {
    t += PERIOD + (uint32_t)(i / 10); /* each gap a little longer: 40 us by the end */
    te_sync_edge(&s, t, 1);
  }
  const uint32_t p = te_sync_period_us(&s);
  TEST_ASSERT_TRUE_MESSAGE(p >= PERIOD + 30 && p <= PERIOD + 45, "did not follow the drift");
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_without_any_edge_it_never_waits);
  RUN_TEST(test_one_edge_is_not_enough_to_know_the_period);
  RUN_TEST(test_the_period_is_the_gap_between_rising_edges);
  RUN_TEST(test_falling_edges_do_not_count_as_periods);
  RUN_TEST(test_a_falling_edge_a_whole_period_later_is_still_not_a_rising_one);
  RUN_TEST(test_it_waits_for_the_offset_after_the_latest_rising_edge);
  RUN_TEST(test_past_the_offset_it_waits_for_the_next_period);
  RUN_TEST(test_the_wait_never_exceeds_one_period);
  RUN_TEST(test_a_later_offset_waits_longer);
  RUN_TEST(test_an_offset_longer_than_the_period_wraps_into_it);
  RUN_TEST(test_the_microsecond_counter_wrapping_does_not_confuse_it);
  RUN_TEST(test_when_the_edges_stop_it_stops_waiting);
  RUN_TEST(test_a_glitch_edge_is_not_taken_for_a_period);
  RUN_TEST(test_a_missed_edge_does_not_double_the_period);
  RUN_TEST(test_the_period_follows_a_slow_drift);
  return UNITY_END();
}
