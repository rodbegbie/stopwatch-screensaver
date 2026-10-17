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

/* The canvas_line this repo had before the unclipped path stopped converting
 * its endpoints to double, copied from main: the reference every line below is
 * compared against. Compiled with the test's flags, so a clip that rounds
 * differently under another compiler setting (FMA contraction) does so in the
 * reference too, which a pinned hash could not allow for. */
/* Liang-Barsky clip of the segment to [0,w-1]x[0,h-1]; false if outside. */
static int reference_clip_line(const Canvas *c, double *x0, double *y0, double *x1,
                     double *y1) {
  double dx = *x1 - *x0, dy = *y1 - *y0, t0 = 0, t1 = 1;
  double p[4] = {-dx, dx, -dy, dy};
  double q[4] = {*x0, (c->w - 1) - *x0, *y0, (c->h - 1) - *y0};
  for (int i = 0; i < 4; i++) {
    if (p[i] == 0) {
      if (q[i] < 0) return 0;
    } else {
      double t = q[i] / p[i];
      if (p[i] < 0) {
        if (t > t1) return 0;
        if (t > t0) t0 = t;
      } else {
        if (t < t0) return 0;
        if (t < t1) t1 = t;
      }
    }
  }
  double ax = *x0, ay = *y0;
  *x0 = ax + t0 * dx;
  *y0 = ay + t0 * dy;
  *x1 = ax + t1 * dx;
  *y1 = ay + t1 * dy;
  return 1;
}

static void reference_canvas_line(Canvas *c, int x0, int y0, int x1, int y1, uint16_t color) {
  double ax = x0, ay = y0, bx = x1, by = y1;
  int clipped = x0 < 0 || y0 < 0 || x0 >= c->w || y0 >= c->h || x1 < 0 ||
                y1 < 0 || x1 >= c->w || y1 >= c->h;
  if (clipped) {
    if (!reference_clip_line(c, &ax, &ay, &bx, &by)) return;
    ax = floor(ax + 0.5);
    ay = floor(ay + 0.5);
    bx = floor(bx + 0.5);
    by = floor(by + 0.5);
  }
  int ix0 = (int)ax, iy0 = (int)ay, ix1 = (int)bx, iy1 = (int)by;
  int dx = abs(ix1 - ix0), sx = ix0 < ix1 ? 1 : -1;
  int dy = -abs(iy1 - iy0), sy = iy0 < iy1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    canvas_point(c, ix0, iy0, color);
    if (ix0 == ix1 && iy0 == iy1) break;
    int e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      ix0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      iy0 += sy;
    }
  }
}


/* Draws many random lines, each alone on a cleared canvas, with canvas_line and
 * with the reference, and counts the lines whose pixels differ. Drawn together,
 * later lines would cover the pixels where an earlier one differs. `margin` is
 * how far past the canvas an endpoint can fall (0 keeps every line inside, so
 * none is clipped). */
static int lines_that_differ_from_the_reference(int margin, int count) {
  Canvas a, b;
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&a, 100, 100, malloc));
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&b, 100, 100, malloc));
  srandom(1);
  int differing = 0;
  for (int i = 0; i < count; i++) {
    canvas_clear(&a, 0);
    canvas_clear(&b, 0);
    int xy[4];
    for (int k = 0; k < 4; k++) xy[k] = (int)(random() % (100 + 2 * margin)) - margin;
    canvas_line(&a, xy[0], xy[1], xy[2], xy[3], 0xFFFF);
    reference_canvas_line(&b, xy[0], xy[1], xy[2], xy[3], 0xFFFF);
    differing += memcmp(a.px, b.px, sizeof(uint16_t) * 100 * 100) != 0;
  }
  canvas_free(&a);
  canvas_free(&b);
  return differing;
}

void test_lines_inside_the_canvas_match_the_reference(void) {
  TEST_ASSERT_EQUAL_INT(0, lines_that_differ_from_the_reference(0, 20000));
}

