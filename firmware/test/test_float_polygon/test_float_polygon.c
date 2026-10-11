#include <sanitizer/allocator_interface.h>
#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include "core/canvas.h"
#include "hacks/float_polygon.h"
#include "x11shim/xshim.h"

static Canvas cv, ref;
static size_t live_before;

void setUp(void) {
  live_before = __sanitizer_get_current_allocated_bytes();
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&cv, 64, 48, malloc));
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&ref, 64, 48, malloc));
  canvas_clear_dirty(&cv);
}
void tearDown(void) {
  canvas_free(&cv);
  canvas_free(&ref);
  TEST_ASSERT_EQUAL_UINT64_MESSAGE(live_before,
                                   __sanitizer_get_current_allocated_bytes(),
                                   "test leaked memory");
}

static int lit(const Canvas *c) {
  int n = 0;
  for (int i = 0; i < c->w * c->h; i++) n += c->px[i] != 0;
  return n;
}

/* The shim's own fill is the reference. It takes crossings in double, so on a
 * row where an edge crosses at exactly a half-pixel the two can round to
 * different pixels: allow that, and nothing more. */
static void check_matches_the_shim(const XPoint *pts, int n, int allowed) {
  canvas_clear(&cv, 0);
  canvas_clear(&ref, 0);
  int xy[1024];
  for (int i = 0; i < n; i++) {
    xy[2 * i] = pts[i].x;
    xy[2 * i + 1] = pts[i].y;
  }
  canvas_fill_polygon(&ref, xy, n, 0xFFFF);
  float_fill_polygon(&cv, pts, n, 0xFFFF);
  int differ = 0;
  for (int i = 0; i < cv.w * cv.h; i++) differ += (cv.px[i] != 0) != (ref.px[i] != 0);
  TEST_ASSERT_TRUE_MESSAGE(differ <= allowed, "more pixels differ from the shim's fill than rounding allows");
}

void test_a_rectangle_and_a_triangle_fill_the_same_pixels_as_the_shim(void) {
  const XPoint rect[4] = {{5, 4}, {40, 4}, {40, 30}, {5, 30}};
  check_matches_the_shim(rect, 4, 0);
  TEST_ASSERT_EQUAL_INT(35 * 26, lit(&cv));
  const XPoint tri[3] = {{10, 40}, {50, 44}, {30, 5}};
  check_matches_the_shim(tri, 3, 4);
}

void test_a_pentagram_leaves_its_middle_empty(void) {
  const XPoint star[5] = {{32, 2}, {42, 44}, {4, 18}, {60, 18}, {22, 44}};
  float_fill_polygon(&cv, star, 5, 0xFFFF);
  TEST_ASSERT_EQUAL_HEX16(0, cv.px[22 * 64 + 32]);
  TEST_ASSERT_TRUE(cv.px[10 * 64 + 32] != 0);
  check_matches_the_shim(star, 5, 6);
}

void test_a_ring_made_of_an_outline_and_a_smaller_copy_fills_only_between_them(void) {
  /* Starfish's polygon: the new outline, then the old one, so the area inside
   * both cancels out. */
  XPoint ring[16];
  for (int i = 0; i < 8; i++) {
    const int a = i * 45;
    const float c = a == 0 ? 1 : a == 45 ? 0.7071f : a == 90 ? 0 : a == 135 ? -0.7071f : a == 180 ? -1 : a == 225 ? -0.7071f : a == 270 ? 0 : 0.7071f;
    const float s = a == 0 ? 0 : a == 45 ? 0.7071f : a == 90 ? 1 : a == 135 ? 0.7071f : a == 180 ? 0 : a == 225 ? -0.7071f : a == 270 ? -1 : -0.7071f;
    ring[i] = (XPoint){(short)(32 + 28 * c), (short)(24 + 22 * s)};
    ring[8 + i] = (XPoint){(short)(32 + 12 * c), (short)(24 + 9 * s)};
  }
  float_fill_polygon(&cv, ring, 16, 0xFFFF);
  TEST_ASSERT_EQUAL_HEX16(0, cv.px[24 * 64 + 32]);
  TEST_ASSERT_TRUE(cv.px[24 * 64 + 32 + 20] != 0);
  check_matches_the_shim(ring, 16, 12);
}

