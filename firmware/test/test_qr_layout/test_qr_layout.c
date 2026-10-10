#include <unity.h>

#include "runner/qr_layout.h"

void setUp(void) {}
void tearDown(void) {}

void test_modules_are_as_big_as_whole_pixels_allow_with_the_quiet_zone(void) {
  QrLayout l = qr_layout(29, 2, 328);
  TEST_ASSERT_EQUAL(9, l.scale);
  TEST_ASSERT_EQUAL(29 * 9, l.size);
}

void test_the_code_is_centred_in_the_square(void) {
  QrLayout l = qr_layout(29, 2, 328);
  TEST_ASSERT_EQUAL((328 - 29 * 9) / 2, l.origin);
}

void test_the_quiet_zone_is_never_smaller_than_asked_for(void) {
  for (int modules = 21; modules <= 177; modules += 4) {
    QrLayout l = qr_layout(modules, 2, 328);
    TEST_ASSERT_GREATER_OR_EQUAL(2 * l.scale, l.origin);
    TEST_ASSERT_LESS_OR_EQUAL(328, l.origin + l.size + 2 * l.scale);
  }
}

void test_an_exact_fit_leaves_exactly_the_quiet_zone(void) {
  QrLayout l = qr_layout(21, 2, 25 * 4);
  TEST_ASSERT_EQUAL(4, l.scale);
  TEST_ASSERT_EQUAL(8, l.origin);
}

void test_a_code_too_big_for_the_square_has_scale_zero(void) {
  QrLayout l = qr_layout(177, 2, 100);
  TEST_ASSERT_EQUAL(0, l.scale);
  TEST_ASSERT_EQUAL(0, l.size);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_modules_are_as_big_as_whole_pixels_allow_with_the_quiet_zone);
  RUN_TEST(test_the_code_is_centred_in_the_square);
  RUN_TEST(test_the_quiet_zone_is_never_smaller_than_asked_for);
  RUN_TEST(test_an_exact_fit_leaves_exactly_the_quiet_zone);
  RUN_TEST(test_a_code_too_big_for_the_square_has_scale_zero);
  return UNITY_END();
}
