#include <sanitizer/allocator_interface.h>
#include <stdlib.h>
#include <unity.h>

#include "core/canvas.h"
#include "hacks/halo_planes.h"
#include "x11shim/pixmap.h"
#include "x11shim/xshim.h"

static Canvas cv;
static Display *dpy;
static size_t live_before;

void setUp(void) {
  live_before = __sanitizer_get_current_allocated_bytes();
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&cv, 8, 8, malloc));
  dpy = xshim_open_display(&cv);
  canvas_clear_dirty(&cv);
}
void tearDown(void) {
  xshim_close_display(dpy);
  canvas_free(&cv);
  TEST_ASSERT_EQUAL_UINT64_MESSAGE(live_before,
                                   __sanitizer_get_current_allocated_bytes(),
                                   "test leaked memory");
}

static uint16_t *pixels(Pixmap p) {
  return ((struct XshimPixmap *)(uintptr_t)p)->canvas.px;
}

static Pixmap plane_with_set_pixels(int n_set) {
  Pixmap p = XCreatePixmap(dpy, XSHIM_WINDOW, 4, 4, 16);
  TEST_ASSERT_TRUE(p != None);
  for (int i = 0; i < n_set; i++) pixels(p)[i] = 1;
  return p;
}

void test_copy_to_the_window_paints_set_bits_in_the_foreground_and_clear_in_the_background(void) {
  Pixmap p = plane_with_set_pixels(1);
  XGCValues v = {0};
  GC gc = XCreateGC(dpy, XSHIM_WINDOW, 0, &v);
  XSetForeground(dpy, gc, 0x1234);
  XSetBackground(dpy, gc, 0x00F0);
  halo_copy_plane(dpy, p, XSHIM_WINDOW, gc, 0, 0, 4, 4, 2, 3, 1);
  TEST_ASSERT_EQUAL_HEX(0x1234, cv.px[3 * 8 + 2]);
  TEST_ASSERT_EQUAL_HEX(0x00F0, cv.px[3 * 8 + 3]);
  TEST_ASSERT_EQUAL_HEX(0x00F0, cv.px[6 * 8 + 5]);
  TEST_ASSERT_EQUAL_HEX(0, cv.px[0]);
  int x0, x1;
  TEST_ASSERT_TRUE(canvas_dirty_row(&cv, 3, &x0, &x1));
  TEST_ASSERT_EQUAL_INT(2, x0);
  TEST_ASSERT_EQUAL_INT(5, x1);
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, p);
}

void test_copy_to_a_pixmap_flips_the_bits_it_sets(void) {
  Pixmap src = plane_with_set_pixels(2);
  Pixmap dst = XCreatePixmap(dpy, XSHIM_WINDOW, 4, 4, 16);
  pixels(dst)[0] = 1;
  pixels(dst)[2] = 1;
  XGCValues v = {0};
  GC gc = XCreateGC(dpy, XSHIM_WINDOW, 0, &v);
  halo_copy_plane(dpy, src, dst, gc, 0, 0, 4, 4, 0, 0, 1);
  TEST_ASSERT_EQUAL_UINT16(0, pixels(dst)[0]);
  TEST_ASSERT_EQUAL_UINT16(1, pixels(dst)[1]);
  TEST_ASSERT_EQUAL_UINT16(1, pixels(dst)[2]);
  TEST_ASSERT_EQUAL_UINT16(0, pixels(dst)[3]);
  halo_copy_plane(dpy, src, dst, gc, 0, 0, 4, 4, 0, 0, 1);
  TEST_ASSERT_EQUAL_UINT16(1, pixels(dst)[0]);
  TEST_ASSERT_EQUAL_UINT16(0, pixels(dst)[1]);
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, src);
  XFreePixmap(dpy, dst);
}

void test_a_copy_hanging_off_every_edge_stays_inside_both_buffers(void) {
  Pixmap p = plane_with_set_pixels(16);
  XGCValues v = {0};
  GC gc = XCreateGC(dpy, XSHIM_WINDOW, 0, &v);
  XSetForeground(dpy, gc, 0xFFFF);
  halo_copy_plane(dpy, p, XSHIM_WINDOW, gc, -2, -2, 100, 100, -1, -1, 1);
  halo_copy_plane(dpy, p, XSHIM_WINDOW, gc, 3, 3, 100, 100, 6, 6, 1);
  /* Source (-2,-2) and destination (-1,-1) first meet at destination (1,1). */
  TEST_ASSERT_EQUAL_HEX(0, cv.px[0]);
  TEST_ASSERT_EQUAL_HEX(0xFFFF, cv.px[1 * 8 + 1]);
  /* Source (3,3) is the last pixel of the 4 by 4 source: one pixel lands. */
  TEST_ASSERT_EQUAL_HEX(0xFFFF, cv.px[6 * 8 + 6]);
  TEST_ASSERT_EQUAL_HEX(0, cv.px[7 * 8 + 7]);
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, p);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_copy_to_the_window_paints_set_bits_in_the_foreground_and_clear_in_the_background);
  RUN_TEST(test_copy_to_a_pixmap_flips_the_bits_it_sets);
  RUN_TEST(test_a_copy_hanging_off_every_edge_stays_inside_both_buffers);
  return UNITY_END();
}
