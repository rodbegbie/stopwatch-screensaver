#include <sanitizer/allocator_interface.h>
#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include "core/canvas.h"
#include "x11shim/xshim.h"

#define LOWREZ_SCALE 3
#include "hacks/lowrez_put.h"

static Canvas cv;
static Display *dpy;
static size_t live_before;

void setUp(void) {
  live_before = __sanitizer_get_current_allocated_bytes();
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&cv, 20, 14, malloc));
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

static XImage *image_16(int w, int h) {
  XImage *im = XCreateImage(dpy, NULL, 16, ZPixmap, 0, NULL, w, h, 8, 0);
  TEST_ASSERT_NOT_NULL(im);
  im->data = (char *)calloc((size_t)h, (size_t)im->bytes_per_line);
  return im;
}

void test_the_window_is_reported_at_a_third_of_its_size(void) {
  XWindowAttributes a;
  lowrez_get_attributes(dpy, XSHIM_WINDOW, &a);
  TEST_ASSERT_EQUAL_INT(20 / 3, a.width);
  TEST_ASSERT_EQUAL_INT(14 / 3, a.height);
}

void test_each_pixel_becomes_a_square_centred_on_the_window(void) {
  XImage *im = image_16(2, 1);
  XPutPixel(im, 0, 0, 0xF800);
  XPutPixel(im, 1, 0, 0x07E0);
  XGCValues v = {0};
  GC gc = XCreateGC(dpy, XSHIM_WINDOW, 0, &v);
  lowrez_put_image(dpy, XSHIM_WINDOW, gc, im, 0, 0, 0, 0, 2, 1);
  /* 2x1 scaled by 3 is 6x3, centred: x from (20-6)/2 = 7, y from (14-3)/2 = 5. */
  for (int y = 0; y < 14; y++)
    for (int x = 0; x < 20; x++) {
      uint16_t want = 0;
      if (y >= 5 && y < 8 && x >= 7 && x < 10) want = px_swap(0xF800);
      if (y >= 5 && y < 8 && x >= 10 && x < 13) want = px_swap(0x07E0);
      TEST_ASSERT_EQUAL_HEX16_MESSAGE(want, cv.px[y * 20 + x], "pixel");
    }
  int x0, x1;
  TEST_ASSERT_TRUE(canvas_dirty_row(&cv, 6, &x0, &x1));
  TEST_ASSERT_EQUAL_INT(7, x0);
  TEST_ASSERT_EQUAL_INT(12, x1);
  TEST_ASSERT_FALSE(canvas_dirty_row(&cv, 4, &x0, &x1));
  XFreeGC(dpy, gc);
  XDestroyImage(im);
}

void test_an_image_wider_than_the_window_is_clipped_not_wrapped(void) {
  XImage *im = image_16(9, 6);
  for (int y = 0; y < 6; y++)
    for (int x = 0; x < 9; x++) XPutPixel(im, x, y, 0x1111);
  XGCValues v = {0};
  GC gc = XCreateGC(dpy, XSHIM_WINDOW, 0, &v);
  lowrez_put_image(dpy, XSHIM_WINDOW, gc, im, 0, 0, 0, 0, 9, 6);
  for (int y = 0; y < 14; y++)
    for (int x = 0; x < 20; x++)
      TEST_ASSERT_EQUAL_HEX16(px_swap(0x1111), cv.px[y * 20 + x]);
  XFreeGC(dpy, gc);
  XDestroyImage(im);
}

void test_a_source_offset_past_the_image_draws_nothing_and_reads_nothing_past_it(void) {
  XImage *im = image_16(2, 1);
  XPutPixel(im, 0, 0, 0xFFFF);
  XPutPixel(im, 1, 0, 0xFFFF);
  XGCValues v = {0};
  GC gc = XCreateGC(dpy, XSHIM_WINDOW, 0, &v);
  lowrez_put_image(dpy, XSHIM_WINDOW, gc, im, 5, 0, 0, 0, 3, 1);
  lowrez_put_image(dpy, XSHIM_WINDOW, gc, im, 2, 0, 0, 0, 1, 1);
  lowrez_put_image(dpy, XSHIM_WINDOW, gc, im, 0, 1, 0, 0, 1, 1);
  lowrez_put_image(dpy, XSHIM_WINDOW, gc, im, 0, 7, 0, 0, 1, 1);
  for (int i = 0; i < 20 * 14; i++) TEST_ASSERT_EQUAL_HEX16(0, cv.px[i]);
  XFreeGC(dpy, gc);
  XDestroyImage(im);
}

void test_a_null_image_draws_nothing(void) {
  XGCValues v = {0};
  GC gc = XCreateGC(dpy, XSHIM_WINDOW, 0, &v);
  lowrez_put_image(dpy, XSHIM_WINDOW, gc, NULL, 0, 0, 0, 0, 2, 1);
  TEST_ASSERT_EQUAL_HEX16(0, cv.px[0]);
  XFreeGC(dpy, gc);
}

/* Every pixel against a plain reference, for images placed off each edge. */
static void reference_put(uint16_t *ref, XImage *im, int sx, int sy, int dx, int dy, int w,
                          int h) {
  if (w > im->width - sx) w = im->width - sx;
  if (h > im->height - sy) h = im->height - sy;
  const int ox = (20 - im->width * 3) / 2, oy = (14 - im->height * 3) / 2;
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++)
      for (int ky = 0; ky < 3; ky++)
        for (int kx = 0; kx < 3; kx++) {
          const int x = ox + (dx + i) * 3 + kx, y = oy + (dy + j) * 3 + ky;
          if (x < 0 || y < 0 || x >= 20 || y >= 14) continue;
          ref[y * 20 + x] = px_swap((uint16_t)XGetPixel(im, sx + i, sy + j));
        }
}

void test_random_placements_match_a_plain_reference(void) {
  XGCValues v = {0};
  GC gc = XCreateGC(dpy, XSHIM_WINDOW, 0, &v);
  srandom(7);
  for (int round = 0; round < 300; round++) {
    const int iw = 1 + (int)(random() % 9), ih = 1 + (int)(random() % 6);
    XImage *im = image_16(iw, ih);
    for (int y = 0; y < ih; y++)
      for (int x = 0; x < iw; x++) XPutPixel(im, x, y, (unsigned long)(random() & 0xFFFF) | 1);
    const int sx = (int)(random() % iw), sy = (int)(random() % ih);
    const int dx = (int)(random() % 13) - 6, dy = (int)(random() % 11) - 5;
    const int w = 1 + (int)(random() % 10), h = 1 + (int)(random() % 7);
    uint16_t ref[20 * 14];
    memset(cv.px, 0, sizeof ref);
    memset(ref, 0, sizeof ref);
    reference_put(ref, im, sx, sy, dx, dy, w, h);
    lowrez_put_image(dpy, XSHIM_WINDOW, gc, im, sx, sy, dx, dy, (unsigned)w, (unsigned)h);
    TEST_ASSERT_EQUAL_HEX16_ARRAY(ref, cv.px, 20 * 14);
    XDestroyImage(im);
  }
  XFreeGC(dpy, gc);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_the_window_is_reported_at_a_third_of_its_size);
  RUN_TEST(test_each_pixel_becomes_a_square_centred_on_the_window);
  RUN_TEST(test_an_image_wider_than_the_window_is_clipped_not_wrapped);
  RUN_TEST(test_a_source_offset_past_the_image_draws_nothing_and_reads_nothing_past_it);
  RUN_TEST(test_a_null_image_draws_nothing);
  RUN_TEST(test_random_placements_match_a_plain_reference);
  return UNITY_END();
}
