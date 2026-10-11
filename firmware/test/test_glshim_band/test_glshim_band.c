#include <sanitizer/allocator_interface.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include "GL/gl.h"
#include "core/canvas.h"
#include "glshim/glshim.h"
#include "glshim/glshim_band.h"
#include "x11shim/xshim.h"
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

/* Held frames are drawn on a canvas several bands tall, its last band short. */
enum { CW = 96, CH = 100 };
static Canvas cv;
static Display *dpy;
static size_t live_before;
static PIXEL direct_px[CW * CH];

void setUp(void) {
  live_before = __sanitizer_get_current_allocated_bytes();
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&cv, CW, CH, malloc));
  dpy = xshim_open_display(&cv);
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

void tearDown(void) {
  ZB_close(zb);
  xshim_close_display(dpy); /* runs the release hook, which frees the context */
  canvas_free(&cv);
  TEST_ASSERT_EQUAL_UINT64_MESSAGE(live_before,
                                   __sanitizer_get_current_allocated_bytes(),
                                   "test leaked memory");
}

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

/* Held frames (issue #43): each scene is drawn once directly and once held
 * and banded, from the same seed, and the canvases must match exactly, since
 * a held call replays through the same rasteriser with the same inputs. Each
 * also checks a frame was really held, or a broken hold would pass. */
typedef void (*Scene)(int frame);

static void open_pixel_context(int banding) {
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  glshim_set_banding(banding);
  glViewport(0, 0, CW, CH);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, CW, CH, 0, -10, 10); /* y down, in pixels */
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

static void run_scene(Scene scene, int frames, int banding) {
  memset(cv.px, 0, CW * CH * sizeof(PIXEL));
  srandom(1);
  open_pixel_context(banding);
  for (int f = 0; f < frames; f++) {
    scene(f);
    glXSwapBuffers(dpy, 0);
  }
}

static void assert_held_matches_direct(Scene scene, int frames) {
  run_scene(scene, frames, 0);
  memcpy(direct_px, cv.px, sizeof direct_px);
  const unsigned held = glshim_band_held_frames();
  run_scene(scene, frames, 1);
  TEST_ASSERT_TRUE_MESSAGE(glshim_band_held_frames() > held, "no frame was held");
  TEST_ASSERT_EQUAL_MEMORY(direct_px, cv.px, sizeof direct_px);
}

static float rnd(float lo, float hi) {
  return lo + (hi - lo) * (float)(random() % 10000) / 10000.0f;
}

static void random_triangles(int n) {
  glBegin(GL_TRIANGLES);
  for (int i = 0; i < n * 3; i++) {
    glColor3f(rnd(0, 1), rnd(0, 1), rnd(0, 1));
    glNormal3f(rnd(-1, 1), rnd(-1, 1), 1);
    glVertex3f(rnd(-10, CW + 10), rnd(-10, CH + 10), rnd(-5, 5));
  }
  glEnd();
}

static void clear_both(void) {
  glClearColor(0.1f, 0.2f, 0.3f, 0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

static void lit_scene(int frame) {
  (void)frame;
  clear_both();
  glEnable(GL_DEPTH_TEST);
  glShadeModel(GL_SMOOTH);
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glEnable(GL_LIGHT1);
  random_triangles(30);
}

void test_a_held_frame_of_overlapping_lit_triangles_matches_direct(void) {
  assert_held_matches_direct(lit_scene, 2);
}

static void flat_scene(int frame) {
  (void)frame;
  clear_both();
  glEnable(GL_DEPTH_TEST);
  glShadeModel(GL_FLAT);
  random_triangles(30);
}

void test_a_held_frame_with_flat_shading_matches_direct(void) {
  assert_held_matches_direct(flat_scene, 2);
}

static void additive_scene(int frame) {
  (void)frame;
  clear_both();
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);
  random_triangles(30);
}

void test_a_held_frame_with_additive_blending_matches_direct(void) {
  assert_held_matches_direct(additive_scene, 2);
}

static void offset_scene(int frame) {
  (void)frame;
  clear_both();
  glEnable(GL_DEPTH_TEST);
  for (int i = 0; i < 10; i++) {
    const float x = rnd(0, CW - 30), y = rnd(0, CH - 30), z = rnd(-5, 5);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glColor3f(1, 0, 0);
    glBegin(GL_TRIANGLES);
    glVertex3f(x, y, z);
    glVertex3f(x + 30, y + 5, z);
    glVertex3f(x + 10, y + 30, z);
    glEnd();
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(0.5f, 0.5f);
    glColor3f(0, 1, 0);
    glBegin(GL_TRIANGLES);
    glVertex3f(x, y, z);
    glVertex3f(x + 30, y + 5, z);
    glVertex3f(x + 10, y + 30, z);
    glEnd();
  }
}

void test_a_held_frame_with_polygon_offset_matches_direct(void) {
  assert_held_matches_direct(offset_scene, 2);
}

/* Each held call keeps the state it was drawn with: depth tested, then
 * blended without the test, then tested without writing depth. */
static void state_change_scene(int frame) {
  (void)frame;
  clear_both();
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  random_triangles(8);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);
  random_triangles(8);
  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  random_triangles(8);
  glDepthMask(GL_TRUE);
}