void test_lines_running_off_the_canvas_match_the_reference(void) {
  TEST_ASSERT_EQUAL_INT(0, lines_that_differ_from_the_reference(60, 20000));
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

/* The original double-precision fill, kept as a reference: the shim now works
 * the rows out in integers (the S3 does double in software). */
static void reference_fill_ellipse(Canvas *cv, int x, int y, int w, int h,
                                   uint16_t color) {
  if (w <= 0 || h <= 0) return;
  double cx = x + (w - 1) / 2.0, cy = y + (h - 1) / 2.0;
  double rx = w / 2.0, ry = h / 2.0;
  long y0 = y < 0 ? 0 : y, y1 = (long)y + h - 1;
  if (y1 >= cv->h) y1 = cv->h - 1;
  for (long yy = y0; yy <= y1; yy++) {
    double ny = (yy - cy) / ry;
    double t = 1.0 - ny * ny;
    if (t < 0) continue;
    double half = rx * sqrt(t);
    long xa = (long)ceil(cx - half), xb = (long)floor(cx + half);
    if (xa < 0) xa = 0;
    if (xb >= cv->w) xb = cv->w - 1;
    for (long xx = xa; xx <= xb; xx++) cv->px[yy * cv->w + xx] = color;
  }
}

static int ellipse_differences(Canvas *got, Canvas *want, int x, int y, int w, int h) {
  canvas_clear(got, 0);
  canvas_clear(want, 0);
  canvas_fill_ellipse(got, x, y, w, h, C1);
  reference_fill_ellipse(want, x, y, w, h, C1);
  int differing = 0;
  for (int p = 0; p < got->w * got->h; p++) differing += got->px[p] != want->px[p];
  return differing;
}

void test_ellipse_fill_matches_the_double_reference_for_every_small_size(void) {
  Canvas got, want;
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&got, 70, 70, malloc));
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&want, 70, 70, malloc));
  int wrong = 0, first_w = 0, first_h = 0;
  for (int w = 1; w <= 48; w++)
    for (int h = 1; h <= 48; h++)
      for (int off = -3; off <= 3; off += 3) {
        int d = ellipse_differences(&got, &want, 10 + off, 12 - off, w, h);
        if (d && !wrong) first_w = w, first_h = h;
        wrong += d != 0;
      }
  char msg[80];
  snprintf(msg, sizeof msg, "%d placements differ, first %dx%d", wrong, first_w, first_h);
  TEST_ASSERT_EQUAL_INT_MESSAGE(0, wrong, msg);
  canvas_free(&got);
  canvas_free(&want);
}

void test_ellipse_fill_matches_the_double_reference_for_big_and_clipped_sizes(void) {
  Canvas got, want;
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&got, 120, 90, malloc));
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&want, 120, 90, malloc));
  const int sizes[][2] = {{100, 100}, {101, 99}, {300, 40}, {40, 300}, {466, 466},
                          {467, 465}, {1000, 700}, {2, 1000}, {1000, 2}, {255, 256}};
  const int origins[][2] = {{0, 0}, {-50, -30}, {-400, -5}, {60, 45}, {-7, 80}, {119, 89}};
  int wrong = 0;
  for (unsigned s = 0; s < sizeof sizes / sizeof sizes[0]; s++)
    for (unsigned o = 0; o < sizeof origins / sizeof origins[0]; o++)
      wrong += ellipse_differences(&got, &want, origins[o][0], origins[o][1],
                                   sizes[s][0], sizes[s][1]) != 0;
  TEST_ASSERT_EQUAL_INT(0, wrong);
  canvas_free(&got);
  canvas_free(&want);
}

/* The definition itself, in whole numbers: a pixel is in when its centre is
 * inside the ellipse. Doubled distances from the centre clear the halves. The
 * double reference above can be wrong at an exact boundary, so this checks the
 * rows against the arithmetic rather than against another float calculation. */
static int pixel_is_in_ellipse(int64_t px, int64_t py, int64_t x, int64_t y, int64_t w, int64_t h) {
  const int64_t dx2 = 2 * (px - x) - w + 1, dy2 = 2 * (py - y) - h + 1;
  return dx2 * dx2 * h * h + dy2 * dy2 * w * w <= w * w * h * h;
}

