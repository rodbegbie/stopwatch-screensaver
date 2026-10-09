#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

/* Pixels are RGB565 with the two bytes swapped: that is the order the display
 * wants, so the push needs no byte swap. */
void test_rgb565_white_black_red(void) {
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, rgb565(255, 255, 255));
  TEST_ASSERT_EQUAL_HEX16(0x0000, rgb565(0, 0, 0));
  TEST_ASSERT_EQUAL_HEX16(0x00F8, rgb565(255, 0, 0));
  TEST_ASSERT_EQUAL_HEX16(0xE007, rgb565(0, 255, 0));
  TEST_ASSERT_EQUAL_HEX16(0x1F00, rgb565(0, 0, 255));
}

void test_rgb565_from16_matches_8bit(void) {
  TEST_ASSERT_EQUAL_HEX16(0x00F8, rgb565_from16(0xFFFF, 0, 0));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, rgb565_from16(0xFFFF, 0xFFFF, 0xFFFF));
}

void test_px_swap_converts_between_display_and_native_order(void) {
  TEST_ASSERT_EQUAL_HEX16(0xF800, px_swap(0x00F8));
  TEST_ASSERT_EQUAL_HEX16(0x3412, px_swap(0x1234));
  TEST_ASSERT_EQUAL_HEX16(0x1234, px_swap(px_swap(0x1234)));
}

void test_a_canvas_too_big_for_int16_spans_is_refused(void) {
  Canvas big;
  TEST_ASSERT_EQUAL_INT(-1, canvas_init(&big, 40000, 8, malloc));
  TEST_ASSERT_EQUAL_INT(-1, canvas_init(&big, 8, 40000, malloc));
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

/* Dirty spans: what a hack drew since the last push, per row. */
static void snapshot(uint16_t *dst) { memcpy(dst, c.px, (size_t)c.w * c.h * sizeof(uint16_t)); }

static int dirty_row_count(void) {
  int n = 0, x0, x1;
  for (int y = 0; y < c.h; y++) n += canvas_dirty_row(&c, y, &x0, &x1);
  return n;
}

static void assert_span(int y, int want0, int want1) {
  int x0 = -1, x1 = -1;
  TEST_ASSERT_TRUE_MESSAGE(canvas_dirty_row(&c, y, &x0, &x1), "row should be dirty");
  TEST_ASSERT_EQUAL_INT(want0, x0);
  TEST_ASSERT_EQUAL_INT(want1, x1);
}

static void assert_changes_marked(const uint16_t *before) {
  for (int y = 0; y < c.h; y++) {
    int x0 = 0, x1 = -1;
    bool dirty = canvas_dirty_row(&c, y, &x0, &x1);
    for (int x = 0; x < c.w; x++)
      if (c.px[y * c.w + x] != before[y * c.w + x])
        TEST_ASSERT_TRUE_MESSAGE(dirty && x >= x0 && x <= x1,
                                 "a changed pixel lies outside its row's span");
  }
}

void test_a_new_canvas_is_dirty_in_every_row_in_full(void) {
  Canvas fresh;
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&fresh, 8, 8, malloc));
  for (int y = 0; y < 8; y++) {
    int x0 = -1, x1 = -1;
    TEST_ASSERT_TRUE(canvas_dirty_row(&fresh, y, &x0, &x1));
    TEST_ASSERT_EQUAL_INT(0, x0);
    TEST_ASSERT_EQUAL_INT(7, x1);
  }
  canvas_free(&fresh);
}

void test_clear_dirty_makes_every_row_clean(void) {
  canvas_clear_dirty(&c);
  TEST_ASSERT_EQUAL_INT(0, dirty_row_count());
}

void test_clear_marks_every_row_in_full(void) {
  canvas_clear_dirty(&c);
  canvas_clear(&c, C1);
  TEST_ASSERT_EQUAL_INT(8, dirty_row_count());
  for (int y = 0; y < 8; y++) assert_span(y, 0, 7);
}

void test_point_marks_exactly_its_pixel(void) {
  canvas_clear_dirty(&c);
  canvas_point(&c, 3, 5, C1);
  TEST_ASSERT_EQUAL_INT(1, dirty_row_count());
  assert_span(5, 3, 3);
}

