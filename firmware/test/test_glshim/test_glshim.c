#include <sanitizer/allocator_interface.h>
#include <stdlib.h>
#include <unity.h>

#include "GL/gl.h"
#include "core/canvas.h"
#include "glshim/glshim.h"
#include "x11shim/xshim.h"
#include "zbuffer.h"

#define SIZE 64

static Canvas cv;
static Display *dpy;
static size_t live_before;

void setUp(void) {
  live_before = __sanitizer_get_current_allocated_bytes();
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&cv, SIZE, SIZE, malloc));
  dpy = xshim_open_display(&cv);
}

void tearDown(void) {
  xshim_close_display(dpy); /* runs the release hook, which frees the context */
  canvas_free(&cv);
  TEST_ASSERT_EQUAL_UINT64_MESSAGE(live_before,
                                   __sanitizer_get_current_allocated_bytes(),
                                   "test leaked memory");
}

/* The canvas keeps RGB565 with its bytes swapped, so TinyGL must write that
 * order itself. The width is 66, not a multiple of 4: the last column catches
 * TinyGL rounding the buffer width down to 64. White would be the same either
 * way round, so the colours are red and green. */
void test_clear_writes_canvas_byte_order(void) {
  Canvas wide;
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&wide, 66, 8, malloc));
  ZBuffer *zb = ZB_open(66, 8, ZB_MODE_5R6G5B, wide.px);
  TEST_ASSERT_NOT_NULL(zb);
  glInit(zb);

  glClearColor(1, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT);
  TEST_ASSERT_EQUAL_HEX16(rgb565(255, 0, 0), wide.px[0]);
  TEST_ASSERT_EQUAL_HEX16(rgb565(255, 0, 0), wide.px[65]);

  glClearColor(0, 1, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT);
  TEST_ASSERT_EQUAL_HEX16(rgb565(0, 255, 0), wide.px[0]);
  TEST_ASSERT_EQUAL_HEX16(rgb565(0, 255, 0), wide.px[65]);

  glClose();
  ZB_close(zb);
  canvas_free(&wide);
}

/* Real GL reserves list name 0 as invalid, and hacks abort on it. */
void test_the_first_list_name_is_not_zero(void) {
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  TEST_ASSERT_TRUE(glGenLists(1) >= 1);
}

/* The eye is off the axis, at (3, 0, 4) looking at the origin, because from
 * the axis the rotation is the identity and a transposed matrix would look
 * the same. By hand with a 90 degree field of view: the world point (1, 0, 0)
 * is 4.4 in front of the eye and 0.8 to its right, so ndc x = 0.8 / 4.4 =
 * 0.18, pixel column 37.8 of 64. The triangle below projects to a shape that
 * covers row 32 from about column 36 to 39.7. Red, not white, so a byte swap
 * would show. */
void test_perspective_and_lookat_put_a_point_where_the_maths_says(void) {
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  glViewport(0, 0, SIZE, SIZE);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(90, 1, 1, 100);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  gluLookAt(3, 0, 4, 0, 0, 0, 0, 1, 0);
  glDisable(GL_LIGHTING);
  glColor3f(1, 0, 0);
  glBegin(GL_TRIANGLES);
  glVertex3f(0.5f, -0.6f, 0.0f);
  glVertex3f(1.5f, -0.6f, 0.0f);
  glVertex3f(1.0f, 0.6f, 0.0f);
  glEnd();
  TEST_ASSERT_EQUAL_HEX16(rgb565(255, 0, 0), cv.px[32 * SIZE + 37]);
  TEST_ASSERT_EQUAL_HEX16(0, cv.px[32 * SIZE + 16]);
}

void test_is_enabled_reports_texturing_off(void) {
  TEST_ASSERT_EQUAL_INT(0, glIsEnabled(GL_TEXTURE_2D));
}

void test_swap_buffers_marks_every_row_dirty(void) {
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  canvas_clear_dirty(&cv);
  glXSwapBuffers(dpy, 1);
  for (int y = 0; y < SIZE; y++) {
    int x0, x1;
    TEST_ASSERT_TRUE(canvas_dirty_row(&cv, y, &x0, &x1));
    TEST_ASSERT_EQUAL_INT(0, x0);
    TEST_ASSERT_EQUAL_INT(SIZE - 1, x1);
  }
}

void test_releasing_the_display_frees_the_context(void) {
  const size_t before = __sanitizer_get_current_allocated_bytes();
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  TEST_ASSERT_TRUE(__sanitizer_get_current_allocated_bytes() > before);
  xshim_release_pixmaps(dpy);
  TEST_ASSERT_EQUAL_UINT64(before, __sanitizer_get_current_allocated_bytes());
}

/* A hack that restarts itself calls init_GL again without stopping. */
void test_opening_twice_replaces_the_context_without_leaking(void) {
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  const size_t once = __sanitizer_get_current_allocated_bytes();
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  TEST_ASSERT_EQUAL_UINT64(once, __sanitizer_get_current_allocated_bytes());
}

void test_opening_on_an_empty_canvas_returns_null(void) {
  Canvas empty = {0};
  const size_t before = __sanitizer_get_current_allocated_bytes();
  TEST_ASSERT_NULL(glshim_open(&empty));
  TEST_ASSERT_EQUAL_UINT64(before, __sanitizer_get_current_allocated_bytes());
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_clear_writes_canvas_byte_order);
  RUN_TEST(test_the_first_list_name_is_not_zero);
  RUN_TEST(test_perspective_and_lookat_put_a_point_where_the_maths_says);
  RUN_TEST(test_is_enabled_reports_texturing_off);
  RUN_TEST(test_swap_buffers_marks_every_row_dirty);
  RUN_TEST(test_releasing_the_display_frees_the_context);
  RUN_TEST(test_opening_twice_replaces_the_context_without_leaking);
  RUN_TEST(test_opening_on_an_empty_canvas_returns_null);
  return UNITY_END();
}
