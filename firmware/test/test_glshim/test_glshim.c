#include <math.h>
#include <sanitizer/allocator_interface.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include "GL/gl.h"
#include "core/canvas.h"
#include "fps.h"
#include "glshim/glshim.h"
#include "glshim/glshim_prof.h"
#include "tinygl/src/zgl.h"
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

/* TinyGL's glOrtho is commented out of its header. The box here is not
 * centred and not square (x 0 to 10, y 0 to 20, z -1 to 1), so a transposed
 * matrix, a flipped sign or a swapped axis would put the triangle somewhere
 * else. By hand: x = 2.5 of 10 is a quarter of the way across, column 16 of
 * 64; y = 15 of 20 is a quarter of the way down from the top, row 16. */
void test_ortho_puts_a_point_where_the_maths_says(void) {
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  glViewport(0, 0, SIZE, SIZE);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, 10, 0, 20, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glDisable(GL_LIGHTING);
  glColor3f(1, 0, 0);
  glBegin(GL_TRIANGLES);
  glVertex3f(1.5f, 13.0f, 0.0f);
  glVertex3f(3.5f, 13.0f, 0.0f);
  glVertex3f(2.5f, 17.0f, 0.0f);
  glEnd();
  TEST_ASSERT_EQUAL_HEX16(rgb565(255, 0, 0), cv.px[16 * SIZE + 16]);
  TEST_ASSERT_EQUAL_HEX16(0, cv.px[16 * SIZE + 48]);
  TEST_ASSERT_EQUAL_HEX16(0, cv.px[48 * SIZE + 16]);
  TEST_ASSERT_EQUAL_HEX16(0, cv.px[48 * SIZE + 48]);
}

/* Depth is mapped too: the near plane is z = -near, so two overlapping
 * triangles show the nearer one whatever order they are drawn in. */
void test_ortho_maps_depth_so_the_nearer_triangle_wins(void) {
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  glViewport(0, 0, SIZE, SIZE);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(-1, 1, -1, 1, 1, 5);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glDisable(GL_LIGHTING);
  glEnable(GL_DEPTH_TEST);
  for (int near_first = 0; near_first < 2; near_first++) {
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    for (int pass = 0; pass < 2; pass++) {
      const int near = near_first ? pass == 0 : pass == 1;
      glColor3f(near ? 0.0f : 1.0f, near ? 1.0f : 0.0f, 0);
      glBegin(GL_TRIANGLES);
      glVertex3f(-0.8f, -0.8f, near ? -2.0f : -4.0f);
      glVertex3f(0.8f, -0.8f, near ? -2.0f : -4.0f);
      glVertex3f(0.0f, 0.8f, near ? -2.0f : -4.0f);
      glEnd();
    }
    TEST_ASSERT_EQUAL_HEX16(rgb565(0, 255, 0), cv.px[(SIZE / 2) * SIZE + SIZE / 2]);
  }
}

/* The planes are where they are asked: near 1 and far 5 keep a triangle at
 * z = -4.9 and clip one at -5.1, and clip one at -0.9 and keep one at -1.1. */
static int ortho_triangle_shows_at(float z) {
  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glColor3f(1, 0, 0);
  glBegin(GL_TRIANGLES);
  glVertex3f(-0.8f, -0.8f, z);
  glVertex3f(0.8f, -0.8f, z);
  glVertex3f(0.0f, 0.8f, z);
  glEnd();
  return cv.px[(SIZE / 2) * SIZE + SIZE / 2] == rgb565(255, 0, 0);
}

void test_ortho_clips_at_the_near_and_far_planes(void) {
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  glViewport(0, 0, SIZE, SIZE);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(-1, 1, -1, 1, 1, 5);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glDisable(GL_LIGHTING);
  glEnable(GL_DEPTH_TEST);
  TEST_ASSERT_TRUE(ortho_triangle_shows_at(-4.9f));
  TEST_ASSERT_FALSE(ortho_triangle_shows_at(-5.1f));
  TEST_ASSERT_TRUE(ortho_triangle_shows_at(-1.1f));
  TEST_ASSERT_FALSE(ortho_triangle_shows_at(-0.9f));
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
  /* Two disjoint primitives a frame, so a box that failed to grow to cover
   * both would leave one of them behind. */
  triangle(-0.9f, -0.9f, -0.5f, -0.9f, -0.9f, -0.5f, 0, 0, 0, 1);
  triangle(0.5f, 0.5f, 0.9f, 0.5f, 0.9f, 0.9f, 0, 0, 0, 1);
  glXSwapBuffers(dpy, 0);
  clear_all();
  triangle(0.5f, -0.9f, 0.9f, -0.9f, 0.9f, -0.5f, 0, 0, 0, 1);
  triangle(-0.9f, 0.5f, -0.5f, 0.5f, -0.9f, 0.9f, 0, 0, 0, 1);
  glXSwapBuffers(dpy, 0);
  clear_all();
  assert_canvas_is(rgb565(255, 0, 0), "a region clear left a stale pixel");
}

/* A line can run in any direction, so its endpoints arrive in any order. */
void test_a_line_drawn_in_any_direction_is_cleared_again(void) {
  const float ends[4][4] = {{-0.8f, -0.6f, 0.8f, 0.6f},
                            {0.8f, 0.6f, -0.8f, -0.6f},
                            {-0.8f, 0.6f, 0.8f, -0.6f},
                            {0.8f, -0.6f, -0.8f, 0.6f}};
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  glClearColor(1, 0, 0, 0);
  clear_all();
  for (int i = 0; i < 4; i++) {
    glBegin(GL_LINES);
    glColor3f(0, 0, 1);
    glVertex3f(ends[i][0], ends[i][1], 0);
    glVertex3f(ends[i][2], ends[i][3], 0);
    glEnd();
    glXSwapBuffers(dpy, 0);
    clear_all();
    assert_canvas_is(rgb565(255, 0, 0), "a line left stale pixels after a clear");
  }
}

static int pixels_not(uint16_t colour) {
  int n = 0;
  for (int i = 0; i < SIZE * SIZE; i++) n += cv.px[i] != colour;
  return n;
}

/* TinyGL also writes the canvas outside its primitives: text, and glDrawPixels
 * (which cannot be tested here: it keeps its data pointer in a 4-byte GLParam,
 * which only holds a pointer on the 32-bit board). Both must be inside the box
 * a clear erases. */