void test_points_in_a_row_widen_its_span(void) {
  canvas_clear_dirty(&c);
  canvas_point(&c, 6, 1, C1);
  canvas_point(&c, 2, 1, C1);
  assert_span(1, 2, 6);
}

void test_fill_rect_marks_exactly_its_rows_and_columns(void) {
  canvas_clear_dirty(&c);
  canvas_fill_rect(&c, 2, 1, 3, 2, C1);
  TEST_ASSERT_EQUAL_INT(2, dirty_row_count());
  assert_span(1, 2, 4);
  assert_span(2, 2, 4);
}

void test_drawing_that_clips_to_nothing_marks_nothing(void) {
  canvas_clear_dirty(&c);
  canvas_point(&c, -1, 0, C1);
  canvas_point(&c, 8, 0, C1);
  canvas_point(&c, 0, -1, C1);
  canvas_point(&c, 0, 8, C1);
  canvas_fill_rect(&c, -5, 0, 5, 8, C1);
  canvas_fill_rect(&c, 8, 0, 5, 8, C1);
  canvas_fill_rect(&c, 0, -5, 8, 5, C1);
  canvas_fill_rect(&c, 0, 8, 8, 5, C1);
  canvas_line(&c, -10, -10, -2, 20, C1);
  TEST_ASSERT_EQUAL_INT(0, dirty_row_count());
}

void test_first_and_last_row_and_column_are_marked(void) {
  canvas_clear_dirty(&c);
  canvas_point(&c, 0, 0, C1);
  canvas_point(&c, 7, 7, C1);
  canvas_fill_rect(&c, 5, 3, 10, 1, C1);
  assert_span(0, 0, 0);
  assert_span(3, 5, 7);
  assert_span(7, 7, 7);
}

void test_huge_coordinates_neither_overflow_nor_mark_stray_rows(void) {
  uint16_t before[64];
  fill_pattern();
  snapshot(before);
  canvas_clear_dirty(&c);
  canvas_fill_rect(&c, INT_MIN, 2, INT_MAX, 1, C1);
  TEST_ASSERT_EQUAL_INT(0, dirty_row_count());
  canvas_fill_rect(&c, 3, INT_MIN, 1, INT_MAX, C1);
  TEST_ASSERT_EQUAL_INT(0, dirty_row_count());
  canvas_fill_rect(&c, -3, 2, INT_MAX, 1, C1);
  TEST_ASSERT_EQUAL_INT(1, dirty_row_count());
  assert_span(2, 0, 7);
  assert_changes_marked(before);
  canvas_clear_dirty(&c);
  canvas_mark_dirty(&c, INT_MIN, 2, INT_MAX, 1);
  canvas_mark_dirty(&c, 3, INT_MIN, 1, INT_MAX);
  TEST_ASSERT_EQUAL_INT(0, dirty_row_count());
}

void test_line_ellipse_and_polygon_changes_are_all_marked(void) {
  uint16_t before[64];
  const int tri[] = {-3, 2, 6, -4, 10, 9};
  canvas_clear(&c, 0);
  snapshot(before);
  canvas_clear_dirty(&c);
  canvas_line(&c, -4, 1, 12, 6, C1);
  assert_changes_marked(before);
  canvas_clear(&c, 0);
  canvas_clear_dirty(&c);
  canvas_fill_ellipse(&c, -2, 1, 7, 5, C1);
  assert_changes_marked(before);
  canvas_clear(&c, 0);
  canvas_clear_dirty(&c);
  canvas_fill_polygon(&c, tri, 3, C1);
  assert_changes_marked(before);
  TEST_ASSERT_TRUE(dirty_row_count() > 0);
}

void test_paste_rect_marks_the_pasted_rows(void) {
  const uint16_t block[6] = {1, 2, 3, 4, 5, 6};
  canvas_clear_dirty(&c);
  canvas_paste_rect(&c, 1, 2, 3, 2, block);
  TEST_ASSERT_EQUAL_INT(2, dirty_row_count());
  assert_span(2, 1, 3);
  assert_span(3, 1, 3);
  canvas_clear_dirty(&c);
  canvas_paste_rect(&c, 6, 6, 3, 3, (const uint16_t[9]){1, 2, 3, 4, 5, 6, 7, 8, 9});
  TEST_ASSERT_EQUAL_INT(2, dirty_row_count());
  assert_span(6, 6, 7);
  assert_span(7, 6, 7);
}