void test_ellipse_fill_is_exact_on_random_big_sizes(void) {
  enum { CW = 3200, CH = 4 };
  Canvas big;
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&big, CW, CH, malloc));
  unsigned long seed = 99;
  int wrong = 0;
  for (int trial = 0; trial < 4000; trial++) {
    seed = seed * 1103515245UL + 12345UL;
    const int w = 1 + (int)((seed >> 8) % 3000);
    seed = seed * 1103515245UL + 12345UL;
    const int h = 1 + (int)((seed >> 8) % 3000);
    seed = seed * 1103515245UL + 12345UL;
    const int y = -(int)((seed >> 8) % (unsigned)h); /* which rows show */
    seed = seed * 1103515245UL + 12345UL;
    const int x = (int)((seed >> 8) % 100) - 50;
    canvas_clear(&big, 0);
    canvas_fill_ellipse(&big, x, y, w, h, C1);
    for (int row = 0; row < CH && !wrong; row++)
      for (int px = 0; px < CW; px++)
        if ((big.px[row * CW + px] == C1) != pixel_is_in_ellipse(px, row, x, y, w, h)) {
          wrong = trial + 1;
          break;
        }
  }
  char msg[48];
  snprintf(msg, sizeof msg, "trial %d is not exact", wrong);
  TEST_ASSERT_EQUAL_INT_MESSAGE(0, wrong, msg);
  canvas_free(&big);
}

/* The float square root that starts each row's search is a little off at some
 * sizes: found by running the same float arithmetic over every size to 400,
 * these come out one too big (220 by 21, 264 by 23, 312 by 25) or one too
 * small (318 by 181). The correction loops must still land on the exact row. */
void test_ellipse_fill_is_exact_where_the_float_estimate_is_off(void) {
  const int sizes[][2] = {{220, 21}, {264, 23}, {312, 25}, {318, 181}, {21, 220}, {181, 318}};
  Canvas big;
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&big, 330, 330, malloc));
  int wrong = 0;
  for (unsigned s = 0; s < sizeof sizes / sizeof sizes[0]; s++) {
    const int w = sizes[s][0], h = sizes[s][1];
    canvas_clear(&big, 0);
    canvas_fill_ellipse(&big, 3, 2, w, h, C1);
    for (int py = 0; py < big.h; py++)
      for (int px = 0; px < big.w; px++)
        wrong += (big.px[py * big.w + px] == C1) != pixel_is_in_ellipse(px, py, 3, 2, w, h);
  }
  TEST_ASSERT_EQUAL_INT(0, wrong);
  canvas_free(&big);
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

/* The fill rule in whole numbers, one pixel at a time: a pixel is in when an odd
 * number of edges cross its row's centre line (y + 0.5) at or left of its
 * centre (x + 0.5). An edge from (xi, yi) to (xj, yj) crosses at
 * x = xlo + (2k + 1) * dx / (2 * dy), with k the row's offset from the edge's
 * top; comparing that to px + 0.5 and clearing the fractions gives one integer
 * test. Slow (every pixel looks at every edge) and independent of the fill. */
static int pixel_is_in_polygon(const int *xy, int n, int px, int py) {
  int crossings = 0;
  for (int i = 0, j = n - 1; i < n; j = i++) {
    int64_t xlo = xy[2 * i], ylo = xy[2 * i + 1], xhi = xy[2 * j], yhi = xy[2 * j + 1];
    if (ylo > yhi) {
      int64_t t;
      t = xlo, xlo = xhi, xhi = t;
      t = ylo, ylo = yhi, yhi = t;
    }
    if (py < ylo || py >= yhi) continue;
    const int64_t dx = xhi - xlo, dy = yhi - ylo, k = py - ylo;
    crossings += (2 * k + 1) * dx + 2 * dy * (xlo - px) - dy <= 0;
  }
  return crossings & 1;
}

static int polygon_differences(const int *xy, int n, Canvas *got) {
  canvas_clear(got, 0);
  canvas_fill_polygon(got, xy, n, C1);
  int differing = 0;
  for (int py = 0; py < got->h; py++)
    for (int px = 0; px < got->w; px++)
      differing += (got->px[py * got->w + px] == C1) != pixel_is_in_polygon(xy, n, px, py);
  return differing;
}

