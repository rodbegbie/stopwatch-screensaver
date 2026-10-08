#include <limits.h>
#include <stdlib.h>
#include <unity.h>

#include "core/canvas.h"

#define C1 0x1234

static Canvas c;

void setUp(void) { TEST_ASSERT_EQUAL_INT(0, canvas_init(&c, 8, 8, malloc)); canvas_clear(&c, 0); }
void tearDown(void) { canvas_free(&c); }

static int count_set(void) {
  int n = 0;
  for (int i = 0; i < c.w * c.h; i++) n += c.px[i] != 0;
  return n;
}
static uint16_t at(int x, int y) { return c.px[y * c.w + x]; }

static void *failing_alloc(size_t n) { (void)n; return NULL; }

void test_rgb565_white_black_red(void) {
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, rgb565(255, 255, 255));
  TEST_ASSERT_EQUAL_HEX16(0x0000, rgb565(0, 0, 0));
  TEST_ASSERT_EQUAL_HEX16(0xF800, rgb565(255, 0, 0));
}

void test_rgb565_from16_matches_8bit(void) {
  TEST_ASSERT_EQUAL_HEX16(0xF800, rgb565_from16(0xFFFF, 0, 0));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, rgb565_from16(0xFFFF, 0xFFFF, 0xFFFF));
}

void test_init_failing_alloc_returns_minus_one(void) {
  Canvas bad;
  TEST_ASSERT_EQUAL_INT(-1, canvas_init(&bad, 8, 8, failing_alloc));
}

void test_point_inside_sets_pixel(void) {
  canvas_point(&c, 3, 4, C1);
  TEST_ASSERT_EQUAL_HEX16(C1, at(3, 4));
  TEST_ASSERT_EQUAL_INT(1, count_set());
}

void test_point_outside_is_ignored(void) {
  canvas_point(&c, -1, 0, C1);
  canvas_point(&c, 0, -1, C1);
  canvas_point(&c, 8, 0, C1);
  canvas_point(&c, 0, 8, C1);
  canvas_point(&c, INT_MAX, INT_MAX, C1);
  canvas_point(&c, INT_MIN, INT_MIN, C1);
  TEST_ASSERT_EQUAL_INT(0, count_set());
}

void test_fill_rect_clips_negative_origin(void) {
  canvas_fill_rect(&c, -5, -5, 10, 10, C1);
  TEST_ASSERT_EQUAL_INT(25, count_set());
  TEST_ASSERT_EQUAL_HEX16(C1, at(4, 4));
  TEST_ASSERT_EQUAL_HEX16(0, at(5, 5));
}

void test_fill_rect_entirely_outside_changes_nothing(void) {
  canvas_fill_rect(&c, 8, 0, 4, 4, C1);
  canvas_fill_rect(&c, -10, -10, 5, 5, C1);
  TEST_ASSERT_EQUAL_INT(0, count_set());
}

void test_fill_rect_huge_size_no_overflow(void) {
  canvas_fill_rect(&c, 0, 0, INT_MAX, INT_MAX, C1);
  TEST_ASSERT_EQUAL_INT(64, count_set());
  canvas_clear(&c, 0);
  canvas_fill_rect(&c, 3, 3, INT_MAX, INT_MAX, C1);
  TEST_ASSERT_EQUAL_INT(25, count_set());
}

void test_fill_rect_zero_or_negative_size_changes_nothing(void) {
  canvas_fill_rect(&c, 1, 1, 0, 5, C1);
  canvas_fill_rect(&c, 1, 1, 5, -3, C1);
  TEST_ASSERT_EQUAL_INT(0, count_set());
}

void test_line_horizontal_vertical_diagonal(void) {
  canvas_line(&c, 1, 2, 6, 2, C1);
  TEST_ASSERT_EQUAL_INT(6, count_set());
  TEST_ASSERT_EQUAL_HEX16(C1, at(1, 2));
  TEST_ASSERT_EQUAL_HEX16(C1, at(6, 2));
  canvas_clear(&c, 0);
  canvas_line(&c, 3, 0, 3, 7, C1);
  TEST_ASSERT_EQUAL_INT(8, count_set());
  canvas_clear(&c, 0);
  canvas_line(&c, 0, 0, 7, 7, C1);
  TEST_ASSERT_EQUAL_INT(8, count_set());
  for (int i = 0; i < 8; i++) TEST_ASSERT_EQUAL_HEX16(C1, at(i, i));
}

void test_line_with_offscreen_endpoint_draws_visible_part(void) {
  canvas_line(&c, -100, 3, 100, 3, C1);
  TEST_ASSERT_EQUAL_INT(8, count_set());
}

void test_line_far_offscreen_changes_nothing_and_terminates(void) {
  canvas_line(&c, -1000000, -1000000, -999000, 1000000, C1);
  canvas_line(&c, 1000000, 1000000, -1000000, 1000001, C1);
  TEST_ASSERT_EQUAL_INT(0, count_set());
}

