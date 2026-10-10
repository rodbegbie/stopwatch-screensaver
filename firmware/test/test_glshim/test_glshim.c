#include <sanitizer/allocator_interface.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
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

/* TinyGL lights a vertex with specular only if glSetEnableSpecular(1) is on
 * (it is off by default, and the hacks here never switch it on, so Gears has
 * no highlights). With it on, a vertex whose normal is (1, 0, 0) under a
 * light in direction (0.6, 0, 0.8) gets specular 0.6 ^ shininess: bright at
 * shininess 1, black at 100. If glMateriali did not reach TinyGL the
 * shininess would stay 0, and 0.6 ^ 0 is 1: bright in both. */
static int specular_red_at_centre(int shininess) {
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  glViewport(0, 0, SIZE, SIZE);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  const GLfloat black[4] = {0, 0, 0, 1}, white[4] = {1, 1, 1, 1};
  const GLfloat direction[4] = {0.6f, 0, 0.8f, 0};
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glSetEnableSpecular(1);
  glLightfv(GL_LIGHT0, GL_POSITION, (GLfloat *)direction);
  glLightfv(GL_LIGHT0, GL_AMBIENT, (GLfloat *)black);
  glLightfv(GL_LIGHT0, GL_DIFFUSE, (GLfloat *)black);
  glLightfv(GL_LIGHT0, GL_SPECULAR, (GLfloat *)white);
  glLightModelfv(GL_LIGHT_MODEL_AMBIENT, (GLfloat *)black);
  glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, (GLfloat *)black);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, (GLfloat *)black);
  glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, (GLfloat *)white);
  glMateriali(GL_FRONT_AND_BACK, GL_SHININESS, shininess);
  glNormal3f(1, 0, 0);
  glBegin(GL_TRIANGLES);
  glVertex3f(-0.9f, -0.9f, 0);
  glVertex3f(0.9f, -0.9f, 0);
  glVertex3f(0.0f, 0.9f, 0);
  glEnd();
  return px_swap(cv.px[(SIZE / 2) * SIZE + SIZE / 2]) >> 11; /* red, 0 to 31 */
}

void test_materiali_sets_the_shininess_tinygl_uses(void) {
  TEST_ASSERT_TRUE(specular_red_at_centre(1) >= 12);
  TEST_ASSERT_TRUE(specular_red_at_centre(100) <= 1);
}

/* Two-sided lighting takes one value, and Morph3D passes a one-element array.
 * TinyGL copied four floats whatever the parameter, which AddressSanitizer
 * reports as a global-buffer-overflow. */