void test_a_state_change_mid_frame_matches_direct(void) {
  assert_held_matches_direct(state_change_scene, 1);
}

static void boundary_scene(int frame) {
  (void)frame;
  clear_both();
  glColor3f(1, 1, 0);
  glBegin(GL_TRIANGLES);
  glVertex3f(5, 31, 0);
  glVertex3f(90, 31, 0);
  glVertex3f(40, 32, 0);
  glVertex3f(5, 63, 0);
  glVertex3f(90, 64, 0);
  glVertex3f(40, 96, 0);
  glEnd();
}

void test_a_triangle_on_a_band_boundary_matches_direct(void) {
  assert_held_matches_direct(boundary_scene, 1);
}

static void moving_scene(int frame) {
  clear_both();
  const float x = 5 + 25 * frame;
  glColor3f(1, 0.5f, 0);
  glBegin(GL_TRIANGLES);
  glVertex3f(x, 10, 0);
  glVertex3f(x + 20, 50, 0);
  glVertex3f(x, 90, 0);
  glEnd();
}

void test_three_frames_of_a_moving_triangle_match_direct(void) {
  assert_held_matches_direct(moving_scene, 3);
}

static void many_scene(int frame) {
  (void)frame;
  clear_both();
  glEnable(GL_DEPTH_TEST);
  glshim_band_set_capacity(4);
  random_triangles(20);
}

void test_a_frame_that_overflows_the_store_matches_direct(void) {
  const unsigned spills = glshim_band_spills();
  assert_held_matches_direct(many_scene, 2);
  TEST_ASSERT_EQUAL_UINT(spills + 2, glshim_band_spills());
}

static void no_clear_scene(int frame) {
  if (frame == 0) clear_both();
  random_triangles(5);
  if (frame == 1) TEST_ASSERT_FALSE(glshim_band_holding());
}

void test_a_frame_without_a_clear_draws_directly(void) {
  assert_held_matches_direct(no_clear_scene, 2);
}

static void lines_scene(int frame) {
  (void)frame;
  clear_both();
  glEnable(GL_DEPTH_TEST);
  random_triangles(10);
  glColor3f(1, 1, 1);
  glBegin(GL_LINES);
  glVertex3f(3, 3, 6);
  glVertex3f(90, 95, 6);
  glEnd();
  glPointSize(3);
  glBegin(GL_POINTS);
  glVertex3f(50, 20, 6);
  glEnd();
  random_triangles(5);
}

void test_lines_and_points_in_a_held_frame_land_in_order(void) {
  assert_held_matches_direct(lines_scene, 1);
}

static void text_scene(int frame) {
  (void)frame;
  clear_both();
  random_triangles(10);
  glDrawText((const GLubyte *)"HI", 20, 40, 0xFFFF);
}

void test_text_in_a_held_frame_lands_over_earlier_triangles(void) {
  assert_held_matches_direct(text_scene, 1);
}

static void colour_only_scene(int frame) {
  if (frame == 1) {
    glClear(GL_COLOR_BUFFER_BIT); /* keeps the depth of frame 0 */
  } else {
    clear_both();
  }
  glEnable(GL_DEPTH_TEST);
  random_triangles(10);
}

