/* lowrez_put.h at a scale of 2, where a pair of pixels goes in one 32-bit
 * store when it is aligned. Both an even and an odd centring offset are
 * reached (image widths 1 to 9 on a 20-pixel canvas). */
#include <sanitizer/allocator_interface.h>
#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include "core/canvas.h"
#include "x11shim/xshim.h"

#define LOWREZ_SCALE 2
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

/* Every pixel against a plain reference, for images placed off each edge. */
static void reference_put(uint16_t *ref, XImage *im, int sx, int sy, int dx, int dy, int w,
                          int h) {
  if (w > im->width - sx) w = im->width - sx;
  if (h > im->height - sy) h = im->height - sy;
  const int ox = (20 - im->width * 2) / 2, oy = (14 - im->height * 2) / 2;
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++)
      for (int ky = 0; ky < 2; ky++)
        for (int kx = 0; kx < 2; kx++) {
          const int x = ox + (dx + i) * 2 + kx, y = oy + (dy + j) * 2 + ky;
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
  RUN_TEST(test_random_placements_match_a_plain_reference);
  return UNITY_END();
}