void test_a_one_value_light_model_is_not_over_read(void) {
  static const GLfloat two_side[1] = {GL_TRUE};
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  glLightModelfv(GL_LIGHT_MODEL_TWO_SIDE, (GLfloat *)two_side);
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

/* Vertex arrays, as tube.c and sphere.c build them: one interleaved struct per
 * vertex, a stride in bytes, freed straight after the draw. */
typedef struct {
  float n[3];
  float v[3];
  float pad[2];
} Vtx; /* 32 bytes */

static const Vtx kTri[3] = {{{0, 0, 1}, {-0.8f, -0.8f, 0}, {0, 0}},
                            {{0, 0, 1}, {0.8f, -0.8f, 0}, {0, 0}},
                            {{0, 0, 1}, {0.0f, 0.8f, 0}, {0, 0}}};

static uint16_t reference[SIZE * SIZE];

static void open_flat_red_scene(void) {
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  glViewport(0, 0, SIZE, SIZE);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glDisable(GL_LIGHTING);
  glColor3f(1, 0, 0);
  glDisableClientState(GL_VERTEX_ARRAY);
  glDisableClientState(GL_NORMAL_ARRAY);
}

static void draw_triangle_immediately(void) {
  glBegin(GL_TRIANGLES);
  for (int i = 0; i < 3; i++) glVertex3f(kTri[i].v[0], kTri[i].v[1], kTri[i].v[2]);
  glEnd();
}

static void take_reference_and_clear(void) {
  draw_triangle_immediately();
  memcpy(reference, cv.px, sizeof reference);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

static int count_set(void) {
  int n = 0;
  for (int i = 0; i < SIZE * SIZE; i++) n += cv.px[i] != 0;
  return n;
}

static void point_at(const Vtx *tri) {
  glVertexPointer(3, GL_FLOAT, sizeof(Vtx), &tri->v);
  glNormalPointer(GL_FLOAT, sizeof(Vtx), &tri->n);
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_NORMAL_ARRAY);
}

void test_draw_arrays_honours_a_byte_stride(void) {
  open_flat_red_scene();
  take_reference_and_clear();
  point_at(kTri);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  TEST_ASSERT_TRUE(count_set() > 100);
  TEST_ASSERT_EQUAL_MEMORY(reference, cv.px, sizeof reference);
}

/* This is the bug that drew needles: TinyGL read the array when the list was
 * replayed, after the hack had freed it. */
void test_a_display_list_keeps_the_arrays_it_recorded(void) {
  open_flat_red_scene();
  take_reference_and_clear();
  Vtx *tri = malloc(sizeof kTri);
  memcpy(tri, kTri, sizeof kTri);
  point_at(tri);
  const GLuint list = glGenLists(1);
  glNewList(list, GL_COMPILE);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glEndList();
  memset(tri, 0xFF, sizeof kTri);
  free(tri);
  glCallList(list);
  TEST_ASSERT_TRUE(count_set() > 100);
  TEST_ASSERT_EQUAL_MEMORY(reference, cv.px, sizeof reference);
}

/* A new context starts with every client array disabled; the arrays the last
 * hack left enabled point at memory it has freed. */
void test_a_new_context_starts_with_no_client_arrays_enabled(void) {
  open_flat_red_scene();
  point_at(kTri);
  glshim_close();
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  glViewport(0, 0, SIZE, SIZE);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glDisable(GL_LIGHTING);
  glColor3f(1, 0, 0);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  TEST_ASSERT_EQUAL_INT(0, count_set());
}

void test_draw_arrays_with_nothing_enabled_or_a_zero_count_draws_nothing(void) {
  open_flat_red_scene();
  glVertexPointer(3, GL_FLOAT, sizeof(Vtx), &kTri[0].v);
  glDrawArrays(GL_TRIANGLES, 0, 3); /* pointers set, nothing enabled */
  TEST_ASSERT_EQUAL_INT(0, count_set());
  glEnableClientState(GL_VERTEX_ARRAY);
  glDrawArrays(GL_TRIANGLES, 0, 0);
  TEST_ASSERT_EQUAL_INT(0, count_set());
}


/* Dirty rectangle: TinyGL tracks the box around what it draws, glClear clears
 * only the box drawn since the last clear, and the swap marks only the old and
 * new boxes for the push. These pin that it changes nothing visible. */
static void triangle(float x0, float y0, float x1, float y1, float x2, float y2,
                     float z, float r, float g, float b) {
  glBegin(GL_TRIANGLES);
  glColor3f(r, g, b);
  glVertex3f(x0, y0, z);
  glVertex3f(x1, y1, z);
  glVertex3f(x2, y2, z);
  glEnd();
}

static void clear_all(void) { glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); }

static void assert_canvas_is(uint16_t colour, const char *why) {
  for (int i = 0; i < SIZE * SIZE; i++)
    if (cv.px[i] != colour) TEST_FAIL_MESSAGE(why);
}

void test_a_region_clear_still_leaves_the_whole_canvas_clear(void) {
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  glClearColor(1, 0, 0, 0);
  clear_all();
  triangle(-0.9f, -0.9f, -0.5f, -0.9f, -0.9f, -0.5f, 0, 0, 0, 1);
  glXSwapBuffers(dpy, 0);
  clear_all();
  triangle(0.5f, 0.5f, 0.9f, 0.5f, 0.9f, 0.9f, 0, 0, 0, 1);
  glXSwapBuffers(dpy, 0);
  clear_all();
  assert_canvas_is(rgb565(255, 0, 0), "a region clear left a stale pixel");
}

void test_the_first_clear_after_opening_clears_over_whatever_was_there(void) {
  for (int i = 0; i < SIZE * SIZE; i++) cv.px[i] = 0x1234;
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  glClearColor(1, 0, 0, 0);
  clear_all();
  assert_canvas_is(rgb565(255, 0, 0), "the first clear was not a full clear");
}

void test_a_new_clear_colour_clears_everything(void) {
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  glClearColor(1, 0, 0, 0);
  clear_all();
  triangle(-0.2f, -0.2f, 0.2f, -0.2f, 0.0f, 0.2f, 0, 0, 0, 1);
  glXSwapBuffers(dpy, 0);
  glClearColor(0, 1, 0, 0);
  clear_all();
  assert_canvas_is(rgb565(0, 255, 0), "a new clear colour left the old one behind");
}