void test_a_polygon_hanging_off_the_canvas_is_clipped_and_marks_only_what_it_drew(void) {
  const XPoint big[4] = {{-20, -20}, {90, -10}, {85, 70}, {-15, 60}};
  float_fill_polygon(&cv, big, 4, 0xFFFF);
  TEST_ASSERT_EQUAL_INT(64 * 48, lit(&cv));
  const XPoint small[3] = {{60, 40}, {200, 41}, {62, 300}};
  canvas_clear(&cv, 0);
  float_fill_polygon(&cv, small, 3, 0xFFFF);
  TEST_ASSERT_TRUE(lit(&cv) > 0);
  canvas_clear(&cv, 0);
  canvas_clear_dirty(&cv);
  const XPoint tiny[3] = {{10, 10}, {14, 10}, {12, 14}};
  float_fill_polygon(&cv, tiny, 3, 0xFFFF);
  int x0, x1;
  TEST_ASSERT_TRUE(canvas_dirty_row(&cv, 11, &x0, &x1));
  TEST_ASSERT_FALSE(canvas_dirty_row(&cv, 30, &x0, &x1));
}

void test_an_edge_that_starts_above_the_canvas_enters_at_the_right_column(void) {
  /* The long edge from (10, -100) to (60, 40) is already at x = 46 on row 0. */
  const XPoint tri[3] = {{10, -100}, {60, 40}, {5, 45}};
  check_matches_the_shim(tri, 3, 12);
  TEST_ASSERT_TRUE(cv.px[0 * 64 + 30] != 0);
  TEST_ASSERT_EQUAL_HEX16(0, cv.px[0 * 64 + 50]);
}

void test_a_translucent_fill_mixes_with_what_is_under_it(void) {
  canvas_clear(&cv, px_swap(0xFFFF));
  const XPoint sq[4] = {{4, 4}, {20, 4}, {20, 20}, {4, 20}};
  float_fill_polygon_alpha(&cv, sq, 4, px_swap(0x0000), 16);
  /* Half of white (31, 63, 31) is (15, 31, 15). */
  TEST_ASSERT_EQUAL_HEX16(px_swap((15 << 11) | (31 << 5) | 15), cv.px[10 * 64 + 10]);
  TEST_ASSERT_EQUAL_HEX16(px_swap(0xFFFF), cv.px[30 * 64 + 40]);
}

void test_the_new_colour_counts_alpha_parts_in_32_and_the_old_the_rest(void) {
  const XPoint sq[4] = {{4, 4}, {20, 4}, {20, 20}, {4, 20}};
  /* Red (31, 0, 0) at 8/32 over green (0, 63, 0) is (7, 47, 0). */
  canvas_clear(&cv, px_swap(63 << 5));
  float_fill_polygon_alpha(&cv, sq, 4, px_swap(31 << 11), 8);
  TEST_ASSERT_EQUAL_HEX16(px_swap((7 << 11) | (47 << 5)), cv.px[10 * 64 + 10]);
  /* And at 24/32 it is (23, 15, 0). */
  canvas_clear(&cv, px_swap(63 << 5));
  float_fill_polygon_alpha(&cv, sq, 4, px_swap(31 << 11), 24);
  TEST_ASSERT_EQUAL_HEX16(px_swap((23 << 11) | (15 << 5)), cv.px[10 * 64 + 10]);
}

void test_a_translucent_fill_covers_the_pixels_an_opaque_one_does(void) {
  const XPoint star[5] = {{32, 2}, {42, 44}, {4, 18}, {60, 18}, {22, 44}};
  canvas_clear(&cv, 0);
  float_fill_polygon(&cv, star, 5, px_swap(0xFFFF));
  Canvas other;
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&other, 64, 48, malloc));
  canvas_clear(&other, 0);
  float_fill_polygon_alpha(&other, star, 5, px_swap(0xFFFF), 23);
  for (int i = 0; i < 64 * 48; i++)
    TEST_ASSERT_EQUAL_INT_MESSAGE(cv.px[i] != 0, other.px[i] != 0, "coverage");
  canvas_free(&other);
}

void test_the_ends_of_the_alpha_range_are_nothing_and_opaque(void) {
  const XPoint sq[4] = {{4, 4}, {20, 4}, {20, 20}, {4, 20}};
  canvas_clear(&cv, px_swap(0x1234));
  float_fill_polygon_alpha(&cv, sq, 4, px_swap(0xFFFF), 0);
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x1234), cv.px[10 * 64 + 10]);
  float_fill_polygon_alpha(&cv, sq, 4, px_swap(0xFFFF), 32);
  TEST_ASSERT_EQUAL_HEX16(px_swap(0xFFFF), cv.px[10 * 64 + 10]);
}

