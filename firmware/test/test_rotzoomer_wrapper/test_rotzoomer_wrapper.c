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
  int painted = 0;
  for (int i = 0; i < cv.w * cv.h; i++)
    if (cv.px[i] == px_swap(0x1234)) painted++;
  TEST_ASSERT_EQUAL_INT(cv.w * cv.h, painted);
  rotzoomer_free(dpy, 1, st);
  xshim_set_grab_image(NULL, 0);
}

static void flat_picture(unsigned char *blob, unsigned lo, unsigned hi) {
  memcpy(blob, "565M", 4);
  blob[4] = 1;
  blob[6] = 1;
  blob[8] = (unsigned char)lo;
  blob[9] = (unsigned char)hi;
  blob[10] = 0x80;
}

/* The picture the hack zooms is a 32-bit image from XGetImage, but its buffer
 * is 16 bits: copying one into the other alternates the picture with black
 * and scrambles it. Every pixel of a flat picture must come out that colour. */
void test_a_flat_picture_comes_out_without_black_pixels(void) {
  static unsigned char flat[11];
  flat_picture(flat, 0x34, 0x12);
  xshim_set_grab_image(flat, sizeof(flat));
  struct state *st = rotzoomer_init(dpy, 1);
  TEST_ASSERT_EQUAL_INT(16, st->buffer_map->bits_per_pixel);
  TEST_ASSERT_EQUAL_INT(16, st->orig_map->bits_per_pixel);
  for (int i = 0; i < cv.w * cv.h; i++)
    TEST_ASSERT_EQUAL_HEX16(px_swap(0x1234), cv.px[i]);
  rotzoomer_free(dpy, 1, st);
  xshim_set_grab_image(NULL, 0);
}

/* Every `duration` seconds the hack loads a picture again. The shim finishes
 * that load at once, so upstream never sees it finish: the wrapper must
 * refresh the zoomed picture itself, or the old one stays on show. */
void test_a_timed_reload_shows_the_new_picture(void) {
  static unsigned char first[11], second[11];
  flat_picture(first, 0x34, 0x12);
  flat_picture(second, 0x78, 0x56);
  xshim_set_grab_image(first, sizeof(first));
  struct state *st = rotzoomer_init(dpy, 1);
  xshim_set_grab_image(second, sizeof(second));
  st->start_time = 0;
  rotzoomer_draw(dpy, 1, st);
  for (int i = 0; i < cv.w * cv.h; i++)
    TEST_ASSERT_EQUAL_HEX16(px_swap(0x5678), cv.px[i]);
  rotzoomer_free(dpy, 1, st);
  xshim_set_grab_image(NULL, 0);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_the_first_draw_finds_a_picture_and_zoom_boxes_ready);
  RUN_TEST(test_the_zoomed_picture_is_the_one_the_load_painted);
  RUN_TEST(test_a_flat_picture_comes_out_without_black_pixels);
  RUN_TEST(test_a_timed_reload_shows_the_new_picture);
  return UNITY_END();
}
