#include <sanitizer/allocator_interface.h>
#include <stdlib.h>
#include <unity.h>

#include "core/canvas.h"
#include "hacks/span_erase.h"
#include "x11shim/xshim.h"

static Canvas cv;
static SpanTrack tr;
static size_t live_before;

void setUp(void) {
  live_before = __sanitizer_get_current_allocated_bytes();
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&cv, 40, 30, malloc));
  span_track_reset(&tr);
  canvas_clear_dirty(&cv);
}
void tearDown(void) {
  canvas_free(&cv);
  TEST_ASSERT_EQUAL_UINT64_MESSAGE(live_before,
                                   __sanitizer_get_current_allocated_bytes(),
                                   "test leaked memory");
}

static int lit(void) {
  int n = 0;
  for (int i = 0; i < 40 * 30; i++) n += cv.px[i] != 0;
  return n;
}

void test_the_first_erase_clears_the_whole_canvas_to_the_colour(void) {
  canvas_clear(&cv, 0x1111);
  span_track_erase(&cv, &tr, 0x0002);
  for (int i = 0; i < 40 * 30; i++) TEST_ASSERT_EQUAL_HEX16(0x0002, cv.px[i]);
}

void test_a_later_erase_clears_only_the_rows_and_columns_that_were_noted(void) {
  span_track_erase(&cv, &tr, 0);
  const XPoint tri[3] = {{5, 4}, {15, 4}, {10, 12}};
  span_track_note(&tr, tri, 3);
  canvas_fill_rect(&cv, 5, 4, 10, 8, 0xFFFF);
  canvas_fill_rect(&cv, 30, 20, 4, 4, 0xAAAA); /* not noted: must survive */
  canvas_clear_dirty(&cv);
  span_track_erase(&cv, &tr, 0);
  TEST_ASSERT_EQUAL_INT(16, lit());
  TEST_ASSERT_EQUAL_HEX16(0xAAAA, cv.px[22 * 40 + 31]);
  int x0, x1;
  TEST_ASSERT_TRUE(canvas_dirty_row(&cv, 6, &x0, &x1));
  TEST_ASSERT_EQUAL_INT(5, x0);
  TEST_ASSERT_EQUAL_INT(15, x1);
  TEST_ASSERT_FALSE(canvas_dirty_row(&cv, 20, &x0, &x1));
}

void test_noted_polygons_union_their_columns_on_a_shared_row(void) {
  span_track_erase(&cv, &tr, 0);
  const XPoint a[3] = {{2, 5}, {6, 5}, {4, 8}};
  const XPoint b[3] = {{20, 6}, {26, 6}, {23, 9}};
  span_track_note(&tr, b, 3);
  span_track_note(&tr, a, 3);
  canvas_fill_rect(&cv, 0, 0, 40, 30, 0x7777);
  span_track_erase(&cv, &tr, 0);
  TEST_ASSERT_EQUAL_HEX16(0, cv.px[6 * 40 + 10]);
  TEST_ASSERT_EQUAL_HEX16(0, cv.px[6 * 40 + 2]);
  TEST_ASSERT_EQUAL_HEX16(0, cv.px[6 * 40 + 26]);
  TEST_ASSERT_EQUAL_HEX16(0x7777, cv.px[6 * 40 + 27]);
  TEST_ASSERT_EQUAL_HEX16(0x7777, cv.px[6 * 40 + 1]);
}

void test_what_is_erased_is_forgotten_so_a_third_erase_does_nothing(void) {
  span_track_erase(&cv, &tr, 0);
  const XPoint t[3] = {{5, 4}, {15, 4}, {10, 12}};
  span_track_note(&tr, t, 3);
  canvas_fill_rect(&cv, 0, 0, 40, 30, 0x7777);
  span_track_erase(&cv, &tr, 0);
  canvas_clear_dirty(&cv);
  span_track_erase(&cv, &tr, 0);
  int x0, x1;
  TEST_ASSERT_FALSE(canvas_dirty_row(&cv, 6, &x0, &x1));
}

void test_polygons_hanging_off_the_canvas_are_clipped_and_extreme_ones_are_safe(void) {
  span_track_erase(&cv, &tr, 0);
  const XPoint off[4] = {{-50, -50}, {90, -40}, {80, 70}, {-60, 60}};
  span_track_note(&tr, off, 4);
  const XPoint far[3] = {{30000, 30000}, {-30000, 20}, {5, -30000}};
  span_track_note(&tr, far, 3);
  span_track_note(&tr, NULL, 0);
  canvas_fill_rect(&cv, 0, 0, 40, 30, 0x7777);
  span_track_erase(&cv, &tr, 0);
  TEST_ASSERT_EQUAL_INT(0, lit());
}

void test_a_canvas_taller_than_the_tracker_is_always_fully_cleared(void) {
  Canvas tall;
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&tall, 8, SPAN_TRACK_ROWS + 20, malloc));
  SpanTrack t2;
  span_track_reset(&t2);
  canvas_clear(&tall, 0x3333);
  span_track_erase(&tall, &t2, 0);
  span_track_erase(&tall, &t2, 0);
  for (int i = 0; i < tall.w * tall.h; i++) TEST_ASSERT_EQUAL_HEX16(0, tall.px[i]);
  canvas_free(&tall);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_the_first_erase_clears_the_whole_canvas_to_the_colour);
  RUN_TEST(test_a_later_erase_clears_only_the_rows_and_columns_that_were_noted);
  RUN_TEST(test_noted_polygons_union_their_columns_on_a_shared_row);
  RUN_TEST(test_what_is_erased_is_forgotten_so_a_third_erase_does_nothing);
  RUN_TEST(test_polygons_hanging_off_the_canvas_are_clipped_and_extreme_ones_are_safe);
  RUN_TEST(test_a_canvas_taller_than_the_tracker_is_always_fully_cleared);
  return UNITY_END();
}
