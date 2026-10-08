#include <limits.h>
#include <math.h>
#include <stdio.h>
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

void test_fill_rect_huge_size_from_nonzero_origin_clips_not_wraps(void) {
  canvas_fill_rect(&c, 5, 0, INT_MAX, 1, C1);
  TEST_ASSERT_EQUAL_INT(3, count_set());
  TEST_ASSERT_EQUAL_HEX16(C1, at(7, 0));
  canvas_clear(&c, 0);
  canvas_fill_rect(&c, 0, 5, 1, INT_MAX, C1);
  TEST_ASSERT_EQUAL_INT(3, count_set());
  TEST_ASSERT_EQUAL_HEX16(C1, at(0, 7));
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

void test_polygon_keeps_every_crossing_when_a_scanline_has_over_64(void) {
  enum { TEETH = 40 };
  Canvas wide;
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&wide, 200, 8, malloc));
  canvas_clear(&wide, 0);
  int xy[TEETH * 4 * 2];
  int n = 0;
  for (int i = 0; i < TEETH; i++) {
    xy[n++] = 4 * i, xy[n++] = 6;
    xy[n++] = 4 * i, xy[n++] = 0;
    xy[n++] = 4 * i + 2, xy[n++] = 0;
    xy[n++] = 4 * i + 2, xy[n++] = 6;
  }
  canvas_fill_polygon(&wide, xy, n / 2, C1);
  int last_tooth = 4 * (TEETH - 1);
  TEST_ASSERT_EQUAL_HEX16(C1, wide.px[3 * wide.w + last_tooth + 1]);
  TEST_ASSERT_EQUAL_HEX16(0, wide.px[3 * wide.w + last_tooth - 1]);
  canvas_free(&wide);
}

/* The original scanline fill with a plain insertion sort, kept as a reference
 * so changes to the sort can be checked for identical output. */
static void reference_fill_polygon(Canvas *cv, const int *xy, int n,
                                   uint16_t color) {
  if (n < 3) return;
  long ymin = xy[1], ymax = xy[1];
  for (int i = 1; i < n; i++) {
    if (xy[2 * i + 1] < ymin) ymin = xy[2 * i + 1];
    if (xy[2 * i + 1] > ymax) ymax = xy[2 * i + 1];
  }
  if (ymin < 0) ymin = 0;
  if (ymax >= cv->h) ymax = cv->h - 1;
  double *xs = malloc((size_t)n * sizeof(double));
  for (long yy = ymin; yy <= ymax; yy++) {
    int nx = 0;
    double sy = yy + 0.5;
    for (int i = 0, j = n - 1; i < n; j = i++) {
      double yi = xy[2 * i + 1], yj = xy[2 * j + 1];
      if ((yi <= sy && yj > sy) || (yj <= sy && yi > sy)) {
        double t = (sy - yi) / (yj - yi);
        xs[nx++] = xy[2 * i] + t * (xy[2 * j] - xy[2 * i]);
      }
    }
    for (int a = 1; a < nx; a++)
      for (int b = a; b > 0 && xs[b - 1] > xs[b]; b--) {
        double tmp = xs[b];
        xs[b] = xs[b - 1];
        xs[b - 1] = tmp;
      }
    for (int k = 0; k + 1 < nx; k += 2) {
      long xa = (long)ceil(xs[k] - 0.5), xb = (long)ceil(xs[k + 1] - 0.5) - 1;
      if (xa < 0) xa = 0;
      if (xb >= cv->w) xb = cv->w - 1;
      for (long x = xa; x <= xb; x++) cv->px[yy * cv->w + x] = color;
    }
  }
  free(xs);
}

void test_polygon_fill_matches_the_reference_on_random_polygons(void) {
  enum { W = 120, H = 100, MAXN = 1000 };
  static int xy[MAXN * 2];
  Canvas got, want;
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&got, W, H, malloc));
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&want, W, H, malloc));
  unsigned long seed = 12345;
  const int sizes[] = {3, 4, 5, 7, 12, 30, 64, 65, 100, 250, 500, 1000};
  for (int round = 0; round < 3; round++) {
    for (unsigned s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
      int n = sizes[s];
      for (int i = 0; i < n * 2; i += 2) {
        seed = seed * 1103515245UL + 12345UL;
        xy[i] = (int)((seed >> 8) % (W + 60)) - 30;
        seed = seed * 1103515245UL + 12345UL;
        xy[i + 1] = (int)((seed >> 8) % (H + 60)) - 30;
      }
      canvas_clear(&got, 0);
      canvas_clear(&want, 0);
      canvas_fill_polygon(&got, xy, n, C1);
      reference_fill_polygon(&want, xy, n, C1);
      for (int p = 0; p < W * H; p++)
        if (got.px[p] != want.px[p]) {
          char msg[64];
          snprintf(msg, sizeof(msg), "n=%d first difference at pixel %d", n, p);
          TEST_FAIL_MESSAGE(msg);
        }
    }
  }
  canvas_free(&got);
  canvas_free(&want);
}

static void fill_pattern(void) {
  for (int i = 0; i < c.w * c.h; i++) c.px[i] = (uint16_t)(i + 1);
}

