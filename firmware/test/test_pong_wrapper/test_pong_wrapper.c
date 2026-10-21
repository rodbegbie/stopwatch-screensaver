#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include "core/canvas.h"
#include "hacks/pong/analogtv.h"
#include "runner/hack_entry.h"
#include "x11shim/xshim.h"

/* Pong is built through hacks/pong_tv.c, which owns the real analogtv_allocate.
 * The test links the registered copy, so it sees what the board runs. */
extern const HackEntry pong_hack;

static const char *const tv_defaults[] = {ANALOGTV_DEFAULTS 0};

static Canvas cv;
static Display *dpy;

void setUp(void) {
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&cv, 466, 466, malloc));
  dpy = xshim_open_display(&cv);
  xshim_set_defaults(tv_defaults);
}
void tearDown(void) {
  xshim_set_defaults(NULL);
  xshim_close_display(dpy);
  canvas_free(&cv);
}

/* analogtv packs ordinary RGB565 from the visual's masks and the canvas holds
 * each pixel's bytes swapped, so every table entry must be in canvas order. A
 * brightness past the top of the gamma table is full on in every channel. */
void test_the_colour_tables_are_in_canvas_order(void) {
  analogtv *it = analogtv_allocate(dpy, XSHIM_WINDOW);
  TEST_ASSERT_NOT_NULL(it);
  TEST_ASSERT_FALSE(it->use_cmap);
  TEST_ASSERT_EQUAL_HEX16(px_swap(0xF800), it->red_values[ANALOGTV_CV_MAX - 1]);
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x07E0), it->green_values[ANALOGTV_CV_MAX - 1]);
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x001F), it->blue_values[ANALOGTV_CV_MAX - 1]);
  TEST_ASSERT_EQUAL_HEX16(0, it->red_values[0] | it->green_values[0] | it->blue_values[0]);
  analogtv_release(it);
}

/* A frame is white paddles on a dark grey field. White is its own byte swap, but
 * a swapped grey is tinted, so a frame in the wrong order fails the grey test. */
void test_a_frame_is_neutral_grey_and_white_not_tinted(void) {
  xshim_set_defaults(pong_hack.defaults);
  void *st = pong_hack.init(dpy, XSHIM_WINDOW);
  for (int i = 0; i < 20; i++) pong_hack.draw(dpy, XSHIM_WINDOW, st);
  int tinted = 0, white = 0;
  for (int i = 0; i < cv.w * cv.h; i++) {
    const uint16_t p = px_swap(cv.px[i]);
    const int r = (p >> 11) << 3, g = ((p >> 5) & 0x3F) << 2, b = (p & 0x1F) << 3;
    if (abs(r - g) > 40 || abs(g - b) > 40) tinted++;
    if (p == 0xFFFF) white++;
  }
  TEST_ASSERT_TRUE_MESSAGE(white > 500, "the paddles and ball should be white");
  TEST_ASSERT_TRUE_MESSAGE(tinted < cv.w * cv.h / 50, "the picture should be grey, not tinted");
  pong_hack.free(dpy, XSHIM_WINDOW, st);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_the_colour_tables_are_in_canvas_order);
  RUN_TEST(test_a_frame_is_neutral_grey_and_white_not_tinted);
  return UNITY_END();
}
