#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include "core/canvas.h"
#include "x11shim/xshim.h"

/* Includes the device wrapper itself, so the hack runs as it will on the
 * board. The renamed entry keeps it from clashing with the copy in the
 * registry. */
#define rotzoomer_hack rotzoomer_hack_under_test
#include "hacks/rotzoomer_single.c"

static Canvas cv;
static Display *dpy;

void setUp(void) {
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&cv, 64, 64, malloc));
  dpy = xshim_open_display(&cv);
  xshim_set_defaults((const char *const *)rotzoomer_defaults);
}
void tearDown(void) {
  xshim_set_defaults(NULL);
  xshim_close_display(dpy);
  canvas_free(&cv);
}

/* Upstream sets up its picture and its zoom boxes the first time its draw
 * finds the load finished. The shim finishes a load at once, so that moment
 * never came and the first draw dereferenced a NULL zoom box. */
void test_the_first_draw_finds_a_picture_and_zoom_boxes_ready(void) {
  struct state *st = rotzoomer_init(dpy, 1);
  TEST_ASSERT_NOT_NULL(st->orig_map);
  TEST_ASSERT_NOT_NULL(st->zoom_box);
  TEST_ASSERT_TRUE(st->num_zoom > 0);
  for (int i = 0; i < st->num_zoom; i++) TEST_ASSERT_NOT_NULL(st->zoom_box[i]);
  rotzoomer_draw(dpy, 1, st);
  rotzoomer_free(dpy, 1, st);
}

/* The picture the hack zooms is what the load painted into its pixmap, in
 * canvas order. A flat picture looks the same however a box turns it, so
 * every pixel a box writes must be that colour. */
void test_the_zoomed_picture_is_the_one_the_load_painted(void) {
  static unsigned char flat[8 + 2 + 1];
  memcpy(flat, "565M", 4);
  flat[4] = 1;
  flat[6] = 1;
  flat[8] = 0x34;
  flat[9] = 0x12;
  flat[10] = 0x80;
  xshim_set_grab_image(flat, sizeof(flat));
  struct state *st = rotzoomer_init(dpy, 1);
  for (int i = 0; i < 10; i++) rotzoomer_draw(dpy, 1, st);
  int painted = 0, other = 0;
  for (int i = 0; i < cv.w * cv.h; i++) {
    if (cv.px[i] == px_swap(0x1234)) painted++;
    else if (cv.px[i] != 0) other++;
  }
  TEST_ASSERT_TRUE(painted > 0);
  TEST_ASSERT_EQUAL_INT(0, other);
  rotzoomer_free(dpy, 1, st);
  xshim_set_grab_image(NULL, 0);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_the_first_draw_finds_a_picture_and_zoom_boxes_ready);
  RUN_TEST(test_the_zoomed_picture_is_the_one_the_load_painted);
  return UNITY_END();
}