void test_text_is_cleared_again(void) {
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  glClearColor(1, 0, 0, 0);
  clear_all();
  glXSwapBuffers(dpy, 0);
  glDrawText((const GLubyte *)"A", 20, 20, 0x0000FF);
  glXSwapBuffers(dpy, 0);
  TEST_ASSERT_TRUE_MESSAGE(pixels_not(rgb565(255, 0, 0)) > 0, "no text was drawn");
  clear_all();
  assert_canvas_is(rgb565(255, 0, 0), "text left stale pixels after a clear");
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

/* With the depth test off nothing reads the depth buffer, so a glClear that
 * asks for it does no work until the test is switched on. HexTrail asks every
 * frame and never enables the test: a second full-canvas clear of PSRAM. */
void test_a_depth_clear_does_nothing_while_the_depth_test_is_off(void) {
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  glDisable(GL_DEPTH_TEST);
  const unsigned before = glshim_depth_clears();
  for (int i = 0; i < 3; i++) {
    clear_all();
    triangle(-0.5f, -0.5f, 0.5f, -0.5f, 0.0f, 0.5f, 0.0f, 0, 0, 1);
    glXSwapBuffers(dpy, 0);
  }
  TEST_ASSERT_EQUAL_UINT(before, glshim_depth_clears());
  glEnable(GL_DEPTH_TEST);
  TEST_ASSERT_EQUAL_UINT(before + 1, glshim_depth_clears());
}

/* The skipped clear is only deferred: a hack that clears with the test off and
 * then enables it sees a cleared depth buffer, as with the stock clear. */
void test_a_depth_clear_made_with_the_test_off_applies_once_it_is_on(void) {
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  glDisable(GL_DEPTH_TEST);
  glClearColor(1, 0, 0, 0);
  clear_all();
  triangle(-0.5f, -0.5f, 0.5f, -0.5f, 0.0f, 0.5f, -0.5f, 0, 0, 1);
  clear_all();
  glEnable(GL_DEPTH_TEST);
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
  /* One triangle was drawn now and the other was erased: a row from each
   * band, at the top and the bottom of the canvas, must be marked. */
  int top = 0, bottom = 0;
  for (int y = 0; y < SIZE / 4; y++) top += canvas_dirty_row(&cv, y, &x0, &x1);
  for (int y = SIZE * 3 / 4; y < SIZE; y++)
    bottom += canvas_dirty_row(&cv, y, &x0, &x1);
  TEST_ASSERT_TRUE_MESSAGE(top > 0, "the top band was not marked");
  TEST_ASSERT_TRUE_MESSAGE(bottom > 0, "the bottom band was not marked");
  TEST_ASSERT_TRUE_MESSAGE(dirty_rows < SIZE * 3 / 4, "almost every row was marked");
  TEST_ASSERT_FALSE_MESSAGE(canvas_dirty_row(&cv, SIZE / 2, &x0, &x1),
                            "a row between the two boxes was marked");
}

void test_line_width_is_accepted_and_changes_nothing(void) {
  open_flat_red_scene();
  take_reference_and_clear();

  draw_triangle_immediately();
  glLineWidth(4);
  TEST_ASSERT_EQUAL_MEMORY(reference, cv.px, sizeof reference);
  draw_triangle_immediately();
  TEST_ASSERT_EQUAL_MEMORY(reference, cv.px, sizeof reference);
}

static void compile_a_quad_strip_into(GLuint list) {
  glNewList(list, GL_COMPILE);
  glBegin(GL_QUAD_STRIP);
  for (int i = 0; i < 100; i++) {
    glVertex3f(i / 100.0f, 0, 0.5f);
    glVertex3f(i / 100.0f, 0, 0);
  }
  glEnd();
  glEndList();
}

/* UnknownPleasures rebuilds one row's lists every tick under the same names.
 * TinyGL must free the old contents, or the board runs out of PSRAM. */
void test_recompiling_a_list_under_the_same_name_does_not_grow_memory(void) {
  open_flat_red_scene();
  GLuint list = glGenLists(1);
  TEST_ASSERT_NOT_EQUAL_UINT(0, list);
  compile_a_quad_strip_into(list);
  const size_t before = __sanitizer_get_current_allocated_bytes();
  for (int i = 0; i < 300; i++) compile_a_quad_strip_into(list);
  const size_t after = __sanitizer_get_current_allocated_bytes();
  TEST_ASSERT_TRUE_MESSAGE(after <= before + 4096, "memory grew");
}

/* Antialiased lines. The brightness of a pixel is its green channel (0 to 63,
 * white is 63). Positions are in clip space, where one pixel is 2 / SIZE. */
#define PIXEL_NDC (2.0f / SIZE)

static void open_line_scene(int smooth) {
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  glViewport(0, 0, SIZE, SIZE);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glDisable(GL_LIGHTING);
  glDisable(GL_DEPTH_TEST);
  glColor3f(1, 1, 1);
  if (smooth)
    glEnable(GL_LINE_SMOOTH);
  else
    glDisable(GL_LINE_SMOOTH);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

static int green_at(int x, int y) { return (px_swap(cv.px[y * SIZE + x]) >> 5) & 63; }

/* Total brightness, and its centroid along the row (0) or column (1) axis. */
static void measure_line(int along_columns, double *total, double *centroid) {
  double sum = 0, moment = 0;
  for (int y = 0; y < SIZE; y++)
    for (int x = 0; x < SIZE; x++) {
      const int g = green_at(x, y);
      sum += g;
      moment += g * (along_columns ? x : y);
    }
  *total = sum;
  *centroid = sum > 0 ? moment / sum : -1;
}

static void draw_line(float x0, float y0, float x1, float y1) {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glBegin(GL_LINES);
  glVertex3f(x0, y0, 0);
  glVertex3f(x1, y1, 0);
  glEnd();
}

/* Moving a line a quarter of a pixel at a time must move it a quarter of a
 * pixel at a time, with the same total brightness. Without smoothing it moves
 * a whole pixel, once, somewhere in the sweep. */
static void check_a_line_moves_smoothly(int vertical) {
  double first_total = 0, last_centroid = 0, direction = 0;
  for (int k = 0; k <= 12; k++) {
    const float at = 0.1f + k * 0.25f * PIXEL_NDC;
    if (vertical)
      draw_line(at, -0.8f, at, 0.8f);
    else
      draw_line(-0.8f, at, 0.8f, at);
    double total, centroid;
    measure_line(vertical, &total, &centroid);
    if (k == 0) {
      first_total = total;
      TEST_ASSERT_TRUE(total > 0);
    } else {
      TEST_ASSERT_FLOAT_WITHIN(0.04f * first_total, first_total, total);
      const double step = centroid - last_centroid;
      if (direction == 0) direction = step > 0 ? 1 : -1;
      TEST_ASSERT_FLOAT_WITHIN(0.07f, 0.25f, (float)(direction * step));
    }
    last_centroid = centroid;
  }
}

void test_a_smooth_horizontal_line_moves_a_quarter_pixel_at_a_time(void) {
  open_line_scene(1);
  check_a_line_moves_smoothly(0);
}

void test_a_smooth_vertical_line_moves_a_quarter_pixel_at_a_time(void) {
  open_line_scene(1);
  check_a_line_moves_smoothly(1);
}

void test_a_line_without_smoothing_still_moves_a_whole_pixel_at_a_time(void) {
  open_line_scene(0);
  double first = 0, jumps = 0, last = 0, total;
  for (int k = 0; k <= 12; k++) {
    draw_line(-0.8f, 0.1f + k * 0.25f * PIXEL_NDC, 0.8f, 0.1f + k * 0.25f * PIXEL_NDC);
    double centroid;
    measure_line(0, &total, &centroid);
    if (k == 0) first = centroid;
    else if (centroid != last) jumps += 1;
    last = centroid;
  }
  TEST_ASSERT_TRUE_MESSAGE(jumps >= 1 && jumps <= 4, "expected a few whole-pixel jumps");
  TEST_ASSERT_TRUE(last != first);
}

/* A shallow slope has a different pixel run in every column. Each column of
 * a smooth line adds up to about one full pixel of brightness. */
void test_a_smooth_sloped_line_has_one_pixel_of_brightness_a_column(void) {
  open_line_scene(1);
  const float slopes[] = {0.0f, 0.05f, 0.2f, 0.7f};
  for (int s = 0; s < 4; s++) {
    draw_line(-0.8f, 0.13f, 0.8f, 0.13f + slopes[s]);
    double total, centroid;
    measure_line(0, &total, &centroid);
    const double columns = 1.6 / PIXEL_NDC;
    const double length = sqrt(1.0 + pow(slopes[s] / 1.6, 2));
    TEST_ASSERT_FLOAT_WITHIN((float)(0.08 * columns * 63 * length),
                             (float)(columns * 63 * length), (float)total);
  }
}

/* A line two depths apart from a triangle is hidden by it in one order and
 * hides it in the other, whether or not it is smoothed. */
static int line_shows_over_triangle(float z_line, float z_triangle, int smooth) {
  open_line_scene(smooth);
  glEnable(GL_DEPTH_TEST);
  glColor3f(1, 0, 0);
  glBegin(GL_TRIANGLES);
  glVertex3f(-1, -1, z_triangle);
  glVertex3f(1, -1, z_triangle);
  glVertex3f(0, 1, z_triangle);
  glEnd();
  glColor3f(1, 1, 1);
  glBegin(GL_LINES);
  glVertex3f(-0.5f, 0.0f, z_line);
  glVertex3f(0.5f, 0.0f, z_line);
  glEnd();
  int shows = 0;
  for (int x = 0; x < SIZE; x++)
    for (int y = 0; y < SIZE; y++) shows += green_at(x, y) > 40;
  return shows > 20;
}

void test_a_smooth_line_obeys_the_depth_test(void) {
  for (int smooth = 0; smooth <= 1; smooth++) {
    const int in_front = line_shows_over_triangle(-0.5f, 0.5f, smooth);
    glshim_close();
    const int behind = line_shows_over_triangle(0.5f, -0.5f, smooth);
    glshim_close();
    TEST_ASSERT_TRUE_MESSAGE(in_front != behind, "depth order made no difference");
  }
  const int off_front = line_shows_over_triangle(-0.5f, 0.5f, 0);
  glshim_close();
  const int on_front = line_shows_over_triangle(-0.5f, 0.5f, 1);
  TEST_ASSERT_EQUAL_INT(off_front, on_front);
}

/* A line a tenth of a pixel inside an edge spills half a pixel of coverage
 * outside it. An unclipped column write wraps to the other side of the next
 * row, still inside the buffer, so check that side stays black; the top and
 * bottom spills fall outside the buffer, where AddressSanitizer sees them. */
static int lit_in_column(int x) {
  int n = 0;
  for (int y = 0; y < SIZE; y++) n += cv.px[y * SIZE + x] != 0;
  return n;
}

static int lit_in_row(int y) {
  int n = 0;
  for (int x = 0; x < SIZE; x++) n += cv.px[y * SIZE + x] != 0;
  return n;
}

void test_a_smooth_line_at_the_canvas_edge_stays_inside_it(void) {
  open_line_scene(1);
  const float e = 1.0f - 0.1f * PIXEL_NDC;
  draw_line(e, -1.2f, e, 1.2f);
  TEST_ASSERT_TRUE(lit_in_column(SIZE - 1) > 0);
  TEST_ASSERT_EQUAL_INT(0, lit_in_column(0));
  draw_line(-e, -1.2f, -e, 1.2f);
  TEST_ASSERT_TRUE(lit_in_column(0) > 0);
  TEST_ASSERT_EQUAL_INT(0, lit_in_column(SIZE - 1));
  draw_line(-1.2f, e, 1.2f, e);
  TEST_ASSERT_TRUE((lit_in_row(0) > 0) != (lit_in_row(SIZE - 1) > 0));
  draw_line(-1.2f, -e, 1.2f, -e);
  TEST_ASSERT_TRUE((lit_in_row(0) > 0) != (lit_in_row(SIZE - 1) > 0));
  draw_line(-1.2f, -1.2f, 1.2f, 1.2f);
  draw_line(-e, -e, e, e);
  TEST_ASSERT_TRUE(glshim_is_open());
}

/* The clipper keeps a line inside the view volume, so only a viewport larger
 * than the canvas can carry one past its right or bottom edge. A column past
 * the right edge wraps to the left of the next row, still inside the buffer. */
void test_a_smooth_line_in_an_oversized_viewport_stays_inside_the_canvas(void) {
  open_line_scene(1);
  glViewport(0, 0, 2 * SIZE, 2 * SIZE);
  const float y = 1.0f - 21.0f / (2 * SIZE - 0.5f); /* the middle of row 10 */
  draw_line(-1.0f, y, 1.0f, y);
  TEST_ASSERT_EQUAL_INT(SIZE, lit_in_row(10));
  TEST_ASSERT_EQUAL_INT(0, lit_in_row(11));
  const float x = 21.0f / (2 * SIZE - 0.5f) - 1.0f;
  draw_line(x, -1.0f, x, 1.0f);
  TEST_ASSERT_EQUAL_INT(SIZE, lit_in_column(10));
  TEST_ASSERT_EQUAL_INT(0, lit_in_column(11));
}

static void a_line_is_the_same_after_asking_for_a_wide_one(int smooth) {
  open_line_scene(smooth);
  draw_line(-0.8f, 0.123f, 0.8f, 0.4f);
  uint16_t narrow[SIZE * SIZE];
  memcpy(narrow, cv.px, sizeof narrow);
  glLineWidth(4);
  draw_line(-0.8f, 0.123f, 0.8f, 0.4f);
  TEST_ASSERT_EQUAL_MEMORY(narrow, cv.px, sizeof narrow);
  glshim_close();
}

void test_line_width_changes_neither_a_smooth_nor_a_plain_line(void) {
  a_line_is_the_same_after_asking_for_a_wide_one(1);
  a_line_is_the_same_after_asking_for_a_wide_one(0);
}

void test_smoothing_can_be_switched_off_again(void) {
  open_line_scene(0);
  draw_line(-0.8f, 0.123f, 0.8f, 0.123f);
  uint16_t plain[SIZE * SIZE];
  memcpy(plain, cv.px, sizeof plain);
  glEnable(GL_LINE_SMOOTH);
  glDisable(GL_LINE_SMOOTH);
  draw_line(-0.8f, 0.123f, 0.8f, 0.123f);
  TEST_ASSERT_EQUAL_MEMORY(plain, cv.px, sizeof plain);
}

/* Fills. TinyGL carries no alpha, so SRC_ALPHA / ONE_MINUS_SRC_ALPHA is what a
 * hack gets with alpha 1: an opaque fill. TinyGL's own blend added the colours,
 * decoding our byte-swapped pixels as if they were not swapped. */
static void fill_everything_with(float r, float g, float b, float z) {
  glColor3f(r, g, b);
  glBegin(GL_TRIANGLES);
  glVertex3f(-1, -1, z);
  glVertex3f(3, -1, z);
  glVertex3f(-1, 3, z);
  glEnd();
}

static uint16_t centre_pixel(void) { return cv.px[(SIZE / 2) * SIZE + SIZE / 2]; }

void test_a_fill_blended_with_source_alpha_is_opaque(void) {
  open_line_scene(0);
  fill_everything_with(0.5f, 0.5f, 0.5f, 0);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  fill_everything_with(0, 0, 1, 0);
  TEST_ASSERT_EQUAL_HEX16(rgb565(0, 0, 255), centre_pixel());
}

void test_blending_set_up_in_the_other_order_is_opaque_too(void) {
  open_line_scene(0);
  fill_everything_with(0.5f, 0.5f, 0.5f, 0);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_BLEND);
  fill_everything_with(1, 0, 0, 0);
  TEST_ASSERT_EQUAL_HEX16(rgb565(255, 0, 0), centre_pixel());
  glDisable(GL_BLEND);
  fill_everything_with(0, 1, 0, 0);
  TEST_ASSERT_EQUAL_HEX16(rgb565(0, 255, 0), centre_pixel());
}

/* Additive blending (GL_ONE, GL_ONE; and GL_SRC_ALPHA, GL_ONE, since TinyGL
 * has no alpha) adds each channel of the canvas's byte-swapped RGB565 and
 * saturates it on its own (issue #73). TinyGL's own blend decoded the swapped
 * pixel as unswapped, which was right only for black and white. */
static uint16_t unswap(uint16_t v) { return (uint16_t)((v << 8) | (v >> 8)); }

static uint16_t saturating_sum(uint16_t a, uint16_t b) {
  const unsigned x = unswap(a), y = unswap(b);
  unsigned r = (x >> 11) + (y >> 11), g = ((x >> 5) & 63) + ((y >> 5) & 63),
           bl = (x & 31) + (y & 31);
  if (r > 31) r = 31;
  if (g > 63) g = 63;
  if (bl > 31) bl = 31;
  return unswap((uint16_t)((r << 11) | (g << 5) | bl));
}

static uint16_t alone(float r, float g, float b, int flat) {
  open_line_scene(0);
  glShadeModel(flat ? GL_FLAT : GL_SMOOTH);
  fill_everything_with(r, g, b, 0);
  return centre_pixel();
}

static uint16_t added(float r1, float g1, float b1, float r2, float g2, float b2,
                      GLenum sfactor, int flat) {
  open_line_scene(0);
  glShadeModel(flat ? GL_FLAT : GL_SMOOTH);
  fill_everything_with(r1, g1, b1, 0);
  glEnable(GL_BLEND);
  glBlendFunc(sfactor, GL_ONE);
  fill_everything_with(r2, g2, b2, 0);
  return centre_pixel();
}

void test_additive_blend_adds_each_channel(void) {
  for (int flat = 0; flat < 2; flat++) {
    const uint16_t red = alone(0.25f, 0, 0, flat), blue = alone(0, 0, 0.25f, flat);
    TEST_ASSERT_EQUAL_HEX16(saturating_sum(red, blue),
                            added(0.25f, 0, 0, 0, 0, 0.25f, GL_ONE, flat));
  }
}

void test_additive_blend_saturates_each_channel_on_its_own(void) {
  for (int flat = 0; flat < 2; flat++) {
    const uint16_t a = alone(0.8f, 0.15f, 0, flat);
    const uint16_t sum = added(0.8f, 0.15f, 0, 0.8f, 0.15f, 0, GL_ONE, flat);
    TEST_ASSERT_EQUAL_HEX16(saturating_sum(a, a), sum);
    TEST_ASSERT_EQUAL_UINT(31, unswap(sum) >> 11);                         /* red saturates */
    TEST_ASSERT_EQUAL_UINT(2 * ((unswap(a) >> 5) & 63), (unswap(sum) >> 5) & 63); /* green does not */
  }
}

void test_source_alpha_with_one_adds_too(void) {
  const uint16_t red = alone(0.25f, 0, 0, 0), green = alone(0, 0.25f, 0, 0);
  TEST_ASSERT_EQUAL_HEX16(saturating_sum(red, green),
                          added(0.25f, 0, 0, 0, 0.25f, 0, GL_SRC_ALPHA, 0));
}

/* Half resolution (issue #43, for Hypnowheel): at scale 2 TinyGL draws into a
 * half-size buffer, glViewport is halved to match, and the swap doubles each
 * pixel into the canvas. A smooth three-colour triangle comes out in 2 by 2
 * blocks only if pixels really are doubled. */
static void corner_triangle_frame(float x0, float y0) {
  glViewport(0, 0, SIZE, SIZE);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glBegin(GL_TRIANGLES);
  glColor3f(1, 0, 0);
  glVertex3f(x0, y0, 0);
  glColor3f(0, 1, 0);
  glVertex3f(1, y0, 0);
  glColor3f(0, 0, 1);
  glVertex3f(x0, 1, 0);
  glEnd();
}

void test_at_scale_two_each_pixel_becomes_a_two_by_two_block(void) {
  glshim_set_render_scale(2);
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  corner_triangle_frame(-1, -1);
  glXSwapBuffers(dpy, 0);
  int lit = 0;
  for (int y = 0; y < SIZE; y += 2)
    for (int x = 0; x < SIZE; x += 2) {
      const uint16_t v = cv.px[y * SIZE + x];
      TEST_ASSERT_EQUAL_HEX16(v, cv.px[y * SIZE + x + 1]);
      TEST_ASSERT_EQUAL_HEX16(v, cv.px[(y + 1) * SIZE + x]);
      TEST_ASSERT_EQUAL_HEX16(v, cv.px[(y + 1) * SIZE + x + 1]);
      if (v) lit++;
    }
  TEST_ASSERT_TRUE(lit > SIZE * SIZE / 16);
  TEST_ASSERT_NOT_EQUAL(0, cv.px[(SIZE - 1) * SIZE]); /* bottom left */
  TEST_ASSERT_EQUAL_HEX16(0, cv.px[SIZE - 1]);        /* top right */
}

void test_at_scale_two_the_dirty_box_is_scaled(void) {
  glshim_set_render_scale(2);
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  corner_triangle_frame(0.5f, 0.5f); /* the first frame is a full clear */
  glXSwapBuffers(dpy, 0);
  canvas_clear_dirty(&cv);
  corner_triangle_frame(0.5f, 0.5f); /* top right only */
  glXSwapBuffers(dpy, 0);
  int x0, x1, rows = 0;
  for (int y = 0; y < SIZE; y++) {
    if (!canvas_dirty_row(&cv, y, &x0, &x1)) continue;
    rows++;
    TEST_ASSERT_TRUE(y < SIZE / 2);
    TEST_ASSERT_EQUAL_INT(SIZE - 1, x1); /* the doubled right edge */
    TEST_ASSERT_TRUE(x0 >= SIZE / 2 && x0 % 2 == 0);
  }
  TEST_ASSERT_TRUE(rows > 0 && rows % 2 == 0);
}

static int canvas_is_two_by_two_blocks(void) {
  for (int y = 0; y < SIZE; y += 2)
    for (int x = 0; x < SIZE; x += 2) {
      const uint16_t v = cv.px[y * SIZE + x];
      if (cv.px[y * SIZE + x + 1] != v || cv.px[(y + 1) * SIZE + x] != v) return 0;
    }
  return 1;
}

/* Opening while a context is open closes it first; that must not lose a
 * scale asked for since. */
void test_a_scale_asked_for_while_a_context_is_open_is_kept(void) {
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  glshim_set_render_scale(2);
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  corner_triangle_frame(-1, -1);
  glXSwapBuffers(dpy, 0);
  TEST_ASSERT_TRUE(canvas_is_two_by_two_blocks());
}

/* A hack that asks for scale 2 and then fails to open, or never opens, must
 * not leave it for the next hack. */
void test_a_failed_open_does_not_leave_the_scale_behind(void) {
  Canvas empty = {0};
  glshim_set_render_scale(2);
  TEST_ASSERT_NULL(glshim_open(&empty));
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  corner_triangle_frame(-1, -1); /* at scale 1 it lands on the canvas at once */
  TEST_ASSERT_NOT_EQUAL(0, cv.px[(SIZE - 1) * SIZE]);
}

void test_a_scale_never_opened_is_dropped_when_the_hack_stops(void) {
  glshim_set_render_scale(2);
  glshim_close(); /* the release hook runs even if the hack never opened */
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  corner_triangle_frame(-1, -1);
  TEST_ASSERT_NOT_EQUAL(0, cv.px[(SIZE - 1) * SIZE]);
}

void test_closing_resets_the_scale(void) {
  glshim_set_render_scale(2);
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  glshim_close();
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  corner_triangle_frame(-1, -1); /* at scale 1 it lands on the canvas at once */
  TEST_ASSERT_NOT_EQUAL(0, cv.px[(SIZE - 1) * SIZE]);
}

/* Polygon offset. A second fill at exactly the same depth wins a tie, since
 * TinyGL's test is >=. With the offset enabled it is pushed back and loses. */
static uint16_t second_fill_behind_the_first(int enable, float factor, float units,
                                             float z_slope, float back) {
  open_line_scene(0);
  glEnable(GL_DEPTH_TEST);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  for (int pass = 0; pass < 2; pass++) {
    if (pass == 1) {
      glPolygonOffset(factor, units);
      if (enable) glEnable(GL_POLYGON_OFFSET_FILL);
    }
    glColor3f(pass ? 1.0f : 0.0f, pass ? 0.0f : 1.0f, 0);
    glBegin(GL_TRIANGLES);
    const float z = pass ? back : 0.0f;
    glVertex3f(-1, -1, -z_slope + z);
    glVertex3f(3, -1, z_slope * 3 + z);
    glVertex3f(-1, 3, -z_slope + z);
    glEnd();
  }
  const uint16_t result = centre_pixel();
  glshim_close();
  return result;
}

static uint16_t second_fill_over_the_first(int enable, float factor, float units,
                                           float z_slope) {
  return second_fill_behind_the_first(enable, factor, units, z_slope, 0.0f);
}

void test_a_coplanar_fill_wins_the_depth_tie_without_an_offset(void) {
  TEST_ASSERT_EQUAL_HEX16(rgb565(255, 0, 0), second_fill_over_the_first(0, 1, 1, 0));
  TEST_ASSERT_EQUAL_HEX16(rgb565(255, 0, 0), second_fill_over_the_first(0, 1, 1, 0.5f));
}

void test_a_positive_offset_pushes_a_fill_back_behind_its_twin(void) {
  TEST_ASSERT_EQUAL_HEX16(rgb565(0, 255, 0), second_fill_over_the_first(1, 0, 1, 0));
}

void test_the_offset_factor_scales_with_the_slope_of_the_fill(void) {
  TEST_ASSERT_EQUAL_HEX16(rgb565(255, 0, 0), second_fill_over_the_first(1, 0, 0, 0.5f));
  TEST_ASSERT_EQUAL_HEX16(rgb565(0, 255, 0), second_fill_over_the_first(1, 1, 0, 0.5f));
}

void test_a_negative_offset_pulls_a_fill_in_front_of_one_just_ahead_of_it(void) {
  const float back = 0.002f;
  TEST_ASSERT_EQUAL_HEX16(rgb565(0, 255, 0), second_fill_behind_the_first(0, -1, -1, 0.5f, back));
  TEST_ASSERT_EQUAL_HEX16(rgb565(255, 0, 0), second_fill_behind_the_first(1, -1, -1, 0.5f, back));
  const float one_step_back = 0.00004f; /* a little over one 16-bit depth step */
  TEST_ASSERT_EQUAL_HEX16(rgb565(0, 255, 0), second_fill_behind_the_first(0, 0, -1, 0.0f, one_step_back));
  TEST_ASSERT_EQUAL_HEX16(rgb565(255, 0, 0), second_fill_behind_the_first(1, 0, -1, 0.0f, one_step_back));
}

/* Depth is 16 bits. An offset that carries a fill past either end of the range
 * must stop at the end, not wrap round to the other. */
static uint16_t offset_fill_over_a_fill_at_zero(float second_z, float units) {
  open_line_scene(0);
  glEnable(GL_DEPTH_TEST);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  fill_everything_with(0, 1, 0, 0.0f);
  glPolygonOffset(0, units);
  glEnable(GL_POLYGON_OFFSET_FILL);
  fill_everything_with(1, 0, 0, second_z);
  glDisable(GL_POLYGON_OFFSET_FILL);
  fill_everything_with(0, 0, 1, 0.0f); /* beats a depth that wrapped round */
  const uint16_t result = centre_pixel();
  glshim_close();
  return result;
}

void test_an_offset_cannot_wrap_a_fill_past_the_far_plane(void) {
  TEST_ASSERT_EQUAL_HEX16(rgb565(0, 0, 255), offset_fill_over_a_fill_at_zero(0.999999f, 1));
}

void test_an_offset_cannot_wrap_a_fill_past_the_near_plane(void) {
  TEST_ASSERT_EQUAL_HEX16(rgb565(255, 0, 0), offset_fill_over_a_fill_at_zero(-0.999999f, -1));
}

void test_an_enormous_offset_pushes_back_instead_of_overflowing(void) {
  TEST_ASSERT_EQUAL_HEX16(rgb565(0, 255, 0), second_fill_over_the_first(1, 0, 1e9f, 0));
  TEST_ASSERT_EQUAL_HEX16(rgb565(255, 0, 0), second_fill_over_the_first(1, 0, -1e9f, 0));
}

void test_a_zero_offset_leaves_the_tie_to_the_later_fill(void) {
  TEST_ASSERT_EQUAL_HEX16(rgb565(255, 0, 0), second_fill_over_the_first(1, 0, 0, 0));
}

/* A line on the edge of its own opaque polygon, as in Unknown Pleasures: each
 * row is a smooth line drawn over the top edge of a black quad on a plane that
 * tilts away from the viewer, with the offset the hack asks for. TinyGL snaps
 * the quad's vertices to whole pixels, so as the pair drifts the quad's depth
 * at the line's pixels moves by up to a pixel's worth of slope, and the quad
 * ate parts of its line. */
static double line_brightness_on_its_quad(float shift, float tilt) {
  open_line_scene(1);
  glEnable(GL_DEPTH_TEST);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(0.5f, 0.5f);
  const float top = 0.1f + shift, depth = 0.3f;
  glColor3f(1, 1, 1);
  glBegin(GL_LINE_STRIP);
  glVertex3f(-0.7f, top, depth);
  glVertex3f(0.7f, top, depth);
  glEnd();
  glColor3f(0, 0, 0);
  glBegin(GL_QUAD_STRIP);
  glVertex3f(-0.7f, top, depth);
  glVertex3f(-0.7f, top - 0.4f, depth - 0.4f * tilt);
  glVertex3f(0.7f, top, depth);
  glVertex3f(0.7f, top - 0.4f, depth - 0.4f * tilt);
  glEnd();
  double total, centroid;
  measure_line(0, &total, &centroid);
  glshim_close();
  return total;
}

void test_a_smooth_line_is_not_eaten_by_its_own_polygon_as_they_drift(void) {
  const float tilts[] = {0.5f, 1.0f, 2.0f};
  for (int t = 0; t < 3; t++) {
    double best = 0, worst = 1e30;
    for (int k = 0; k < 16; k++) {
      const double b = line_brightness_on_its_quad(k * 0.0625f * PIXEL_NDC, tilts[t]);
      if (b > best) best = b;
      if (b < worst) worst = b;
    }
    TEST_ASSERT_TRUE_MESSAGE(best > 0, "nothing drawn");
    TEST_ASSERT_TRUE_MESSAGE(worst >= 0.98 * best, "the polygon ate part of the line");
  }
}

void test_the_device_is_never_turned(void) {
  TEST_ASSERT_TRUE(current_device_rotation() == 0.0);
}

/* Lighting reads the material when a vertex is shaded. With lazy lighting a
 * vertex is shaded when its triangle is drawn, so a material change between
 * two vertices of one triangle must first shade the vertices before it: the
 * first two stay red and the third is green, as GL orders it. Built and run
 * with and without the flag. */
void test_a_material_change_mid_triangle_lights_earlier_vertices_with_the_old_one(void) {
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  const GLfloat red[4] = {1, 0, 0, 1}, green[4] = {0, 1, 0, 1};
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glNormal3f(0, 0, 1);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, red);
  glBegin(GL_TRIANGLES);
  glVertex3f(-0.5f, -0.5f, 0);
  glVertex3f(0.5f, -0.5f, 0);
  glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, green);
  glVertex3f(0, 0.5f, 0);
  glEnd();
  const GLVertex *v = gl_get_context()->vertex;
  for (int i = 0; i < 2; i++) {
    TEST_ASSERT_TRUE_MESSAGE(v[i].color.v[0] > 0.9f, "vertex before the change");
    TEST_ASSERT_TRUE_MESSAGE(v[i].color.v[1] < 0.1f, "vertex before the change");
  }
  TEST_ASSERT_TRUE(v[2].color.v[0] < 0.1f);
  TEST_ASSERT_TRUE(v[2].color.v[1] > 0.9f);
}

