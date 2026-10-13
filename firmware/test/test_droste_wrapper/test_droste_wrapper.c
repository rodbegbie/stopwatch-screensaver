#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include "core/canvas.h"
#include "x11shim/xshim.h"

/* Includes the device wrapper itself, so the hack runs as it will on the
 * board. The renamed entry keeps it from clashing with the copy in the
 * registry. */
#define droste_hack droste_hack_under_test
#include "hacks/droste_small.c"

static Canvas cv;
static Display *dpy;

void setUp(void) {
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&cv, 64, 64, malloc));
  dpy = xshim_open_display(&cv);
  xshim_set_defaults((const char *const *)droste_defaults);
}
void tearDown(void) {
  xshim_set_defaults(NULL);
  xshim_close_display(dpy);
  canvas_free(&cv);
}

static void use_flat_picture(void) {
  static unsigned char flat[8 + 2 + 1];
  memcpy(flat, "565M", 4);
  flat[4] = 1;
  flat[6] = 1;
  flat[8] = 0x34;
  flat[9] = 0x12;
  flat[10] = 0x80;
  xshim_set_grab_image(flat, sizeof(flat));
}

/* Upstream sets up its input and output images the first time a poll finds
 * the load finished. The shim finishes at once and returned no loader, so that
 * moment never came and the hack rendered from a NULL image. */
void test_the_hack_gets_its_images_once_the_load_has_finished(void) {
  struct state *st = droste_init(dpy, 1);
  droste_draw(dpy, 1, st);
  droste_draw(dpy, 1, st);
  TEST_ASSERT_NULL(st->img_loader);
  TEST_ASSERT_NOT_NULL(st->in);
  TEST_ASSERT_NOT_NULL(st->out);
  droste_draw(dpy, 1, st);
  droste_free(dpy, 1, st);
}

/* Upstream loads a picture 2.5 times the window each way, which as a 32-bit
 * image would be 5 MB for a 466 pixel window. The wrapper loads one the size of
 * the window. */
void test_the_picture_is_the_size_of_the_window_not_two_and_a_half_times_it(void) {
  struct state *st = droste_init(dpy, 1);
  droste_draw(dpy, 1, st);
  droste_draw(dpy, 1, st);
  TEST_ASSERT_EQUAL_INT(cv.w, st->in->width);
  TEST_ASSERT_EQUAL_INT(cv.h, st->in->height);
  droste_free(dpy, 1, st);
}

/* The window shows the picture the load painted, in canvas order, or black
 * where the zoom runs off its edge: nothing else. */
void test_the_window_shows_the_picture_or_black(void) {
  use_flat_picture();
  struct state *st = droste_init(dpy, 1);
  for (int i = 0; i < 8; i++) droste_draw(dpy, 1, st);
  int painted = 0, other = 0;
  for (int i = 0; i < cv.w * cv.h; i++) {
    if (cv.px[i] == px_swap(0x1234)) painted++;
    else if (cv.px[i] != 0) other++;
  }
  TEST_ASSERT_TRUE(painted > cv.w * cv.h / 4);
  TEST_ASSERT_EQUAL_INT(0, other);
  droste_free(dpy, 1, st);
  xshim_set_grab_image(NULL, 0);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_the_hack_gets_its_images_once_the_load_has_finished);
  RUN_TEST(test_the_picture_is_the_size_of_the_window_not_two_and_a_half_times_it);
  RUN_TEST(test_the_window_shows_the_picture_or_black);
  return UNITY_END();
}