void test_polygon_fill_is_exact_on_random_polygons(void) {
  enum { W = 90, H = 70, MAXN = 1000 };
  static int xy[MAXN * 2];
  Canvas got;
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&got, W, H, malloc));
  unsigned long seed = 777;
  const int sizes[] = {3, 4, 5, 7, 12, 30, 64, 65, 100, 250, 500, 1000};
  int wrong = 0, wrong_n = 0;
  for (int round = 0; round < 12; round++)
    for (unsigned s = 0; s < sizeof sizes / sizeof sizes[0]; s++) {
      const int n = sizes[s];
      const int span = round < 6 ? 8 : 60; /* few distinct values means many exact ties */
      for (int i = 0; i < n * 2; i += 2) {
        seed = seed * 1103515245UL + 12345UL;
        xy[i] = (int)((seed >> 8) % (unsigned)(W + 2 * span)) - span;
        seed = seed * 1103515245UL + 12345UL;
        xy[i + 1] = (int)((seed >> 8) % (unsigned)(H + 2 * span)) - span;
      }
      if (round < 6) /* a coarse lattice: many edges through pixel centres */
        for (int i = 0; i < n * 2; i++) xy[i] = xy[i] / 8 * 8;
      const int differing = polygon_differences(xy, n, &got) != 0;
      if (differing && !wrong) wrong_n = n;
      wrong += differing;
    }
  char msg[64];
  snprintf(msg, sizeof msg, "%d polygons differ, first with n=%d", wrong, wrong_n);
  TEST_ASSERT_EQUAL_INT_MESSAGE(0, wrong, msg);
  canvas_free(&got);
}

void test_polygon_45_degree_edges_through_pixel_centres_fill_the_same_every_time(void) {
  /* A diamond whose edges pass exactly through pixel centres. A pixel on a
   * left edge is in, one on a right edge is out. */
  Canvas got;
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&got, 30, 30, malloc));
  const int diamond[] = {10, 3, 17, 10, 10, 17, 3, 10};
  TEST_ASSERT_EQUAL_INT(0, polygon_differences(diamond, 4, &got));
  const int triangle[] = {0, 0, 21, 0, 0, 21};
  TEST_ASSERT_EQUAL_INT(0, polygon_differences(triangle, 3, &got));
  const int thirds[] = {1, 1, 25, 4, 7, 27}; /* slopes with no exact float form */
  TEST_ASSERT_EQUAL_INT(0, polygon_differences(thirds, 3, &got));
  canvas_free(&got);
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

void test_no_disc_means_whole_rows(void) {
  int x0, x1;
  for (int y = 0; y < c.h; y++) {
    TEST_ASSERT_TRUE(canvas_visible_span(&c, y, &x0, &x1));
    TEST_ASSERT_EQUAL_INT(0, x0);
    TEST_ASSERT_EQUAL_INT(c.w - 1, x1);
  }
}

static Canvas disc_canvas(int w, int h) {
  Canvas d;
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&d, w, h, malloc));
  TEST_ASSERT_EQUAL_INT(0, canvas_set_disc(&d, malloc));
  return d;
}

void test_disc_rows_are_symmetric_and_widest_in_the_middle(void) {
  Canvas d = disc_canvas(466, 466);
  int a0, a1, b0, b1;
  for (int y = 0; y < 233; y++) {
    TEST_ASSERT_TRUE(canvas_visible_span(&d, y, &a0, &a1));
    TEST_ASSERT_TRUE(canvas_visible_span(&d, 465 - y, &b0, &b1));
    TEST_ASSERT_EQUAL_INT(a0, b0);
    TEST_ASSERT_EQUAL_INT(a1, b1);
    TEST_ASSERT_EQUAL_INT(465, a0 + a1);
  }
  for (int y = 232; y <= 233; y++) {
    TEST_ASSERT_TRUE(canvas_visible_span(&d, y, &a0, &a1));
    TEST_ASSERT_EQUAL_INT(0, a0);
    TEST_ASSERT_EQUAL_INT(465, a1);
  }
  TEST_ASSERT_TRUE(canvas_visible_span(&d, 0, &a0, &a1));
  TEST_ASSERT_TRUE(a1 - a0 + 1 < 100);
  canvas_free(&d);
}

