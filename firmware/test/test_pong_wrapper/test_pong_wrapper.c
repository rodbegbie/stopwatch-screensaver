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
/* The wrapper's two ways of doing less work per frame (see pong_tv.c). */
extern int pong_line_step;
extern int pong_view_size;

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

/* One analogtv frame of a solid white picture, into an image first filled with a
 * pattern no pixel can take. Returns how many image pixels still hold it. */
static int g_dark_rows;

static int unwritten_pixels(int line_step, int view) {
  const int was_step = pong_line_step, was_view = pong_view_size;
  pong_line_step = line_step;
  pong_view_size = view;
  analogtv *it = analogtv_allocate(dpy, XSHIM_WINDOW);
  TEST_ASSERT_NOT_NULL(it);
  analogtv_set_defaults(it, "");
  analogtv_input *inp = analogtv_input_allocate();
  analogtv_setup_sync(inp, 0, 0);
  int white[4];
  analogtv_lcp_to_ntsc(100.0, 0.0, 0.0, white);
  analogtv_draw_solid(inp, ANALOGTV_VIS_START, ANALOGTV_VIS_END, ANALOGTV_TOP,
                      ANALOGTV_BOT, white);
  analogtv_reception rec;
  memset(&rec, 0, sizeof(rec));
  rec.input = inp;
  rec.level = 2.0;
  const analogtv_reception *recs = &rec;
  memset(it->image->data, 0x5A, (size_t)it->image->height * it->image->bytes_per_line);
  analogtv_draw(it, 0.0, &recs, 1);
  int left = 0;
  g_dark_rows = 0;
  for (int y = 0; y < it->image->height; y++) {
    const uint16_t *row = (const uint16_t *)(it->image->data + (size_t)y * it->image->bytes_per_line);
    for (int x = 0; x < it->image->width; x++) left += row[x] == 0x5A5A;
    /* The middle of a solid white picture is lit on every row. */
    if (row[it->image->width / 2] == 0) g_dark_rows++;
  }
  analogtv_release(it);
  free(inp);
  pong_line_step = was_step;
  pong_view_size = was_view;
  return left;
}

/* A step of N draws every Nth line and its picture must cover the rows of the
 * lines it skips: a row nobody drew would keep last frame's picture. Upstream
 * itself leaves the top and bottom row alone (the picture is a hair under full
 * height and the line edges truncate), so step 1 is the baseline. */
void test_every_row_of_the_image_is_drawn_at_each_line_step(void) {
  const int baseline = unwritten_pixels(1, 0);
  TEST_ASSERT_TRUE_MESSAGE(baseline <= 2 * 466, "upstream leaves more than the edge rows");
  for (int step = 2; step <= 4; step++)
    TEST_ASSERT_EQUAL_INT_MESSAGE(baseline, unwritten_pixels(step, 0),
                                  "a row was left undrawn");
}

void test_every_row_is_drawn_in_a_smaller_view_too(void) {
  const int baseline = unwritten_pixels(1, 330);
  TEST_ASSERT_TRUE_MESSAGE(baseline <= 2 * 330, "upstream leaves more than the edge rows");
  for (int step = 2; step <= 3; step++)
    TEST_ASSERT_EQUAL_INT_MESSAGE(baseline, unwritten_pixels(step, 330),
                                  "a row was left undrawn");
}

/* analogtv shades each row of a line by its place in the line (a scanline gap),
 * from a table it builds only for the row heights near the average. A stepped
 * line that was drawn as one tall line would pick up an empty entry and come out
 * black: the rows must be shaded as upstream shades them, so a solid white
 * picture has no more dark rows at a step of N than at a step of 1. */
void test_a_line_step_leaves_no_dark_rows_a_solid_picture_does_not_have(void) {
  unwritten_pixels(1, 0);
  const int baseline = g_dark_rows;
  for (int step = 2; step <= 4; step++) {
    unwritten_pixels(step, 0);
    TEST_ASSERT_EQUAL_INT_MESSAGE(baseline, g_dark_rows, "a stepped line left rows black");
  }
}

/* The test above cannot tell a step of 2 from a step of 1 if every row is drawn
 * either way, so count the lines that were processed: the work is what we cut. */
