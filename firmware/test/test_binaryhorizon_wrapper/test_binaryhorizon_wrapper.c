#include <sanitizer/allocator_interface.h>
#include <stdlib.h>
#include <unity.h>

#include "core/canvas.h"
#include "x11shim/xshim.h"

/* Includes the device wrapper itself, so the hack's own pixel packing runs
 * through whatever adapters the wrapper sets up. The renamed entry keeps it
 * from clashing with the copy in the registry. */
#define binaryhorizon_hack binaryhorizon_hack_under_test
#include "hacks/binaryhorizon_swapped.c"

static Canvas cv;
static Display *dpy;

void setUp(void) {
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&cv, 64, 64, malloc));
  dpy = xshim_open_display(&cv);
}
void tearDown(void) {
  xshim_close_display(dpy);
  canvas_free(&cv);
}

/* The hack is told the display is 24 bits deep, so it packs 0xRRGGBB, and the
 * window must hold canvas-order (byte-swapped) RGB565. Pure red is 0xF800. */
void test_a_blended_point_reaches_the_window_in_canvas_order(void) {
  struct state *st = binaryhorizon_init(dpy, 1);
  draw_point(st, 2, 3, rgb2point(st->depth, 255, 0, 0), 1.0f);
  TEST_ASSERT_EQUAL_INT(24, st->depth);
  TEST_ASSERT_EQUAL_HEX16(px_swap(0xF800), cv.px[3 * cv.w + 2]);
  TEST_ASSERT_EQUAL_HEX16(0, cv.px[3 * cv.w + 3]);
  binaryhorizon_free(dpy, 1, st);
}

/* White over black at 15% is a dark grey in all three channels. */
void test_white_blended_over_black_keeps_all_three_channels(void) {
  struct state *st = binaryhorizon_init(dpy, 1);
  draw_point(st, 2, 3, rgb2point(st->depth, 255, 255, 255), 0.15f);
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x2124), cv.px[3 * cv.w + 2]);
  binaryhorizon_free(dpy, 1, st);
}

/* A draw does not copy a frame-sized image or pixmap onto the window, and
 * makes none: nothing reads them. */
void test_a_frame_leaves_the_window_alone_and_holds_no_frame_sized_image(void) {
  struct state *st = binaryhorizon_init(dpy, 1);
  TEST_ASSERT_EQUAL_INT(1, st->buf->width);
  TEST_ASSERT_EQUAL_INT(1, st->buf->height);
  canvas_clear(&cv, 0x1234);
  binaryhorizon_draw(dpy, 1, st);
  TEST_ASSERT_EQUAL_HEX16(0x1234, cv.px[0]);
  TEST_ASSERT_EQUAL_HEX16(0x1234, cv.px[cv.w * cv.h - 1]);
  binaryhorizon_free(dpy, 1, st);
}

/* Every `duration` seconds the hack starts again and allocates a new pixel
 * buffer without freeing the old one: 868 KB each time on the device. */
void test_a_timed_restart_does_not_leak_the_pixel_buffer(void) {
  struct state *st = binaryhorizon_init(dpy, 1);
  st->duration = 30;
  binaryhorizon_draw(dpy, 1, st);
  const size_t before = __sanitizer_get_current_allocated_bytes();
  for (int i = 0; i < 20; i++) {
    st->start_time = 0;
    binaryhorizon_draw(dpy, 1, st);
  }
  const size_t after = __sanitizer_get_current_allocated_bytes();
  TEST_ASSERT_TRUE(after <= before + 512);
  binaryhorizon_free(dpy, 1, st);
}

static time_t fake_reads[2];
static int fake_calls;
static time_t fake_clock(void) { return fake_reads[fake_calls++ ? 1 : 0]; }

/* The wrapper's restart check and upstream's are two reads of the clock. When
 * the second falls in the next second, upstream replaced the buffer with the
 * wrapper's cleanup skipped: 868 KB leaked on the device. Both must see the
 * same second. */
void test_a_restart_on_the_edge_of_a_second_still_frees_the_old_buffer(void) {
  struct state *st = binaryhorizon_init(dpy, 1);
  st->duration = 30;
  st->start_time = 1000;
  binaryhorizon_draw(dpy, 1, st);
  const size_t before = __sanitizer_get_current_allocated_bytes();
  for (int i = 0; i < 20; i++) {
    st->start_time = 1000;
    fake_reads[0] = 1030;
    fake_reads[1] = 1031;
    fake_calls = 0;
    binaryhorizon_clock = fake_clock;
    binaryhorizon_draw(dpy, 1, st);
  }
  binaryhorizon_clock = binaryhorizon_real_clock;
  TEST_ASSERT_TRUE(__sanitizer_get_current_allocated_bytes() <= before + 512);
  binaryhorizon_free(dpy, 1, st);
}

/* The restart starts from black: the picture is not left under the new one. */
void test_a_timed_restart_clears_the_window(void) {
  struct state *st = binaryhorizon_init(dpy, 1);
  st->duration = 30;
  canvas_clear(&cv, 0x1234);
  binaryhorizon_draw(dpy, 1, st);
  TEST_ASSERT_EQUAL_HEX16(0x1234, cv.px[0]);
  st->start_time = 0;
  binaryhorizon_draw(dpy, 1, st);
  TEST_ASSERT_EQUAL_HEX16(0, cv.px[0]);
  TEST_ASSERT_EQUAL_HEX16(0, cv.px[cv.w * cv.h - 1]);
  binaryhorizon_free(dpy, 1, st);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_a_blended_point_reaches_the_window_in_canvas_order);
  RUN_TEST(test_white_blended_over_black_keeps_all_three_channels);
  RUN_TEST(test_a_frame_leaves_the_window_alone_and_holds_no_frame_sized_image);
  RUN_TEST(test_a_timed_restart_does_not_leak_the_pixel_buffer);
  RUN_TEST(test_a_timed_restart_clears_the_window);
  RUN_TEST(test_a_restart_on_the_edge_of_a_second_still_frees_the_old_buffer);
  return UNITY_END();
}
