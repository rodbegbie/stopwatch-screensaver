#include <stdlib.h>
#include <unity.h>

#include "core/canvas.h"
#include "core/push_plan.h"

#define W 466
#define H 466
#define C1 0x1234

static Canvas cv;
static PushRow *rows;

void setUp(void) {
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&cv, W, H, malloc));
  rows = malloc(H * sizeof(PushRow));
  canvas_clear_dirty(&cv);
}
void tearDown(void) {
  free(rows);
  canvas_free(&cv);
}

typedef struct {
  char kind;
  int y, x0, x1;
} Event;
static Event events[3 * H + 4];
static int nevents;

static void on_begin(void *ctx) { (void)ctx; events[nevents++] = (Event){'b', 0, 0, 0}; }
static void on_row(void *ctx, int y, int x0, int x1) { (void)ctx; events[nevents++] = (Event){'r', y, x0, x1}; }
static void on_all(void *ctx) { (void)ctx; events[nevents++] = (Event){'a', 0, 0, 0}; }
static void on_end(void *ctx) { (void)ctx; events[nevents++] = (Event){'e', 0, 0, 0}; }
static const PushSink sink = {NULL, on_begin, on_row, on_all, on_end};

void test_nothing_dirty_plans_nothing(void) {
  int count = 99;
  TEST_ASSERT_EQUAL_INT(PUSH_NONE, push_plan(&cv, rows, &count, false));
  TEST_ASSERT_EQUAL_INT(0, count);
}

void test_a_fresh_canvas_plans_everything(void) {
  Canvas fresh;
  int count = 99;
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&fresh, W, H, malloc));
  TEST_ASSERT_EQUAL_INT(PUSH_ALL, push_plan(&fresh, rows, &count, false));
  TEST_ASSERT_EQUAL_INT(0, count);
  canvas_free(&fresh);
}

void test_one_point_plans_one_row_of_one_pixel(void) {
  int count = 0;
  canvas_point(&cv, 200, 77, C1);
  TEST_ASSERT_EQUAL_INT(PUSH_ROWS, push_plan(&cv, rows, &count, false));
  TEST_ASSERT_EQUAL_INT(1, count);
  TEST_ASSERT_EQUAL_INT(77, rows[0].y);
  TEST_ASSERT_EQUAL_INT(200, rows[0].x0);
  TEST_ASSERT_EQUAL_INT(200, rows[0].x1);
}

void test_rows_come_out_in_increasing_order(void) {
  int count = 0;
  canvas_point(&cv, 1, 300, C1);
  canvas_point(&cv, 2, 5, C1);
  canvas_point(&cv, 3, 100, C1);
  TEST_ASSERT_EQUAL_INT(PUSH_ROWS, push_plan(&cv, rows, &count, false));
  TEST_ASSERT_EQUAL_INT(3, count);
  TEST_ASSERT_EQUAL_INT(5, rows[0].y);
  TEST_ASSERT_EQUAL_INT(100, rows[1].y);
  TEST_ASSERT_EQUAL_INT(300, rows[2].y);
}

void test_planning_clears_the_spans(void) {
  int count = 0;
  canvas_point(&cv, 10, 10, C1);
  push_plan(&cv, rows, &count, false);
  TEST_ASSERT_EQUAL_INT(PUSH_NONE, push_plan(&cv, rows, &count, false));
}

void test_a_full_width_row_alone_is_still_rows(void) {
  int count = 0;
  canvas_fill_rect(&cv, 0, 200, W, 1, C1);
  TEST_ASSERT_EQUAL_INT(PUSH_ROWS, push_plan(&cv, rows, &count, false));
  TEST_ASSERT_EQUAL_INT(1, count);
  TEST_ASSERT_EQUAL_INT(W - 1, rows[0].x1);
}