void test_a_line_step_draws_that_fraction_of_the_lines(void) {
  extern long pong_lines_drawn;
  long drawn[5] = {0};
  for (int step = 1; step <= 4; step++) {
    pong_lines_drawn = 0;
    unwritten_pixels(step, 0);
    drawn[step] = pong_lines_drawn;
  }
  TEST_ASSERT_EQUAL_INT(ANALOGTV_VISLINES, drawn[1]);
  TEST_ASSERT_EQUAL_INT(ANALOGTV_VISLINES / 2, drawn[2]);
  TEST_ASSERT_EQUAL_INT((ANALOGTV_VISLINES + 2) / 3, drawn[3]);
  TEST_ASSERT_EQUAL_INT(ANALOGTV_VISLINES / 4, drawn[4]);
}

/* A smaller view sits in the middle of the canvas, leaves the rest black, and
 * its corners stay inside the round glass. */
void test_a_330_pixel_view_is_centred_and_inside_the_glass(void) {
  const int was_view = pong_view_size;
  pong_view_size = 330;
  xshim_set_defaults(pong_hack.defaults);
  void *st = pong_hack.init(dpy, XSHIM_WINDOW);
  for (int i = 0; i < 20; i++) pong_hack.draw(dpy, XSHIM_WINDOW, st);
  int inside = 0, outside = 0, off_glass = 0;
  for (int y = 0; y < cv.h; y++)
    for (int x = 0; x < cv.w; x++) {
      const uint16_t p = cv.px[y * cv.w + x];
      const int in = x >= 68 && x < 398 && y >= 68 && y < 398;
      if (!p) continue;
      if (in) inside++;
      else outside++;
      const int dx = 2 * x + 1 - cv.w, dy = 2 * y + 1 - cv.h;
      if (dx * dx + dy * dy > (cv.w + 4) * (cv.w + 4)) off_glass++;
    }
  TEST_ASSERT_TRUE_MESSAGE(inside > 20000, "the picture should fill the view");
  TEST_ASSERT_EQUAL_INT_MESSAGE(0, outside, "nothing may be drawn outside the view");
  TEST_ASSERT_EQUAL_INT_MESSAGE(0, off_glass, "the view's corners must be inside the glass");
  pong_hack.free(dpy, XSHIM_WINDOW, st);
  pong_view_size = was_view;
}

/* The whole-window view fills the canvas, which is what lets the push overlap
 * the next step: the registry flag is only true if every pixel is repainted. */
void test_the_whole_window_view_repaints_every_pixel(void) {
  const int was_view = pong_view_size;
  pong_view_size = 0;
  xshim_set_defaults(pong_hack.defaults);
  void *st = pong_hack.init(dpy, XSHIM_WINDOW);
  pong_hack.draw(dpy, XSHIM_WINDOW, st);
  memset(cv.px, 0xA5, (size_t)cv.w * cv.h * sizeof(uint16_t));
  pong_hack.draw(dpy, XSHIM_WINDOW, st);
  int stale = 0;
  for (int i = 0; i < cv.w * cv.h; i++) stale += cv.px[i] == 0xA5A5;
  TEST_ASSERT_EQUAL_INT_MESSAGE(0, stale, "a pixel kept the old canvas");
  pong_hack.free(dpy, XSHIM_WINDOW, st);
  pong_view_size = was_view;
}

/* The entry's flag must agree with the build's view. */
void test_the_hack_overlaps_its_push_only_with_the_whole_window_view(void) {
  TEST_ASSERT_EQUAL_INT(pong_view_size == 0, pong_hack.repaints_all);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_the_colour_tables_are_in_canvas_order);
  RUN_TEST(test_a_frame_is_neutral_grey_and_white_not_tinted);
  RUN_TEST(test_every_row_of_the_image_is_drawn_at_each_line_step);
  RUN_TEST(test_every_row_is_drawn_in_a_smaller_view_too);
  RUN_TEST(test_a_line_step_leaves_no_dark_rows_a_solid_picture_does_not_have);
  RUN_TEST(test_a_line_step_draws_that_fraction_of_the_lines);
  RUN_TEST(test_a_330_pixel_view_is_centred_and_inside_the_glass);
  RUN_TEST(test_the_whole_window_view_repaints_every_pixel);
  RUN_TEST(test_the_hack_overlaps_its_push_only_with_the_whole_window_view);
  return UNITY_END();
}
