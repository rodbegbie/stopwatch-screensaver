#include <stdint.h>
#include <string.h>
#include <unity.h>

#include "GL/gl.h"
#include "zbuffer.h"

/* A band is a ZBuffer of only a few rows; a held frame replays primitives
 * into it with their y values moved up by the band's first row, so a
 * primitive may start above the band or run past its end. The rasterisers
 * must draw only the band's rows (issue #43). The band sits in the middle of
 * a larger buffer whose rows either side are guards. */
enum { W = 64, BAND = 8, GUARD = 8, ROWS = GUARD + BAND + GUARD };
static const PIXEL kSentinel = 0x1234;

static PIXEL rows[ROWS * W];
static ZBuffer *zb;

void setUp(void) {
  for (int i = 0; i < ROWS * W; i++) rows[i] = kSentinel;
  zb = ZB_open(W, BAND, ZB_MODE_5R6G5B, rows + GUARD * W);
  TEST_ASSERT_NOT_NULL(zb);
  memset(zb->zbuf, 0, W * BAND * sizeof(GLushort));
  zb->depth_test = 0;
  zb->depth_write = 1;
  zb->enable_blend = 0;
  zb->line_smooth = 0;
  zb->pointsize = 1;
}

void tearDown(void) { ZB_close(zb); }

static ZBufferPoint point(GLint x, GLint y) {
  ZBufferPoint p;
  memset(&p, 0, sizeof p);
  p.x = x;
  p.y = y;
  p.fx = (GLfloat)x;
  p.fy = (GLfloat)y;
  p.z = 1000 << ZB_POINT_Z_FRAC_BITS;
  p.r = p.g = p.b = 0xff00;
  return p;
}

static int guards_untouched(void) {
  for (int y = 0; y < ROWS; y++) {
    if (y >= GUARD && y < GUARD + BAND) continue;
    for (int x = 0; x < W; x++)
      if (rows[y * W + x] != kSentinel) return 0;
  }
  return 1;
}

static int band_row_drawn(int row) {
  for (int x = 0; x < W; x++)
    if (rows[(GUARD + row) * W + x] != kSentinel) return 1;
  return 0;
}

void test_a_triangle_taller_than_the_band_fills_only_band_rows(void) {
  ZBufferPoint a = point(4, -5), b = point(60, 3), c = point(20, 14);
  ZB_fillTriangleSmoothNOBLEND(zb, &a, &b, &c);
  for (int row = 0; row < BAND; row++) TEST_ASSERT_TRUE(band_row_drawn(row));
  TEST_ASSERT_TRUE(guards_untouched());
}

void test_a_triangle_wholly_below_the_band_draws_nothing(void) {
  ZBufferPoint a = point(4, 9), b = point(60, 12), c = point(20, 15);
  ZB_fillTriangleSmoothNOBLEND(zb, &a, &b, &c);
  for (int row = 0; row < BAND; row++) TEST_ASSERT_FALSE(band_row_drawn(row));
  TEST_ASSERT_TRUE(guards_untouched());
}

void test_a_line_crossing_the_band_draws_only_inside_it(void) {
  ZBufferPoint a = point(2, -6), b = point(60, 13);
  ZB_line_z(zb, &a, &b);
  TEST_ASSERT_TRUE(band_row_drawn(0));
  TEST_ASSERT_TRUE(band_row_drawn(BAND - 1));
  TEST_ASSERT_TRUE(guards_untouched());
}

void test_a_line_without_depth_crossing_the_band_draws_only_inside_it(void) {
  ZBufferPoint a = point(2, -6), b = point(60, 13);
  ZB_line(zb, &a, &b);
  TEST_ASSERT_TRUE(band_row_drawn(0));
  TEST_ASSERT_TRUE(band_row_drawn(BAND - 1));
  TEST_ASSERT_TRUE(guards_untouched());
}

void test_a_smooth_line_crossing_the_band_draws_only_inside_it(void) {
  zb->line_smooth = 1;
  ZBufferPoint a = point(2, -6), b = point(60, 13);
  ZB_line_z(zb, &a, &b);
  TEST_ASSERT_TRUE(band_row_drawn(0));
  TEST_ASSERT_TRUE(band_row_drawn(BAND - 1));
  TEST_ASSERT_TRUE(guards_untouched());
}

void test_a_point_outside_the_band_draws_nothing(void) {
  const GLfloat sizes[2] = {1, 3};
  for (int i = 0; i < 2; i++) {
    zb->pointsize = sizes[i];
    ZBufferPoint above = point(10, -1), below = point(20, BAND);
    if (sizes[i] > 1) {
      above.y = -3;
      below.y = BAND + 2;
    }
    ZB_plot(zb, &above);
    ZB_plot(zb, &below);
  }
  TEST_ASSERT_TRUE(guards_untouched());
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_a_triangle_taller_than_the_band_fills_only_band_rows);
  RUN_TEST(test_a_triangle_wholly_below_the_band_draws_nothing);
  RUN_TEST(test_a_line_crossing_the_band_draws_only_inside_it);
  RUN_TEST(test_a_line_without_depth_crossing_the_band_draws_only_inside_it);
  RUN_TEST(test_a_smooth_line_crossing_the_band_draws_only_inside_it);
  RUN_TEST(test_a_point_outside_the_band_draws_nothing);
  return UNITY_END();
}
