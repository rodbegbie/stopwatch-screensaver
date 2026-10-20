#include <unity.h>

#include "hacks/phosphor_pick.h"
#include "hacks/phosphor_text.h"

void setUp(void) {}
void tearDown(void) {}

void test_it_never_picks_the_poem_it_just_showed(void) {
  for (int count = 2; count <= 9; count++)
    for (int last = 0; last < count; last++)
      for (unsigned long r = 0; r < 200; r++)
        TEST_ASSERT_NOT_EQUAL(last, phosphor_next_poem(last, count, r));
}

void test_it_stays_inside_the_list(void) {
  for (int count = 1; count <= 9; count++)
    for (int last = -1; last < count; last++)
      for (unsigned long r = 0; r < 200; r++) {
        const int n = phosphor_next_poem(last, count, r);
        TEST_ASSERT_TRUE(n >= 0 && n < count);
      }
}

void test_every_other_poem_comes_up_equally_often(void) {
  const int count = 7, last = 3;
  int seen[7] = {0};
  for (unsigned long r = 0; r < 6 * 50; r++) seen[phosphor_next_poem(last, count, r)]++;
  for (int i = 0; i < count; i++) TEST_ASSERT_EQUAL_INT(i == last ? 0 : 50, seen[i]);
}

void test_the_first_pick_can_be_any_poem(void) {
  int seen[5] = {0};
  for (unsigned long r = 0; r < 5; r++) seen[phosphor_next_poem(-1, 5, r)]++;
  for (int i = 0; i < 5; i++) TEST_ASSERT_EQUAL_INT(1, seen[i]);
}

void test_a_single_poem_is_picked_again(void) {
  TEST_ASSERT_EQUAL_INT(0, phosphor_next_poem(0, 1, 12345));
  TEST_ASSERT_EQUAL_INT(0, phosphor_next_poem(-1, 1, 12345));
}

static const char *const kStub[] = {"ab\001cd", "xy"};

static void expect_gap(struct text_data *td, int frames) {
  for (int i = 0; i < frames; i++) TEST_ASSERT_EQUAL_INT(-1, phosphor_text_next(td));
}

/* Starts on poem 0 whatever random() says: poem 1 is only picked from there. */
static struct text_data *start_on_first(void) {
  struct text_data *td = phosphor_text_create(kStub, 2);
  td->last = 0;
  td->poem = kStub[0];
  td->pos = 0;
  return td;
}

void test_a_page_break_waits_and_then_goes_on_with_the_same_poem(void) {
  struct text_data *td = start_on_first();
  TEST_ASSERT_EQUAL_INT('a', phosphor_text_next(td));
  TEST_ASSERT_EQUAL_INT('b', phosphor_text_next(td));
  expect_gap(td, 1 + PHOSPHOR_MORE_FRAMES);
  TEST_ASSERT_EQUAL_INT('c', phosphor_text_next(td));
  TEST_ASSERT_EQUAL_INT('d', phosphor_text_next(td));
  free(td);
}

void test_the_end_of_a_poem_waits_and_then_starts_the_other_poem(void) {
  struct text_data *td = start_on_first();
  for (const char *p = "ab"; *p; p++) TEST_ASSERT_EQUAL_INT(*p, phosphor_text_next(td));
  expect_gap(td, 1 + PHOSPHOR_MORE_FRAMES);
  for (const char *p = "cd"; *p; p++) TEST_ASSERT_EQUAL_INT(*p, phosphor_text_next(td));
  expect_gap(td, 1 + PHOSPHOR_PAUSE_FRAMES);
  TEST_ASSERT_EQUAL_INT('x', phosphor_text_next(td));
  TEST_ASSERT_EQUAL_INT('y', phosphor_text_next(td));
  free(td);
}

void test_poems_alternate_when_there_are_two_and_never_repeat(void) {
  struct text_data *td = start_on_first();
  int first_of_each[40];
  int loops = 0;
  while (loops < 40) {
    const int c = phosphor_text_next(td);
    if (c == 'a' || c == 'x') first_of_each[loops++] = c;
  }
  for (int i = 0; i < 40; i++) TEST_ASSERT_EQUAL_INT(i % 2 == 0 ? 'a' : 'x', first_of_each[i]);
  free(td);
}

void test_a_fresh_source_can_start_on_any_poem(void) {
  int seen[2] = {0, 0};
  for (int s = 1; s <= 40; s++) {
    srandom(s);
    struct text_data *td = phosphor_text_create(kStub, 2);
    seen[phosphor_text_next(td) == 'x']++;
    free(td);
  }
  TEST_ASSERT_TRUE(seen[0] > 0 && seen[1] > 0);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_it_never_picks_the_poem_it_just_showed);
  RUN_TEST(test_it_stays_inside_the_list);
  RUN_TEST(test_every_other_poem_comes_up_equally_often);
  RUN_TEST(test_the_first_pick_can_be_any_poem);
  RUN_TEST(test_a_single_poem_is_picked_again);
  RUN_TEST(test_a_page_break_waits_and_then_goes_on_with_the_same_poem);
  RUN_TEST(test_the_end_of_a_poem_waits_and_then_starts_the_other_poem);
  RUN_TEST(test_poems_alternate_when_there_are_two_and_never_repeat);
  RUN_TEST(test_a_fresh_source_can_start_on_any_poem);
  return UNITY_END();
}