void test_many_nearly_full_rows_fall_back_to_a_full_push(void) {
  int count = 0;
  canvas_fill_rect(&cv, 0, 0, 460, H, C1);
  TEST_ASSERT_EQUAL_INT(PUSH_ALL, push_plan(&cv, rows, &count, false));
  TEST_ASSERT_EQUAL_INT(0, count);
  canvas_fill_rect(&cv, 0, 0, 300, H, C1);
  TEST_ASSERT_EQUAL_INT(PUSH_ROWS, push_plan(&cv, rows, &count, false));
  TEST_ASSERT_EQUAL_INT(H, count);
}

/* The panel flushes one bounding box per batch, so two distant pixels in one
 * batch cost a full-screen flush (11.7 ms on the device) but 0.1 ms apart. */
void test_two_corner_pixels_are_two_groups_not_one_big_box(void) {
  int count = 0;
  canvas_point(&cv, 0, 0, C1);
  canvas_point(&cv, W - 1, H - 1, C1);
  TEST_ASSERT_EQUAL_INT(PUSH_ROWS, push_plan(&cv, rows, &count, false));
  TEST_ASSERT_EQUAL_INT(2, count);
  TEST_ASSERT_TRUE(rows[0].end_of_group);
  TEST_ASSERT_TRUE(rows[1].end_of_group);
}

void test_adjacent_rows_share_a_group(void) {
  int count = 0;
  canvas_fill_rect(&cv, 200, 100, 40, 20, C1);
  TEST_ASSERT_EQUAL_INT(PUSH_ROWS, push_plan(&cv, rows, &count, false));
  TEST_ASSERT_EQUAL_INT(20, count);
  for (int i = 0; i < 19; i++) TEST_ASSERT_FALSE(rows[i].end_of_group);
  TEST_ASSERT_TRUE(rows[19].end_of_group);
}

void test_narrow_rows_with_a_small_gap_share_a_group(void) {
  int count = 0;
  canvas_fill_rect(&cv, 200, 100, 40, 1, C1);
  canvas_fill_rect(&cv, 200, 103, 40, 1, C1);
  TEST_ASSERT_EQUAL_INT(PUSH_ROWS, push_plan(&cv, rows, &count, false));
  TEST_ASSERT_EQUAL_INT(2, count);
  TEST_ASSERT_FALSE(rows[0].end_of_group);
  TEST_ASSERT_TRUE(rows[1].end_of_group);
}

void test_distant_narrow_rows_do_not_share_a_group(void) {
  int count = 0;
  canvas_fill_rect(&cv, 200, 10, 40, 1, C1);
  canvas_fill_rect(&cv, 200, 400, 40, 1, C1);
  TEST_ASSERT_EQUAL_INT(PUSH_ROWS, push_plan(&cv, rows, &count, false));
  TEST_ASSERT_EQUAL_INT(2, count);
  TEST_ASSERT_TRUE(rows[0].end_of_group);
  TEST_ASSERT_TRUE(rows[1].end_of_group);
}

void test_the_last_row_always_ends_a_group(void) {
  int count = 0;
  canvas_point(&cv, 5, 7, C1);
  push_plan(&cv, rows, &count, false);
  TEST_ASSERT_TRUE(rows[count - 1].end_of_group);
}

void test_present_calls_nothing_when_nothing_is_dirty(void) {
  nevents = 0;
  TEST_ASSERT_EQUAL_INT(0, push_present(&cv, rows, &sink, false));
  TEST_ASSERT_EQUAL_INT(0, nevents);
}

void test_present_runs_begin_rows_end_in_order(void) {
  nevents = 0;
  canvas_point(&cv, 9, 41, C1);
  canvas_point(&cv, 4, 40, C1);
  canvas_point(&cv, 8, 40, C1);
  TEST_ASSERT_EQUAL_INT(2, push_present(&cv, rows, &sink, false));
  TEST_ASSERT_EQUAL_INT(4, nevents);
  TEST_ASSERT_EQUAL_INT('b', events[0].kind);
  TEST_ASSERT_EQUAL_INT('r', events[1].kind);
  TEST_ASSERT_EQUAL_INT(40, events[1].y);
  TEST_ASSERT_EQUAL_INT(4, events[1].x0);
  TEST_ASSERT_EQUAL_INT(8, events[1].x1);
  TEST_ASSERT_EQUAL_INT('r', events[2].kind);
  TEST_ASSERT_EQUAL_INT(41, events[2].y);
  TEST_ASSERT_EQUAL_INT('e', events[3].kind);
}