/* Frame 1 draws against depth a held frame 0 never wrote, so the context
 * stops holding: frame 2 is drawn directly and matches. */
void test_a_colour_only_clear_with_depth_on_after_a_held_frame_stops_banding(void) {
  run_scene(colour_only_scene, 3, 0);
  memcpy(direct_px, cv.px, sizeof direct_px);
  const unsigned held = glshim_band_held_frames();
  run_scene(colour_only_scene, 3, 1);
  TEST_ASSERT_EQUAL_UINT(held + 1, glshim_band_held_frames());
  TEST_ASSERT_EQUAL_MEMORY(direct_px, cv.px, sizeof direct_px);
}

void test_banding_stays_off_when_internal_ram_is_short(void) {
  run_scene(lit_scene, 2, 0);
  memcpy(direct_px, cv.px, sizeof direct_px);
  const unsigned held = glshim_band_held_frames();
  glshim_band_fail_next_open();
  run_scene(lit_scene, 2, 1);
  TEST_ASSERT_EQUAL_UINT(held, glshim_band_held_frames());
  TEST_ASSERT_EQUAL_MEMORY(direct_px, cv.px, sizeof direct_px);
}

void test_closing_mid_frame_drops_the_held_calls(void) {
  memset(cv.px, 0, CW * CH * sizeof(PIXEL));
  open_pixel_context(1);
  clear_both();
  random_triangles(10);
  TEST_ASSERT_TRUE(glshim_band_holding());
  glshim_close();
  open_pixel_context(1);
  glXSwapBuffers(dpy, 0);
  for (int i = 0; i < CW * CH; i++) TEST_ASSERT_EQUAL_HEX16(0, cv.px[i]);
}

void test_opening_and_closing_fifty_times_does_not_grow_memory(void) {
  open_pixel_context(1);
  glshim_close();
  const size_t before = __sanitizer_get_current_allocated_bytes();
  for (int i = 0; i < 50; i++) {
    open_pixel_context(1);
    clear_both();
    random_triangles(3);
    glXSwapBuffers(dpy, 0);
    glshim_close();
  }
  TEST_ASSERT_EQUAL_UINT64(before, __sanitizer_get_current_allocated_bytes());
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_a_triangle_taller_than_the_band_fills_only_band_rows);
  RUN_TEST(test_a_triangle_wholly_below_the_band_draws_nothing);
  RUN_TEST(test_a_line_crossing_the_band_draws_only_inside_it);
  RUN_TEST(test_a_line_without_depth_crossing_the_band_draws_only_inside_it);
  RUN_TEST(test_a_smooth_line_crossing_the_band_draws_only_inside_it);
  RUN_TEST(test_a_point_outside_the_band_draws_nothing);
  RUN_TEST(test_a_held_frame_of_overlapping_lit_triangles_matches_direct);
  RUN_TEST(test_a_held_frame_with_flat_shading_matches_direct);
  RUN_TEST(test_a_held_frame_with_additive_blending_matches_direct);
  RUN_TEST(test_a_held_frame_with_polygon_offset_matches_direct);
  RUN_TEST(test_a_state_change_mid_frame_matches_direct);
  RUN_TEST(test_a_triangle_on_a_band_boundary_matches_direct);
  RUN_TEST(test_three_frames_of_a_moving_triangle_match_direct);
  RUN_TEST(test_a_frame_that_overflows_the_store_matches_direct);
  RUN_TEST(test_a_frame_without_a_clear_draws_directly);
  RUN_TEST(test_text_in_a_held_frame_lands_over_earlier_triangles);
  RUN_TEST(test_lines_and_points_in_a_held_frame_land_in_order);
  RUN_TEST(test_a_colour_only_clear_with_depth_on_after_a_held_frame_stops_banding);
  RUN_TEST(test_banding_stays_off_when_internal_ram_is_short);
  RUN_TEST(test_closing_mid_frame_drops_the_held_calls);
  RUN_TEST(test_opening_and_closing_fifty_times_does_not_grow_memory);
  return UNITY_END();
}