void test_stale_depth_does_not_hide_the_next_frame(void) {
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  glEnable(GL_DEPTH_TEST);
  glClearColor(1, 0, 0, 0);
  clear_all();
  triangle(-0.5f, -0.5f, 0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0, 0, 1);
  glXSwapBuffers(dpy, 0);
  clear_all();
  triangle(-0.5f, -0.5f, 0.5f, -0.5f, 0.0f, 0.5f, 0.5f, 0, 1, 0);
  glXSwapBuffers(dpy, 0);
  TEST_ASSERT_EQUAL_HEX16(rgb565(0, 255, 0), cv.px[(SIZE / 2) * SIZE + SIZE / 2]);
}

/* Contract: pixels outside what was drawn are not rewritten by a clear, so
 * nothing else may draw on the canvas while a GL hack runs. */
void test_a_clear_rewrites_only_the_region_that_was_drawn(void) {
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  glClearColor(1, 0, 0, 0);
  clear_all();
  triangle(-0.9f, -0.9f, -0.5f, -0.9f, -0.9f, -0.5f, 0, 0, 0, 1);
  glXSwapBuffers(dpy, 0);
  const int far_pixel = (SIZE - 2) * SIZE + (SIZE - 2);
  cv.px[far_pixel] = 0x1234;
  clear_all();
  TEST_ASSERT_EQUAL_HEX16_MESSAGE(0x1234, cv.px[far_pixel],
                                  "the clear rewrote the whole canvas");
}

void test_the_swap_marks_only_the_old_and_new_boxes_dirty(void) {
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  glClearColor(1, 0, 0, 0);
  clear_all();
  triangle(-0.9f, 0.5f, -0.5f, 0.5f, -0.9f, 0.9f, 0, 0, 0, 1);
  glXSwapBuffers(dpy, 0);
  canvas_clear_dirty(&cv);
  clear_all();
  triangle(0.5f, -0.9f, 0.9f, -0.9f, 0.9f, -0.5f, 0, 0, 0, 1);
  glXSwapBuffers(dpy, 0);
  int x0, x1, dirty_rows = 0;
  for (int y = 0; y < SIZE; y++) dirty_rows += canvas_dirty_row(&cv, y, &x0, &x1);
  TEST_ASSERT_TRUE_MESSAGE(dirty_rows > 0, "nothing was marked dirty");
  TEST_ASSERT_TRUE_MESSAGE(dirty_rows < SIZE * 3 / 4, "almost every row was marked");
  TEST_ASSERT_FALSE_MESSAGE(canvas_dirty_row(&cv, SIZE / 2, &x0, &x1),
                            "a row between the two boxes was marked");
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_draw_arrays_honours_a_byte_stride);
  RUN_TEST(test_a_display_list_keeps_the_arrays_it_recorded);
  RUN_TEST(test_a_new_context_starts_with_no_client_arrays_enabled);
  RUN_TEST(test_draw_arrays_with_nothing_enabled_or_a_zero_count_draws_nothing);
  RUN_TEST(test_clear_writes_canvas_byte_order);
  RUN_TEST(test_the_first_list_name_is_not_zero);
  RUN_TEST(test_perspective_and_lookat_put_a_point_where_the_maths_says);
  RUN_TEST(test_materiali_sets_the_shininess_tinygl_uses);
  RUN_TEST(test_a_region_clear_still_leaves_the_whole_canvas_clear);
  RUN_TEST(test_the_first_clear_after_opening_clears_over_whatever_was_there);
  RUN_TEST(test_a_new_clear_colour_clears_everything);
  RUN_TEST(test_stale_depth_does_not_hide_the_next_frame);
  RUN_TEST(test_a_clear_rewrites_only_the_region_that_was_drawn);
  RUN_TEST(test_the_swap_marks_only_the_old_and_new_boxes_dirty);
  RUN_TEST(test_a_one_value_light_model_is_not_over_read);
  RUN_TEST(test_is_enabled_reports_texturing_off);
  RUN_TEST(test_swap_buffers_marks_every_row_dirty);
  RUN_TEST(test_releasing_the_display_frees_the_context);
  RUN_TEST(test_opening_twice_replaces_the_context_without_leaking);
  RUN_TEST(test_opening_on_an_empty_canvas_returns_null);
  return UNITY_END();
}
