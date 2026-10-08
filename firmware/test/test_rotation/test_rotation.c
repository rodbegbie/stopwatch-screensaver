#include <unity.h>

#include "runner/rotation.h"

static Rotation r;

void setUp(void) { rotation_init(&r, 90000, 1000); }
void tearDown(void) {}

void test_it_is_not_due_until_the_interval_has_passed(void) {
  TEST_ASSERT_FALSE(rotation_due(&r, 1000));
  TEST_ASSERT_FALSE(rotation_due(&r, 90999));
  TEST_ASSERT_TRUE(rotation_due(&r, 91000));
  TEST_ASSERT_TRUE(rotation_due(&r, 500000));
}

void test_reset_restarts_the_countdown(void) {
  rotation_reset(&r, 50000);
  TEST_ASSERT_FALSE(rotation_due(&r, 91000));
  TEST_ASSERT_FALSE(rotation_due(&r, 139999));
  TEST_ASSERT_TRUE(rotation_due(&r, 140000));
}

void test_an_interval_of_zero_never_rotates(void) {
  rotation_init(&r, 0, 0);
  TEST_ASSERT_FALSE(rotation_due(&r, 0));
  TEST_ASSERT_FALSE(rotation_due(&r, 0x7FFFFFFFu));
}

void test_the_countdown_survives_the_millisecond_counter_wrapping(void) {
  rotation_reset(&r, 0xFFFFFFFFu - 10000);
  TEST_ASSERT_FALSE(rotation_due(&r, 50000));
  TEST_ASSERT_TRUE(rotation_due(&r, 80000));
}

void test_a_short_interval_works_for_leak_testing(void) {
  rotation_init(&r, 5000, 0);
  TEST_ASSERT_FALSE(rotation_due(&r, 4999));
  TEST_ASSERT_TRUE(rotation_due(&r, 5000));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_it_is_not_due_until_the_interval_has_passed);
  RUN_TEST(test_reset_restarts_the_countdown);
  RUN_TEST(test_an_interval_of_zero_never_rotates);
  RUN_TEST(test_the_countdown_survives_the_millisecond_counter_wrapping);
  RUN_TEST(test_a_short_interval_works_for_leak_testing);
  return UNITY_END();
}