void test_present_wraps_each_group_in_its_own_begin_and_end(void) {
  nevents = 0;
  canvas_point(&cv, 0, 0, C1);
  canvas_point(&cv, W - 1, H - 1, C1);
  TEST_ASSERT_EQUAL_INT(2, push_present(&cv, rows, &sink, false));
  TEST_ASSERT_EQUAL_INT(6, nevents);
  const char want[] = {'b', 'r', 'e', 'b', 'r', 'e'};
  for (int i = 0; i < 6; i++) TEST_ASSERT_EQUAL_INT(want[i], events[i].kind);
}

void test_present_pushes_all_with_a_single_all_call(void) {
  nevents = 0;
  canvas_clear(&cv, C1);
  TEST_ASSERT_EQUAL_INT(H, push_present(&cv, rows, &sink, false));
  TEST_ASSERT_EQUAL_INT(1, nevents);
  TEST_ASSERT_EQUAL_INT('a', events[0].kind);
}

static void add_disc(Canvas *c) { TEST_ASSERT_EQUAL_INT(0, canvas_set_disc(c, malloc)); }

static int groups_in(const PushRow *r, int count) {
  int g = 0;
  for (int i = 0; i < count; i++) g += r[i].end_of_group;
  return g;
}

void test_disc_drops_a_row_dirty_only_in_a_corner(void) {
  int count = 99;
  add_disc(&cv);
  canvas_point(&cv, 0, 0, C1);
  TEST_ASSERT_EQUAL_INT(PUSH_NONE, push_plan(&cv, rows, &count, true));
  TEST_ASSERT_EQUAL_INT(0, count);
}

void test_disc_clips_a_full_row_to_its_span(void) {
  int count = 0, v0, v1;
  add_disc(&cv);
  canvas_fill_rect(&cv, 0, 0, W, 1, C1);
  TEST_ASSERT_EQUAL_INT(PUSH_ROWS, push_plan(&cv, rows, &count, true));
  TEST_ASSERT_EQUAL_INT(1, count);
  TEST_ASSERT_TRUE(canvas_visible_span(&cv, 0, &v0, &v1));
  TEST_ASSERT_EQUAL_INT(0, rows[0].y);
  TEST_ASSERT_EQUAL_INT(v0, rows[0].x0);
  TEST_ASSERT_EQUAL_INT(v1, rows[0].x1);
}

void test_disc_full_frame_is_bands_not_all(void) {
  Canvas fresh;
  int count = 0;
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&fresh, W, H, malloc));
  add_disc(&fresh);
  TEST_ASSERT_EQUAL_INT(PUSH_ROWS, push_plan(&fresh, rows, &count, true));
  TEST_ASSERT_EQUAL_INT(H, count);
  TEST_ASSERT_TRUE(groups_in(rows, count) > 1);
  for (int i = 0; i < count; i++) {
    int v0, v1;
    TEST_ASSERT_TRUE(canvas_visible_span(&fresh, rows[i].y, &v0, &v1));
    TEST_ASSERT_EQUAL_INT(v0, rows[i].x0);
    TEST_ASSERT_EQUAL_INT(v1, rows[i].x1);
  }
  canvas_free(&fresh);
}

/* The model predicts about 26.8 ms in bands against 28.2 ms for the greedy
 * grouping of the same clipped rows and 31.1 ms for one full push. */
void test_disc_full_frame_beats_greedy_and_a_full_push(void) {
  Canvas fresh;
  int count = 0;
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&fresh, W, H, malloc));
  add_disc(&fresh);
  push_plan(&fresh, rows, &count, true);
  const int64_t banded = push_plan_cost(rows, count);
  const int64_t greedy = push_group_greedy(rows, count);
  TEST_ASSERT_TRUE(banded < greedy);
  TEST_ASSERT_TRUE(banded < (int64_t)W * H * PUSH_PIXEL_NS);
  TEST_ASSERT_INT64_WITHIN(500000, 26800000, banded);
  canvas_free(&fresh);
}