void test_copy_then_paste_restores_a_rect_after_it_is_drawn_over(void) {
  fill_pattern();
  uint16_t saved[3 * 2];
  canvas_copy_rect(&c, 2, 3, 3, 2, saved);
  canvas_fill_rect(&c, 2, 3, 3, 2, C1);
  TEST_ASSERT_EQUAL_HEX16(C1, at(3, 4));
  canvas_paste_rect(&c, 2, 3, 3, 2, saved);
  TEST_ASSERT_EQUAL_HEX16(3 * 8 + 2 + 1, at(2, 3));
  TEST_ASSERT_EQUAL_HEX16(4 * 8 + 4 + 1, at(4, 4));
}

void test_paste_changes_nothing_outside_the_rect(void) {
  fill_pattern();
  uint16_t saved[2 * 2] = {0, 0, 0, 0};
  canvas_paste_rect(&c, 1, 1, 2, 2, saved);
  TEST_ASSERT_EQUAL_HEX16(1, at(0, 0));
  TEST_ASSERT_EQUAL_HEX16(1 * 8 + 3 + 1, at(3, 1));
  TEST_ASSERT_EQUAL_HEX16(3 * 8 + 1 + 1, at(1, 3));
  TEST_ASSERT_EQUAL_HEX16(0, at(2, 2));
}

void test_a_rect_hanging_off_the_canvas_round_trips_its_visible_part(void) {
  fill_pattern();
  uint16_t saved[4 * 4];
  for (int i = 0; i < 16; i++) saved[i] = 0xAAAA;
  canvas_copy_rect(&c, 6, 6, 4, 4, saved);
  TEST_ASSERT_EQUAL_HEX16(6 * 8 + 6 + 1, saved[0]);
  TEST_ASSERT_EQUAL_HEX16(7 * 8 + 7 + 1, saved[1 * 4 + 1]);
  TEST_ASSERT_EQUAL_HEX16(0xAAAA, saved[2]);
  canvas_fill_rect(&c, 6, 6, 4, 4, C1);
  canvas_paste_rect(&c, 6, 6, 4, 4, saved);
  TEST_ASSERT_EQUAL_HEX16(6 * 8 + 6 + 1, at(6, 6));
  TEST_ASSERT_EQUAL_HEX16(7 * 8 + 7 + 1, at(7, 7));
}

void test_a_rect_hanging_off_the_top_left_copies_from_the_right_offset(void) {
  fill_pattern();
  uint16_t saved[4 * 4];
  canvas_copy_rect(&c, -2, -1, 4, 4, saved);
  TEST_ASSERT_EQUAL_HEX16(0 * 8 + 0 + 1, saved[1 * 4 + 2]);
  TEST_ASSERT_EQUAL_HEX16(2 * 8 + 1 + 1, saved[3 * 4 + 3]);
  canvas_fill_rect(&c, 0, 0, 3, 3, C1);
  canvas_paste_rect(&c, -2, -1, 4, 4, saved);
  TEST_ASSERT_EQUAL_HEX16(0 * 8 + 0 + 1, at(0, 0));
  TEST_ASSERT_EQUAL_HEX16(1 * 8 + 1 + 1, at(1, 1));
  TEST_ASSERT_EQUAL_HEX16(C1, at(2, 2));
}

void test_a_rect_wholly_off_the_canvas_is_ignored(void) {
  fill_pattern();
  uint16_t saved[2 * 2] = {0, 0, 0, 0};
  canvas_copy_rect(&c, -5, -5, 2, 2, saved);
  canvas_paste_rect(&c, 100, 100, 2, 2, saved);
  canvas_paste_rect(&c, -5, -5, 2, 2, saved);
  TEST_ASSERT_EQUAL_HEX16(1, at(0, 0));
  TEST_ASSERT_EQUAL_HEX16(0, saved[0]);
}

void test_extreme_rect_coordinates_do_not_overflow(void) {
  uint16_t saved[1] = {0};
  canvas_copy_rect(&c, INT_MAX, INT_MAX, 5, 5, saved);
  canvas_paste_rect(&c, INT_MIN, INT_MIN, INT_MAX, INT_MAX, saved);
  TEST_ASSERT_EQUAL_INT(0, count_set());
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_polygon_fill_matches_the_reference_on_random_polygons);
  RUN_TEST(test_polygon_keeps_every_crossing_when_a_scanline_has_over_64);
  RUN_TEST(test_rgb565_white_black_red);
  RUN_TEST(test_rgb565_from16_matches_8bit);
  RUN_TEST(test_init_failing_alloc_returns_minus_one);
  RUN_TEST(test_point_inside_sets_pixel);
  RUN_TEST(test_point_outside_is_ignored);
  RUN_TEST(test_fill_rect_clips_negative_origin);
  RUN_TEST(test_fill_rect_entirely_outside_changes_nothing);
  RUN_TEST(test_fill_rect_huge_size_no_overflow);
  RUN_TEST(test_fill_rect_huge_size_from_nonzero_origin_clips_not_wraps);
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
  RUN_TEST(test_copy_then_paste_restores_a_rect_after_it_is_drawn_over);
  RUN_TEST(test_paste_changes_nothing_outside_the_rect);
  RUN_TEST(test_a_rect_hanging_off_the_canvas_round_trips_its_visible_part);
  RUN_TEST(test_a_rect_hanging_off_the_top_left_copies_from_the_right_offset);
  RUN_TEST(test_a_rect_wholly_off_the_canvas_is_ignored);
  RUN_TEST(test_extreme_rect_coordinates_do_not_overflow);
  return UNITY_END();
}
