#include <string.h>
#include <unity.h>

#include "hacks/phosphor_pick.h"
#include "hacks/phosphor_text.h"
#include "hacks/phosphor_timers.h"

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

static int fired[8];
static int fired_count;
static unsigned long fired_id[8];
static void note(void *closure, unsigned long *id) {
  fired[fired_count] = (int)(long)closure;
  fired_id[fired_count++] = *id;
}

static PtQueue q;
static void reset_queue(void) {
  memset(&q, 0, sizeof q);
  fired_count = 0;
}

void test_a_timer_fires_once_when_its_time_comes_and_not_before(void) {
  reset_queue();
  unsigned long id = pt_add(&q, 10.0, 333, note, (void *)1);
  TEST_ASSERT_NOT_EQUAL(0, id);
  pt_fire(&q, 10.332);
  TEST_ASSERT_EQUAL_INT(0, fired_count);
  pt_fire(&q, 10.333);
  TEST_ASSERT_EQUAL_INT(1, fired_count);
  TEST_ASSERT_EQUAL_UINT32(id, fired_id[0]);
  pt_fire(&q, 99.0);
  TEST_ASSERT_EQUAL_INT(1, fired_count);
}

void test_a_removed_timer_never_fires(void) {
  reset_queue();
  unsigned long a = pt_add(&q, 0.0, 100, note, (void *)1);
  pt_add(&q, 0.0, 100, note, (void *)2);
  pt_remove(&q, a);
  pt_fire(&q, 1.0);
  TEST_ASSERT_EQUAL_INT(1, fired_count);
  TEST_ASSERT_EQUAL_INT(2, fired[0]);
}

void test_removing_an_id_that_is_not_there_changes_nothing(void) {
  reset_queue();
  pt_add(&q, 0.0, 100, note, (void *)1);
  pt_remove(&q, 0);
  pt_remove(&q, 12345);
  pt_fire(&q, 1.0);
  TEST_ASSERT_EQUAL_INT(1, fired_count);
}

static void rearm(void *closure, unsigned long *id) {
  (void)id;
  fired[fired_count++] = 7;
  pt_add((PtQueue *)closure, 5.0, 100, note, (void *)9);
}

void test_a_timer_may_add_another_and_that_one_waits_for_its_own_time(void) {
  reset_queue();
  pt_add(&q, 5.0, 0, rearm, &q);
  pt_fire(&q, 5.0);
  TEST_ASSERT_EQUAL_INT(1, fired_count);
  pt_fire(&q, 5.05);
  TEST_ASSERT_EQUAL_INT(1, fired_count);
  pt_fire(&q, 5.1);
  TEST_ASSERT_EQUAL_INT(2, fired_count);
  TEST_ASSERT_EQUAL_INT(9, fired[1]);
}

void test_every_timer_that_is_due_fires_in_one_call(void) {
  reset_queue();
  for (long i = 1; i <= 3; i++) pt_add(&q, 0.0, 10 * (unsigned long)i, note, (void *)i);
  pt_fire(&q, 1.0);
  TEST_ASSERT_EQUAL_INT(3, fired_count);
}

void test_ids_are_never_zero_and_never_reused_while_pending(void) {
  reset_queue();
  unsigned long seen[PT_SLOTS];
  for (int i = 0; i < PT_SLOTS; i++) {
    seen[i] = pt_add(&q, 0.0, 10, note, (void *)1);
    TEST_ASSERT_NOT_EQUAL(0, seen[i]);
    for (int j = 0; j < i; j++) TEST_ASSERT_NOT_EQUAL(seen[j], seen[i]);
  }
}

void test_a_full_queue_says_so_with_id_zero(void) {
  reset_queue();
  for (int i = 0; i < PT_SLOTS; i++) pt_add(&q, 0.0, 10, note, (void *)1);
  TEST_ASSERT_EQUAL_UINT32(0, pt_add(&q, 0.0, 10, note, (void *)1));
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
  RUN_TEST(test_a_timer_fires_once_when_its_time_comes_and_not_before);
  RUN_TEST(test_a_removed_timer_never_fires);
  RUN_TEST(test_removing_an_id_that_is_not_there_changes_nothing);
  RUN_TEST(test_a_timer_may_add_another_and_that_one_waits_for_its_own_time);
  RUN_TEST(test_every_timer_that_is_due_fires_in_one_call);
  RUN_TEST(test_ids_are_never_zero_and_never_reused_while_pending);
  RUN_TEST(test_a_full_queue_says_so_with_id_zero);
  return UNITY_END();
}