#ifndef GL_EAGER_SHADE
/* Lazy lighting is the default (issue #43): a culled triangle's vertices are
 * left waiting to be lit, and GL_EAGER_SHADE goes back to lighting each
 * vertex as it arrives. */
void test_lazy_lighting_is_the_default(void) {
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glEnable(GL_CULL_FACE);
  glBegin(GL_TRIANGLES);
  glVertex3f(-0.5f, -0.5f, 0); /* clockwise: a back face */
  glVertex3f(0, 0.5f, 0);
  glVertex3f(0.5f, -0.5f, 0);
  glEnd();
  const GLVertex *v = gl_get_context()->vertex;
  for (int i = 0; i < 3; i++) TEST_ASSERT_EQUAL_INT(1, v[i].shade_pending);
}
#endif

#if defined(GL_PROFILE) && !defined(GL_EAGER_SHADE)
/* A culled triangle's vertices are never lit; a drawn one's are, once each. */
void test_lazy_lighting_skips_the_vertices_of_culled_triangles(void) {
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glEnable(GL_CULL_FACE);
  glshim_prof_reset();
  glBegin(GL_TRIANGLES);
  glVertex3f(-0.5f, -0.5f, 0); /* clockwise: a back face */
  glVertex3f(0, 0.5f, 0);
  glVertex3f(0.5f, -0.5f, 0);
  glEnd();
  TEST_ASSERT_EQUAL_UINT32(0, glshim_prof_calls[GLPROF_SHADE]);
  glBegin(GL_TRIANGLE_STRIP);
  glVertex3f(-0.5f, -0.5f, 0);
  glVertex3f(0.5f, -0.5f, 0);
  glVertex3f(-0.5f, 0.5f, 0);
  glVertex3f(0.5f, 0.5f, 0);
  glEnd();
  TEST_ASSERT_EQUAL_UINT32(2, glshim_prof_calls[GLPROF_FILL]);
  TEST_ASSERT_EQUAL_UINT32(4, glshim_prof_calls[GLPROF_SHADE]);
}
#endif