static void assert_disc_plan_no_dearer_than_greedy(void) {
  int count = 0;
  if (push_plan(&cv, rows, &count, true) != PUSH_ROWS) return;
  const int64_t chosen = push_plan_cost(rows, count);
  TEST_ASSERT_TRUE(chosen <= push_group_greedy(rows, count));
}

void test_disc_plan_never_costs_more_than_greedy(void) {
  add_disc(&cv);
  for (int y = 0; y < H; y += 2) canvas_fill_rect(&cv, 0, y, W, 1, C1);
  assert_disc_plan_no_dearer_than_greedy();
  srandom(7);
  for (int k = 0; k < 200; k++) {
    const int n = 1 + (int)(random() % 6);
    for (int r = 0; r < n; r++)
      canvas_fill_rect(&cv, (int)(random() % W), (int)(random() % H),
                       1 + (int)(random() % 200), 1 + (int)(random() % 200), C1);
    assert_disc_plan_no_dearer_than_greedy();
  }
}

/* Longer than the split's lookback: one thin run must stay one flush. */
void test_disc_keeps_a_long_thin_run_in_one_group(void) {
  int count = 0;
  add_disc(&cv);
  canvas_fill_rect(&cv, 233, 100, 1, 266, C1);
  TEST_ASSERT_EQUAL_INT(PUSH_ROWS, push_plan(&cv, rows, &count, true));
  TEST_ASSERT_EQUAL_INT(266, count);
  TEST_ASSERT_EQUAL_INT(1, groups_in(rows, count));
}

void test_without_the_flag_a_disc_canvas_plans_as_today(void) {
  Canvas fresh;
  int count = 99;
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&fresh, W, H, malloc));
  add_disc(&fresh);
  TEST_ASSERT_EQUAL_INT(PUSH_ALL, push_plan(&fresh, rows, &count, false));
  canvas_point(&fresh, 0, 0, C1);
  TEST_ASSERT_EQUAL_INT(PUSH_ROWS, push_plan(&fresh, rows, &count, false));
  TEST_ASSERT_EQUAL_INT(1, count);
  canvas_free(&fresh);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_nothing_dirty_plans_nothing);
  RUN_TEST(test_a_fresh_canvas_plans_everything);
  RUN_TEST(test_one_point_plans_one_row_of_one_pixel);
  RUN_TEST(test_rows_come_out_in_increasing_order);
  RUN_TEST(test_planning_clears_the_spans);
  RUN_TEST(test_a_full_width_row_alone_is_still_rows);
  RUN_TEST(test_many_nearly_full_rows_fall_back_to_a_full_push);
  RUN_TEST(test_two_corner_pixels_are_two_groups_not_one_big_box);
  RUN_TEST(test_adjacent_rows_share_a_group);
  RUN_TEST(test_narrow_rows_with_a_small_gap_share_a_group);
  RUN_TEST(test_distant_narrow_rows_do_not_share_a_group);
  RUN_TEST(test_the_last_row_always_ends_a_group);
  RUN_TEST(test_present_calls_nothing_when_nothing_is_dirty);
  RUN_TEST(test_present_runs_begin_rows_end_in_order);
  RUN_TEST(test_present_wraps_each_group_in_its_own_begin_and_end);
  RUN_TEST(test_present_pushes_all_with_a_single_all_call);
  RUN_TEST(test_disc_drops_a_row_dirty_only_in_a_corner);
  RUN_TEST(test_disc_clips_a_full_row_to_its_span);
  RUN_TEST(test_disc_full_frame_is_bands_not_all);
  RUN_TEST(test_disc_full_frame_beats_greedy_and_a_full_push);
  RUN_TEST(test_disc_plan_never_costs_more_than_greedy);
  RUN_TEST(test_disc_keeps_a_long_thin_run_in_one_group);
  RUN_TEST(test_without_the_flag_a_disc_canvas_plans_as_today);
  return UNITY_END();
}
