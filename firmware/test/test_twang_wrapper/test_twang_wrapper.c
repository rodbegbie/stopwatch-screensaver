#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include "core/canvas.h"
#include "x11shim/xshim.h"

/* Includes the device wrapper itself, so the hack runs as it will on the
 * board. The renamed entry keeps it from clashing with the copy in the
 * registry. */
#define twang_hack twang_hack_under_test
#include "hacks/twang_single.c"

static Canvas cv;
static Display *dpy;

void setUp(void) {
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&cv, 128, 128, malloc));
  dpy = xshim_open_display(&cv);
  xshim_set_defaults((const char *const *)twang_defaults);
}
void tearDown(void) {
  xshim_set_defaults(NULL);
  xshim_close_display(dpy);
  canvas_free(&cv);
}

/* Upstream takes its picture the first time its draw finds the image load
 * finished. The shim finishes a load at once, so that moment never came and
 * the hack drew with no source image. */
void test_the_first_draw_finds_a_source_image_and_a_work_image_ready(void) {
  struct state *st = twang_init(dpy, 1);
  TEST_ASSERT_NOT_NULL(st->sourceImage);
  TEST_ASSERT_NOT_NULL(st->workImage);
  TEST_ASSERT_TRUE(st->tileCount > 0);
  twang_draw(dpy, 1, st);
  twang_free(dpy, 1, st);
}

/* The tiles show the picture the load painted, in canvas order: a flat
 * picture must reach the window as that colour. */
void test_the_tiles_show_the_picture_the_load_painted(void) {
  static unsigned char flat[8 + 2 + 1];
  memcpy(flat, "565M", 4);
  flat[4] = 1;
  flat[6] = 1;
  flat[8] = 0x34;
  flat[9] = 0x12;
  flat[10] = 0x80;
  xshim_set_grab_image(flat, sizeof(flat));
  struct state *st = twang_init(dpy, 1);
  for (int i = 0; i < 10; i++) twang_draw(dpy, 1, st);
  int painted = 0;
  for (int i = 0; i < cv.w * cv.h; i++)
    if (cv.px[i] == px_swap(0x1234)) painted++;
  TEST_ASSERT_TRUE(painted > cv.w * cv.h / 4);
  twang_free(dpy, 1, st);
  xshim_set_grab_image(NULL, 0);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_the_first_draw_finds_a_source_image_and_a_work_image_ready);
  RUN_TEST(test_the_tiles_show_the_picture_the_load_painted);
  return UNITY_END();
}