#ifdef GL_PROFILE
/* The profile counts each stage once per call, and a list called from a list
 * counts once: a nested list must not add its time to its caller's twice. */
void test_the_profile_counts_each_stage_once_per_call(void) {
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  const GLuint inner = glGenLists(2), outer = inner + 1;
  glNewList(inner, GL_COMPILE);
  glBegin(GL_TRIANGLES);
  glVertex3f(-0.5f, -0.5f, 0);
  glVertex3f(0.5f, -0.5f, 0);
  glVertex3f(0, 0.5f, 0);
  glEnd();
  glEndList();
  glNewList(outer, GL_COMPILE);
  glCallList(inner);
  glEndList();
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glshim_prof_reset();
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glCallList(outer);
  glXSwapBuffers(dpy, 0);
  TEST_ASSERT_EQUAL_UINT32(1, glshim_prof_frames);
  TEST_ASSERT_EQUAL_UINT32(1, glshim_prof_calls[GLPROF_CLEAR]);
  TEST_ASSERT_EQUAL_UINT32(1, glshim_prof_calls[GLPROF_LIST]);
  TEST_ASSERT_EQUAL_UINT32(3, glshim_prof_calls[GLPROF_VERTEX]);
  TEST_ASSERT_EQUAL_UINT32(3, glshim_prof_calls[GLPROF_SHADE]);
  TEST_ASSERT_EQUAL_UINT32(1, glshim_prof_calls[GLPROF_TRIANGLE]);
  TEST_ASSERT_EQUAL_UINT32(1, glshim_prof_calls[GLPROF_FILL]);
  TEST_ASSERT_EQUAL_INT(0, glshim_prof_depth);
  TEST_ASSERT_TRUE(glshim_prof_time[GLPROF_LIST] >= glshim_prof_time[GLPROF_VERTEX]);
  TEST_ASSERT_TRUE(glshim_prof_time[GLPROF_TRIANGLE] >= glshim_prof_time[GLPROF_FILL]);
}
#endif