void test_a_translucent_fill_marks_what_it_drew_dirty(void) {
  canvas_clear_dirty(&cv);
  const XPoint tri[3] = {{10, 10}, {20, 10}, {15, 20}};
  float_fill_polygon_alpha(&cv, tri, 3, px_swap(0xF800), 16);
  int x0, x1;
  TEST_ASSERT_TRUE(canvas_dirty_row(&cv, 12, &x0, &x1));
  TEST_ASSERT_TRUE(x0 >= 10 && x1 <= 20);
  TEST_ASSERT_FALSE(canvas_dirty_row(&cv, 30, &x0, &x1));
}

void test_degenerate_input_draws_nothing(void) {
  const XPoint line[2] = {{1, 1}, {30, 30}};
  float_fill_polygon(&cv, line, 2, 0xFFFF);
  const XPoint flat[3] = {{5, 5}, {20, 5}, {40, 5}};
  float_fill_polygon(&cv, flat, 3, 0xFFFF);
  TEST_ASSERT_EQUAL_INT(0, lit(&cv));
  float_fill_polygon(&cv, NULL, 0, 0xFFFF);
}

void test_a_polygon_of_hundreds_of_vertices_fills_like_the_shim(void) {
  XPoint pts[300];
  for (int i = 0; i < 300; i++) {
    const float a = (float)i * 6.2831853f / 300.0f;
    const float r = (i & 1) ? 18.0f : 22.0f;
    pts[i] = (XPoint){(short)(32 + r * __builtin_cosf(a)), (short)(24 + r * __builtin_sinf(a))};
  }
  check_matches_the_shim(pts, 300, 40);
}

/* qsort allocates once, the first time it runs, which would read as a leak in
 * whichever test happens to be first. */
static void warm_up_qsort(void) {
  Canvas c;
  canvas_init(&c, 8, 8, malloc);
  const XPoint t[3] = {{1, 1}, {6, 2}, {3, 6}};
  float_fill_polygon(&c, t, 3, 1);
  canvas_free(&c);
}

void test_filling_again_and_again_does_not_grow_memory(void) {
  const XPoint t[3] = {{1, 1}, {40, 2}, {3, 30}};
  float_fill_polygon(&cv, t, 3, 1);
  const size_t before = __sanitizer_get_current_allocated_bytes();
  for (int i = 0; i < 200; i++) float_fill_polygon(&cv, t, 3, 1);
  XPoint many[200];
  for (int i = 0; i < 200; i++) many[i] = (XPoint){(short)(i % 60), (short)(i % 40)};
  for (int i = 0; i < 20; i++) float_fill_polygon(&cv, many, 200, 1);
  TEST_ASSERT_EQUAL_UINT64(before, __sanitizer_get_current_allocated_bytes());
}

int main(void) {
  warm_up_qsort();
  UNITY_BEGIN();
  RUN_TEST(test_a_rectangle_and_a_triangle_fill_the_same_pixels_as_the_shim);
  RUN_TEST(test_a_pentagram_leaves_its_middle_empty);
  RUN_TEST(test_a_ring_made_of_an_outline_and_a_smaller_copy_fills_only_between_them);
  RUN_TEST(test_a_polygon_hanging_off_the_canvas_is_clipped_and_marks_only_what_it_drew);
  RUN_TEST(test_an_edge_that_starts_above_the_canvas_enters_at_the_right_column);
  RUN_TEST(test_a_translucent_fill_mixes_with_what_is_under_it);
  RUN_TEST(test_the_new_colour_counts_alpha_parts_in_32_and_the_old_the_rest);
  RUN_TEST(test_a_translucent_fill_covers_the_pixels_an_opaque_one_does);
  RUN_TEST(test_the_ends_of_the_alpha_range_are_nothing_and_opaque);
  RUN_TEST(test_a_translucent_fill_marks_what_it_drew_dirty);
  RUN_TEST(test_degenerate_input_draws_nothing);
  RUN_TEST(test_a_polygon_of_hundreds_of_vertices_fills_like_the_shim);
  RUN_TEST(test_filling_again_and_again_does_not_grow_memory);
  return UNITY_END();
}
