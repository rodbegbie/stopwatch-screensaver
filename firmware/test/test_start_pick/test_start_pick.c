#include <unity.h>

#include "runner/start_pick.h"

static const HackEntry a = {.name = "pyro"};
static const HackEntry b = {.name = "Galaxy"};
static const HackEntry c = {.name = "drift"};
static const HackEntry *const hacks[] = {&a, &b, &c};
#define N 3

void setUp(void) {}
void tearDown(void) {}

void test_a_name_finds_its_index(void) {
  TEST_ASSERT_EQUAL_INT(0, start_find_hack(hacks, N, "pyro"));
  TEST_ASSERT_EQUAL_INT(2, start_find_hack(hacks, N, "drift"));
}

void test_names_match_regardless_of_case(void) {
  TEST_ASSERT_EQUAL_INT(1, start_find_hack(hacks, N, "galaxy"));
  TEST_ASSERT_EQUAL_INT(0, start_find_hack(hacks, N, "PYRO"));
}

void test_an_unknown_or_partial_name_is_not_found(void) {
  TEST_ASSERT_EQUAL_INT(-1, start_find_hack(hacks, N, "nope"));
  TEST_ASSERT_EQUAL_INT(-1, start_find_hack(hacks, N, "pyr"));
  TEST_ASSERT_EQUAL_INT(-1, start_find_hack(hacks, N, "pyros"));
  TEST_ASSERT_EQUAL_INT(-1, start_find_hack(hacks, N, ""));
}

void test_no_forced_name_picks_by_the_random_number(void) {
  TEST_ASSERT_EQUAL_INT(0, start_pick_index(hacks, N, NULL, 0));
  TEST_ASSERT_EQUAL_INT(1, start_pick_index(hacks, N, NULL, 1));
  TEST_ASSERT_EQUAL_INT(2, start_pick_index(hacks, N, NULL, 5));
}

void test_every_hack_is_reachable_from_a_full_range_random_number(void) {
  TEST_ASSERT_EQUAL_INT(0xFFFFFFFFu % N, start_pick_index(hacks, N, NULL, 0xFFFFFFFFu));
}

void test_a_forced_name_beats_the_random_number(void) {
  TEST_ASSERT_EQUAL_INT(1, start_pick_index(hacks, N, "galaxy", 0));
  TEST_ASSERT_EQUAL_INT(1, start_pick_index(hacks, N, "galaxy", 2));
}

void test_an_unknown_forced_name_falls_back_to_random(void) {
  TEST_ASSERT_EQUAL_INT(2, start_pick_index(hacks, N, "nope", 5));
}

void test_an_empty_forced_name_means_random(void) {
  TEST_ASSERT_EQUAL_INT(1, start_pick_index(hacks, N, "", 4));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_a_name_finds_its_index);
  RUN_TEST(test_names_match_regardless_of_case);
  RUN_TEST(test_an_unknown_or_partial_name_is_not_found);
  RUN_TEST(test_no_forced_name_picks_by_the_random_number);
  RUN_TEST(test_every_hack_is_reachable_from_a_full_range_random_number);
  RUN_TEST(test_a_forced_name_beats_the_random_number);
  RUN_TEST(test_an_unknown_forced_name_falls_back_to_random);
  RUN_TEST(test_an_empty_forced_name_means_random);
  return UNITY_END();
}