/* TinyGL is built without GL_POLYGON, which also drops GL_LINE_LOOP, and an
 * unhandled primitive kept counting vertices: the fifth was written past
 * c->vertex, over the matrices and colours that follow it (Pipes' buildlwo.c
 * draws polygons; the context's light list was overwritten). Such a primitive
 * still draws nothing, but must leave the rest of the context alone. */
void test_an_unhandled_primitive_never_writes_past_the_vertex_buffer(void) {
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glBegin(GL_TRIANGLES); /* brings the derived matrices up to date */
  glEnd();
  GLContext *c = gl_get_context();
  const char *from = (const char *)&c->matrix_model_view_inv;
  const size_t n = (const char *)(&c->first_light + 1) - from;
  char before[1024];
  TEST_ASSERT_TRUE(n <= sizeof before);
  memcpy(before, from, n);
  const GLenum kinds[2] = {GL_POLYGON, GL_LINE_LOOP};
  for (int k = 0; k < 2; k++) {
    glBegin(kinds[k]);
    for (int i = 0; i < 12; i++) glVertex3f(cosf(i * 0.5f), sinf(i * 0.5f), 0);
    glEnd();
  }
  TEST_ASSERT_EQUAL_MEMORY(before, from, n);
}

/* GL_LINE_LOOP is patched in without GL_POLYGON (issue #80): StonerView
 * outlines its squares with it and Unknown Pleasures draws its rim. A square
 * has four edges, so two rows and two columns are long runs of lit pixels; a
 * loop that forgot the closing edge (last vertex back to the first) has one
 * column. */