void test_mark_dirty_clips_to_the_canvas(void) {
  canvas_clear_dirty(&c);
  canvas_mark_dirty(&c, -2, -2, 4, 4);
  TEST_ASSERT_EQUAL_INT(2, dirty_row_count());
  assert_span(0, 0, 1);
  assert_span(1, 0, 1);
  canvas_clear_dirty(&c);
  canvas_mark_dirty(&c, 20, 20, 3, 3);
  canvas_mark_dirty(&c, 0, 0, 0, 5);
  TEST_ASSERT_EQUAL_INT(0, dirty_row_count());
}

static uint32_t rng_state = 2463534242u;
static int rnd(int lo, int hi) {
  rng_state ^= rng_state << 13;
  rng_state ^= rng_state >> 17;
  rng_state ^= rng_state << 5;
  return lo + (int)(rng_state % (uint32_t)(hi - lo + 1));
}
static int coord(void) {
  switch (rnd(0, 19)) {
    case 0: return INT_MIN;
    case 1: return INT_MAX;
    default: return rnd(-20, 30);
  }
}

void test_random_primitive_calls_mark_every_pixel_they_change(void) {
  uint16_t before[64];
  uint16_t block[36];
  for (int i = 0; i < 36; i++) block[i] = (uint16_t)(0x4000 + i);
  for (int round = 0; round < 300; round++) {
    for (int kind = 0; kind < 6; kind++) {
      fill_pattern();
      snapshot(before);
      canvas_clear_dirty(&c);
      switch (kind) {
        case 0: canvas_point(&c, coord(), coord(), C1); break;
        case 1: canvas_line(&c, coord(), coord(), coord(), coord(), C1); break;
        case 2: canvas_fill_rect(&c, coord(), coord(), coord(), coord(), C1); break;
        case 3: canvas_fill_ellipse(&c, coord(), coord(), coord(), coord(), C1); break;
        case 4: {
          int xy[12], n = rnd(3, 6);
          for (int i = 0; i < 2 * n; i++) xy[i] = coord();
          canvas_fill_polygon(&c, xy, n, C1);
          break;
        }
        default:
          canvas_paste_rect(&c, rnd(-20, 30), rnd(-20, 30), rnd(1, 6), rnd(1, 6), block);
      }
      assert_changes_marked(before);
    }
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_polygon_fill_matches_the_reference_on_random_polygons);
  RUN_TEST(test_polygon_keeps_every_crossing_when_a_scanline_has_over_64);
  RUN_TEST(test_rgb565_white_black_red);
  RUN_TEST(test_rgb565_from16_matches_8bit);
  RUN_TEST(test_px_swap_converts_between_display_and_native_order);
  RUN_TEST(test_a_canvas_too_big_for_int16_spans_is_refused);
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
  RUN_TEST(test_a_new_canvas_is_dirty_in_every_row_in_full);
  RUN_TEST(test_clear_dirty_makes_every_row_clean);
  RUN_TEST(test_clear_marks_every_row_in_full);
  RUN_TEST(test_point_marks_exactly_its_pixel);
  RUN_TEST(test_points_in_a_row_widen_its_span);
  RUN_TEST(test_fill_rect_marks_exactly_its_rows_and_columns);
  RUN_TEST(test_drawing_that_clips_to_nothing_marks_nothing);
  RUN_TEST(test_first_and_last_row_and_column_are_marked);
  RUN_TEST(test_huge_coordinates_neither_overflow_nor_mark_stray_rows);
  RUN_TEST(test_line_ellipse_and_polygon_changes_are_all_marked);
  RUN_TEST(test_paste_rect_marks_the_pasted_rows);
  RUN_TEST(test_mark_dirty_clips_to_the_canvas);
  RUN_TEST(test_random_primitive_calls_mark_every_pixel_they_change);
  return UNITY_END();
}
