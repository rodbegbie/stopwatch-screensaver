#include <stdlib.h>
#include <unity.h>

#include "core/canvas.h"
#include "x11shim/xshim.h"

/* Includes the device wrapper itself, so the hack's own alpha blend runs with
 * whatever pixel adapters the wrapper sets up. The renamed entry keeps it from
 * clashing with the copy in the registry. */
#define interaggregate_hack interaggregate_hack_under_test
#include "hacks/interaggregate_direct.c"

static Canvas cv;
static Display *dpy;

void setUp(void) {
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&cv, 4, 4, malloc));
  dpy = xshim_open_display(&cv);
}
void tearDown(void) {
  xshim_close_display(dpy);
  canvas_free(&cv);
}

/* The hack unpacks and packs pixels itself, as ordinary RGB565, but the canvas
 * holds them byte-swapped. Half-intensity red over black must reach the canvas
 * as 0x7800 in ordinary RGB565, which is 0x0078 swapped. */
void test_half_intensity_red_over_black_blends_to_7800(void) {
  XColor red = {0};
  red.red = 0xFFFF;
  red.flags = DoRed | DoGreen | DoBlue;
  TEST_ASSERT_TRUE(XAllocColor(dpy, 1, &red) != 0);

  unsigned long background = 0;
  struct field f = {0};
  f.width = 1;
  f.height = 1;
  f.off_img = &background;
  f.visdepth = 16;
  const unsigned long blended = trans_point(0, 0, red.pixel, 0.5f, &f);

  XGCValues v = {0};
  GC gc = XCreateGC(dpy, 1, 0, &v);
  XSetForeground(dpy, gc, blended);
  XDrawPoint(dpy, 1, gc, 0, 0);
  XFreeGC(dpy, gc);

  TEST_ASSERT_EQUAL_HEX(px_swap(0x7800), cv.px[0]);
}

/* Half white over black is a mid grey, 0x7BEF in ordinary RGB565. */
void test_half_intensity_white_over_black_blends_to_grey(void) {
  XColor white = {0};
  white.red = white.green = white.blue = 0xFFFF;
  white.flags = DoRed | DoGreen | DoBlue;
  TEST_ASSERT_TRUE(XAllocColor(dpy, 1, &white) != 0);

  unsigned long background = 0;
  struct field f = {0};
  f.width = 1;
  f.height = 1;
  f.off_img = &background;
  f.visdepth = 16;
  const unsigned long blended = trans_point(0, 0, white.pixel, 0.5f, &f);

  XGCValues v = {0};
  GC gc = XCreateGC(dpy, 1, 0, &v);
  XSetForeground(dpy, gc, blended);
  XDrawPoint(dpy, 1, gc, 0, 0);
  XFreeGC(dpy, gc);

  TEST_ASSERT_EQUAL_HEX(px_swap(0x7BEF), cv.px[0]);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_half_intensity_red_over_black_blends_to_7800);
  RUN_TEST(test_half_intensity_white_over_black_blends_to_grey);
  return UNITY_END();
}