static int rows_with_a_long_run(void) {
  int n = 0;
  for (int y = 0; y < SIZE; y++) n += lit_in_row(y) >= SIZE / 4;
  return n;
}

static int columns_with_a_long_run(void) {
  int n = 0;
  for (int x = 0; x < SIZE; x++) n += lit_in_column(x) >= SIZE / 4;
  return n;
}

void test_a_line_loop_draws_every_edge_including_the_closing_one(void) {
  open_line_scene(0);
  glBegin(GL_LINE_LOOP);
  glVertex3f(-0.5f, -0.5f, 0);
  glVertex3f(0.5f, -0.5f, 0);
  glVertex3f(0.5f, 0.5f, 0);
  glVertex3f(-0.5f, 0.5f, 0);
  glEnd();
  TEST_ASSERT_EQUAL_INT(2, rows_with_a_long_run());
  TEST_ASSERT_EQUAL_INT(2, columns_with_a_long_run());
}

void test_a_line_loop_starts_afresh_after_each_glEnd(void) {
  open_line_scene(0);
  for (int pass = 0; pass < 2; pass++) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBegin(GL_LINE_LOOP);
    glVertex3f(-0.5f, -0.5f, 0);
    glVertex3f(0.5f, -0.5f, 0);
    glVertex3f(0.5f, 0.5f, 0);
    glVertex3f(-0.5f, 0.5f, 0);
    glEnd();
    TEST_ASSERT_EQUAL_INT(2, rows_with_a_long_run());
    TEST_ASSERT_EQUAL_INT(2, columns_with_a_long_run());
  }
}