void test_line_diagonal_crossing_canvas_from_far_away(void) {
  canvas_line(&c, -1000, -1000, 1000, 1000, C1);
  TEST_ASSERT_EQUAL_INT(8, count_set());
  TEST_ASSERT_EQUAL_HEX16(C1, at(0, 0));
  TEST_ASSERT_EQUAL_HEX16(C1, at(7, 7));
}

void test_ellipse_diameter_1_is_one_pixel(void) {
  canvas_fill_ellipse(&c, 2, 2, 1, 1, C1);
  TEST_ASSERT_EQUAL_INT(1, count_set());
  TEST_ASSERT_EQUAL_HEX16(C1, at(2, 2));
}

void test_ellipse_zero_size_draws_nothing(void) {
  canvas_fill_ellipse(&c, 2, 2, 0, 0, C1);
  canvas_fill_ellipse(&c, 2, 2, 0, 5, C1);
  canvas_fill_ellipse(&c, 2, 2, -3, 5, C1);
  TEST_ASSERT_EQUAL_INT(0, count_set());
}

void test_ellipse_5x5_is_symmetric(void) {
  canvas_fill_ellipse(&c, 1, 1, 5, 5, C1);
  TEST_ASSERT_TRUE(count_set() >= 17);
  TEST_ASSERT_EQUAL_HEX16(C1, at(3, 3));
  for (int y = 0; y < 5; y++)
    for (int x = 0; x < 5; x++) {
      TEST_ASSERT_EQUAL_HEX16(at(1 + x, 1 + y), at(1 + 4 - x, 1 + y));
      TEST_ASSERT_EQUAL_HEX16(at(1 + x, 1 + y), at(1 + x, 1 + 4 - y));
    }
  TEST_ASSERT_EQUAL_HEX16(0, at(1, 1));
}

void test_ellipse_partly_offscreen_clips(void) {
  canvas_fill_ellipse(&c, -2, -2, 6, 6, C1);
  TEST_ASSERT_TRUE(count_set() > 0);
  canvas_clear(&c, 0);
  canvas_fill_ellipse(&c, INT_MAX - 2, INT_MAX - 2, 5, 5, C1);
  canvas_fill_ellipse(&c, 1000000, 1000000, INT_MAX, INT_MAX, C1);
  TEST_ASSERT_EQUAL_INT(0, count_set());
}

void test_polygon_triangle_fills_interior_not_exterior(void) {
  int tri[] = {0, 0, 6, 0, 0, 6};
  canvas_fill_polygon(&c, tri, 3, C1);
  TEST_ASSERT_EQUAL_HEX16(C1, at(1, 1));
  TEST_ASSERT_EQUAL_HEX16(C1, at(2, 2));
  TEST_ASSERT_EQUAL_HEX16(0, at(6, 6));
  TEST_ASSERT_EQUAL_HEX16(0, at(5, 5));
  TEST_ASSERT_EQUAL_HEX16(0, at(7, 0));
}

void test_polygon_degenerate_changes_nothing(void) {
  int two[] = {0, 0, 5, 5};
  canvas_fill_polygon(&c, two, 2, C1);
  canvas_fill_polygon(&c, two, 0, C1);
  TEST_ASSERT_EQUAL_INT(0, count_set());
}

void test_polygon_offscreen_vertices_clip(void) {
  int big[] = {-1000000, -1000000, 1000000, -1000000, 0, 1000000};
  canvas_fill_polygon(&c, big, 3, C1);
  TEST_ASSERT_EQUAL_INT(64, count_set());
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_rgb565_white_black_red);
  RUN_TEST(test_rgb565_from16_matches_8bit);
  RUN_TEST(test_init_failing_alloc_returns_minus_one);
  RUN_TEST(test_point_inside_sets_pixel);
  RUN_TEST(test_point_outside_is_ignored);
  RUN_TEST(test_fill_rect_clips_negative_origin);
  RUN_TEST(test_fill_rect_entirely_outside_changes_nothing);
  RUN_TEST(test_fill_rect_huge_size_no_overflow);
  RUN_TEST(test_fill_rect_zero_or_negative_size_changes_nothing);
  RUN_TEST(test_line_horizontal_vertical_diagonal);
  RUN_TEST(test_line_with_offscreen_endpoint_draws_visible_part);
  RUN_TEST(test_line_far_offscreen_changes_nothing_and_terminates);
  RUN_TEST(test_line_diagonal_crossing_canvas_from_far_away);
  RUN_TEST(test_ellipse_diameter_1_is_one_pixel);
  RUN_TEST(test_ellipse_zero_size_draws_nothing);
  RUN_TEST(test_ellipse_5x5_is_symmetric);
  RUN_TEST(test_ellipse_partly_offscreen_clips);
  RUN_TEST(test_polygon_triangle_fills_interior_not_exterior);
  RUN_TEST(test_polygon_degenerate_changes_nothing);
  RUN_TEST(test_polygon_offscreen_vertices_clip);
  return UNITY_END();
}