/* Nearest point of pixel (x, y)'s square to the centre, against the radius. */
static bool touches_circle(int x, int y, int w, int h) {
  const double cx = w / 2.0, cy = h / 2.0, r = (w < h ? w : h) / 2.0;
  const double nx = cx < x ? x : (cx > x + 1 ? x + 1 : cx);
  const double ny = cy < y ? y : (cy > y + 1 ? y + 1 : cy);
  return (nx - cx) * (nx - cx) + (ny - cy) * (ny - cy) < r * r;
}

static void assert_disc_matches_circle(int w, int h) {
  Canvas d = disc_canvas(w, h);
  for (int y = 0; y < h; y++) {
    int x0 = 0, x1 = -1;
    const bool any = canvas_visible_span(&d, y, &x0, &x1);
    for (int x = 0; x < w; x++) {
      const bool in = any && x >= x0 && x <= x1;
      if (in != touches_circle(x, y, w, h)) {
        char msg[64];
        snprintf(msg, sizeof msg, "%dx%d at (%d, %d)", w, h, x, y);
        TEST_FAIL_MESSAGE(msg);
      }
    }
  }
  canvas_free(&d);
}

void test_disc_includes_every_pixel_touching_the_circle(void) {
  assert_disc_matches_circle(466, 466);
  assert_disc_matches_circle(17, 17);
  assert_disc_matches_circle(20, 12);
}

void test_disc_follows_a_small_canvas(void) {
  Canvas d = disc_canvas(16, 16);
  int x0, x1;
  TEST_ASSERT_TRUE(canvas_visible_span(&d, 0, &x0, &x1));
  TEST_ASSERT_TRUE(x0 > 0);
  TEST_ASSERT_EQUAL_INT(15, x0 + x1);
  canvas_free(&d);
}

/* main.cpp's QR overlay: a 328 px square centred on the 466 px screen. */
void test_the_centred_qr_square_lies_inside_the_disc(void) {
  Canvas d = disc_canvas(466, 466);
  const int side = 328, at = (466 - side) / 2;
  for (int y = at; y < at + side; y++) {
    int x0, x1;
    TEST_ASSERT_TRUE(canvas_visible_span(&d, y, &x0, &x1));
    TEST_ASSERT_TRUE(x0 <= at);
    TEST_ASSERT_TRUE(x1 >= at + side - 1);
  }
  canvas_free(&d);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_the_centred_qr_square_lies_inside_the_disc);
  RUN_TEST(test_no_disc_means_whole_rows);
  RUN_TEST(test_disc_rows_are_symmetric_and_widest_in_the_middle);
  RUN_TEST(test_disc_includes_every_pixel_touching_the_circle);
  RUN_TEST(test_disc_follows_a_small_canvas);
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
  RUN_TEST(test_lines_inside_the_canvas_match_the_reference);
  RUN_TEST(test_lines_running_off_the_canvas_match_the_reference);
  RUN_TEST(test_line_far_offscreen_changes_nothing_and_terminates);
  RUN_TEST(test_line_diagonal_crossing_canvas_from_far_away);
  RUN_TEST(test_ellipse_diameter_1_is_one_pixel);
  RUN_TEST(test_ellipse_zero_size_draws_nothing);
  RUN_TEST(test_ellipse_5x5_is_symmetric);
  RUN_TEST(test_ellipse_partly_offscreen_clips);
  RUN_TEST(test_ellipse_fill_matches_the_double_reference_for_every_small_size);
  RUN_TEST(test_ellipse_fill_matches_the_double_reference_for_big_and_clipped_sizes);
  RUN_TEST(test_ellipse_fill_is_exact_on_random_big_sizes);
  RUN_TEST(test_ellipse_fill_is_exact_where_the_float_estimate_is_off);
  RUN_TEST(test_polygon_fill_is_exact_on_random_polygons);
  RUN_TEST(test_polygon_45_degree_edges_through_pixel_centres_fill_the_same_every_time);
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