/* The round panel (issue #77): with a disc on the canvas, clears skip the
 * corners nobody sees. */
#define SENTINEL 0x5A5A

static void assert_disc_is(uint16_t v) {
  for (int y = 0; y < SIZE; y++) {
    int x0, x1;
    if (!canvas_visible_span(&cv, y, &x0, &x1)) continue;
    for (int x = x0; x <= x1; x++) TEST_ASSERT_EQUAL_HEX16(v, cv.px[y * SIZE + x]);
  }
}

void test_no_disc_clear_is_unchanged(void) {
  canvas_clear(&cv, SENTINEL);
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT);
  TEST_ASSERT_EQUAL_HEX16(0, cv.px[0]);
  TEST_ASSERT_EQUAL_HEX16(0, cv.px[SIZE * SIZE - 1]);
  TEST_ASSERT_NULL(glshim_span_x0);
}

void test_disc_clear_leaves_the_corners_alone(void) {
  TEST_ASSERT_EQUAL_INT(0, canvas_set_disc(&cv, malloc));
  canvas_clear(&cv, SENTINEL);
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT);
  TEST_ASSERT_EQUAL_HEX16(SENTINEL, cv.px[0]);
  TEST_ASSERT_EQUAL_HEX16(SENTINEL, cv.px[SIZE * SIZE - 1]);
  assert_disc_is(0);
}

void test_disc_depth_clear_is_clipped_too(void) {
  TEST_ASSERT_EQUAL_INT(0, canvas_set_disc(&cv, malloc));
  ZBuffer *zb = *glshim_open(&cv);
  for (int i = 0; i < SIZE * SIZE; i++) zb->zbuf[i] = SENTINEL;
  glEnable(GL_DEPTH_TEST);
  glClear(GL_DEPTH_BUFFER_BIT);
  TEST_ASSERT_EQUAL_HEX16(SENTINEL, zb->zbuf[0]);
  for (int y = 0; y < SIZE; y++) {
    int x0, x1;
    TEST_ASSERT_TRUE(canvas_visible_span(&cv, y, &x0, &x1));
    TEST_ASSERT_NOT_EQUAL(SENTINEL, zb->zbuf[y * SIZE + x0]);
    TEST_ASSERT_NOT_EQUAL(SENTINEL, zb->zbuf[y * SIZE + x1]);
  }
}

void test_disc_scale_two_clears_every_block_touching_the_disc(void) {
  TEST_ASSERT_EQUAL_INT(0, canvas_set_disc(&cv, malloc));
  canvas_clear(&cv, SENTINEL);
  glshim_set_render_scale(2);
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT);
  glXSwapBuffers(dpy, 0);
  assert_disc_is(0);
  TEST_ASSERT_NOT_NULL(glshim_span_x0);
  TEST_ASSERT_EQUAL_HEX16(SENTINEL, cv.px[0]);
}

void test_the_disc_spans_go_with_the_context(void) {
  TEST_ASSERT_EQUAL_INT(0, canvas_set_disc(&cv, malloc));
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  TEST_ASSERT_NOT_NULL(glshim_span_x0);
  TEST_ASSERT_NOT_NULL(glshim_span_x1);
  glshim_close();
  TEST_ASSERT_NULL(glshim_span_x0);
  TEST_ASSERT_NULL(glshim_span_x1);
}

static void flat_view(void) {
  glViewport(0, 0, SIZE, SIZE);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glDisable(GL_LIGHTING);
}

void test_disc_fill_leaves_the_corners_alone(void) {
  TEST_ASSERT_EQUAL_INT(0, canvas_set_disc(&cv, malloc));
  canvas_clear(&cv, SENTINEL);
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  flat_view();
  glColor3f(1, 0, 0);
  glBegin(GL_TRIANGLES);
  glVertex3f(-1, -1, 0);
  glVertex3f(1, -1, 0);
  glVertex3f(1, 1, 0);
  glVertex3f(-1, -1, 0);
  glVertex3f(1, 1, 0);
  glVertex3f(-1, 1, 0);
  glEnd();
  TEST_ASSERT_EQUAL_HEX16(SENTINEL, cv.px[0]);
  TEST_ASSERT_EQUAL_HEX16(SENTINEL, cv.px[SIZE - 1]);
  TEST_ASSERT_EQUAL_HEX16(SENTINEL, cv.px[(SIZE - 1) * SIZE]);
  TEST_ASSERT_EQUAL_HEX16(SENTINEL, cv.px[SIZE * SIZE - 1]);
  assert_disc_is(rgb565(255, 0, 0));
}

/* Every vertex is right of the disc's left edge; only the top right one is
 * past its right edge, so that test alone must turn the clip on. */
void test_disc_fill_clips_a_triangle_out_only_on_the_right(void) {
  TEST_ASSERT_EQUAL_INT(0, canvas_set_disc(&cv, malloc));
  canvas_clear(&cv, SENTINEL);
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  flat_view();
  glColor3f(1, 0, 0);
  glBegin(GL_TRIANGLES);
  glVertex3f(0, 0, 0);
  glVertex3f(1, 1, 0);
  glVertex3f(0, 1, 0);
  glEnd();
  TEST_ASSERT_EQUAL_HEX16(SENTINEL, cv.px[SIZE - 1]);
  TEST_ASSERT_EQUAL_HEX16(rgb565(255, 0, 0), cv.px[8 * SIZE + 40]);
}

/* Two smooth, depth-tested triangles over the top left corner, where the disc
 * cuts every scanline short on the left: clipping must start each scanline
 * with the depth and colour it would have had, so inside the disc the frame
 * matches one drawn without a disc. */
static void two_triangles_in_the_corner(void) {
  flat_view();
  glEnable(GL_DEPTH_TEST);
  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glBegin(GL_TRIANGLES);
  glColor3f(1, 0, 0);
  glVertex3f(-1, 1, -0.6f);
  glColor3f(0, 1, 0);
  glVertex3f(0.3f, 1, 0.8f);
  glColor3f(0, 0, 1);
  glVertex3f(-1, -0.3f, 0.8f);
  glColor3f(1, 1, 0);
  glVertex3f(-1, 0.9f, 0.1f);
  glColor3f(0, 1, 1);
  glVertex3f(0.2f, 0.9f, 0.1f);
  glColor3f(1, 0, 1);
  glVertex3f(-1, -0.2f, 0.1f);
  glEnd();
}

void test_disc_clipped_triangle_matches_the_unclipped_one_inside(void) {
  uint16_t plain[SIZE * SIZE];
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  two_triangles_in_the_corner();
  memcpy(plain, cv.px, sizeof plain);
  TEST_ASSERT_EQUAL_INT(0, canvas_set_disc(&cv, malloc));
  canvas_clear(&cv, SENTINEL);
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  two_triangles_in_the_corner();
  int checked = 0;
  for (int y = 0; y < SIZE; y++) {
    int x0, x1;
    if (!canvas_visible_span(&cv, y, &x0, &x1)) continue;
    for (int x = x0; x <= x1; x++, checked++)
      TEST_ASSERT_EQUAL_HEX16(plain[y * SIZE + x], cv.px[y * SIZE + x]);
  }
  TEST_ASSERT_EQUAL_HEX16(SENTINEL, cv.px[0]);
  TEST_ASSERT_NOT_EQUAL(0, plain[0]); /* the triangles do reach the corner */
  TEST_ASSERT_TRUE(checked > SIZE * SIZE / 2);
}

/* At scale 2 the fill reads the half-size spans, a row per half-res line. */
void test_disc_clipped_triangle_matches_the_unclipped_one_at_scale_two(void) {
  uint16_t plain[SIZE * SIZE];
  glshim_set_render_scale(2);
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  two_triangles_in_the_corner();
  glXSwapBuffers(dpy, 0);
  memcpy(plain, cv.px, sizeof plain);
  TEST_ASSERT_EQUAL_INT(0, canvas_set_disc(&cv, malloc));
  canvas_clear(&cv, SENTINEL);
  glshim_set_render_scale(2);
  TEST_ASSERT_NOT_NULL(glshim_open(&cv));
  two_triangles_in_the_corner();
  glXSwapBuffers(dpy, 0);
  for (int y = 0; y < SIZE; y++) {
    int x0, x1;
    if (!canvas_visible_span(&cv, y, &x0, &x1)) continue;
    for (int x = x0; x <= x1; x++)
      TEST_ASSERT_EQUAL_HEX16(plain[y * SIZE + x], cv.px[y * SIZE + x]);
  }
  TEST_ASSERT_EQUAL_HEX16(SENTINEL, cv.px[0]);
  TEST_ASSERT_NOT_EQUAL(0, plain[0]);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_disc_clipped_triangle_matches_the_unclipped_one_at_scale_two);
  RUN_TEST(test_disc_fill_leaves_the_corners_alone);
  RUN_TEST(test_disc_fill_clips_a_triangle_out_only_on_the_right);
  RUN_TEST(test_disc_clipped_triangle_matches_the_unclipped_one_inside);
  RUN_TEST(test_no_disc_clear_is_unchanged);
  RUN_TEST(test_disc_clear_leaves_the_corners_alone);
  RUN_TEST(test_disc_depth_clear_is_clipped_too);
  RUN_TEST(test_disc_scale_two_clears_every_block_touching_the_disc);
  RUN_TEST(test_the_disc_spans_go_with_the_context);
  RUN_TEST(test_an_unhandled_primitive_never_writes_past_the_vertex_buffer);
  RUN_TEST(test_a_line_loop_draws_every_edge_including_the_closing_one);
  RUN_TEST(test_a_line_loop_starts_afresh_after_each_glEnd);
#ifdef GL_PROFILE
  RUN_TEST(test_the_profile_counts_each_stage_once_per_call);
#endif
  RUN_TEST(test_a_material_change_mid_triangle_lights_earlier_vertices_with_the_old_one);
#ifndef GL_EAGER_SHADE
  RUN_TEST(test_lazy_lighting_is_the_default);
#endif
#if defined(GL_PROFILE) && !defined(GL_EAGER_SHADE)
  RUN_TEST(test_lazy_lighting_skips_the_vertices_of_culled_triangles);
#endif
  RUN_TEST(test_the_device_is_never_turned);
  RUN_TEST(test_a_smooth_horizontal_line_moves_a_quarter_pixel_at_a_time);
  RUN_TEST(test_a_smooth_vertical_line_moves_a_quarter_pixel_at_a_time);
  RUN_TEST(test_a_line_without_smoothing_still_moves_a_whole_pixel_at_a_time);
  RUN_TEST(test_a_smooth_sloped_line_has_one_pixel_of_brightness_a_column);
  RUN_TEST(test_a_smooth_line_obeys_the_depth_test);
  RUN_TEST(test_a_smooth_line_at_the_canvas_edge_stays_inside_it);
  RUN_TEST(test_a_smooth_line_in_an_oversized_viewport_stays_inside_the_canvas);
  RUN_TEST(test_line_width_changes_neither_a_smooth_nor_a_plain_line);
  RUN_TEST(test_smoothing_can_be_switched_off_again);
  RUN_TEST(test_a_fill_blended_with_source_alpha_is_opaque);
  RUN_TEST(test_at_scale_two_each_pixel_becomes_a_two_by_two_block);
  RUN_TEST(test_at_scale_two_the_dirty_box_is_scaled);
  RUN_TEST(test_closing_resets_the_scale);
  RUN_TEST(test_a_scale_asked_for_while_a_context_is_open_is_kept);
  RUN_TEST(test_a_failed_open_does_not_leave_the_scale_behind);
  RUN_TEST(test_a_scale_never_opened_is_dropped_when_the_hack_stops);
  RUN_TEST(test_additive_blend_adds_each_channel);
  RUN_TEST(test_additive_blend_saturates_each_channel_on_its_own);
  RUN_TEST(test_source_alpha_with_one_adds_too);
  RUN_TEST(test_blending_set_up_in_the_other_order_is_opaque_too);
  RUN_TEST(test_a_coplanar_fill_wins_the_depth_tie_without_an_offset);
  RUN_TEST(test_a_positive_offset_pushes_a_fill_back_behind_its_twin);
  RUN_TEST(test_the_offset_factor_scales_with_the_slope_of_the_fill);
  RUN_TEST(test_a_negative_offset_pulls_a_fill_in_front_of_one_just_ahead_of_it);
  RUN_TEST(test_an_offset_cannot_wrap_a_fill_past_the_far_plane);
  RUN_TEST(test_an_offset_cannot_wrap_a_fill_past_the_near_plane);
  RUN_TEST(test_an_enormous_offset_pushes_back_instead_of_overflowing);
  RUN_TEST(test_a_zero_offset_leaves_the_tie_to_the_later_fill);
  RUN_TEST(test_a_smooth_line_is_not_eaten_by_its_own_polygon_as_they_drift);
  RUN_TEST(test_recompiling_a_list_under_the_same_name_does_not_grow_memory);
  RUN_TEST(test_line_width_is_accepted_and_changes_nothing);
  RUN_TEST(test_draw_arrays_honours_a_byte_stride);
  RUN_TEST(test_a_display_list_keeps_the_arrays_it_recorded);
  RUN_TEST(test_a_new_context_starts_with_no_client_arrays_enabled);
  RUN_TEST(test_draw_arrays_with_nothing_enabled_or_a_zero_count_draws_nothing);
  RUN_TEST(test_clear_writes_canvas_byte_order);
  RUN_TEST(test_the_first_list_name_is_not_zero);
  RUN_TEST(test_perspective_and_lookat_put_a_point_where_the_maths_says);
  RUN_TEST(test_ortho_puts_a_point_where_the_maths_says);
  RUN_TEST(test_ortho_maps_depth_so_the_nearer_triangle_wins);
  RUN_TEST(test_ortho_clips_at_the_near_and_far_planes);
  RUN_TEST(test_materiali_sets_the_shininess_tinygl_uses);
  RUN_TEST(test_a_region_clear_still_leaves_the_whole_canvas_clear);
  RUN_TEST(test_a_line_drawn_in_any_direction_is_cleared_again);
  RUN_TEST(test_text_is_cleared_again);
  RUN_TEST(test_the_first_clear_after_opening_clears_over_whatever_was_there);
  RUN_TEST(test_a_new_clear_colour_clears_everything);
  RUN_TEST(test_stale_depth_does_not_hide_the_next_frame);
  RUN_TEST(test_a_depth_clear_does_nothing_while_the_depth_test_is_off);
  RUN_TEST(test_a_depth_clear_made_with_the_test_off_applies_once_it_is_on);
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
