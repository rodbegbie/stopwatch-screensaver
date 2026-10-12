/* The Xft stand-in (x11shim/xft.c), drawn from a font built by hand here so the
 * expected pixels can be worked out without the real glyph data. */
#include <limits.h>
#include <sanitizer/allocator_interface.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include "core/canvas.h"
#include "screenhack.h"
#include "x11shim/xft.h"
#include "x11shim/xft_font.h"
#include "x11shim/xshim.h"

#define GLYPHS 95
#define IDX(c) ((c) - 32)

/* Size 8: ascent 6, descent 2. 'N' is a solid 3 by 4 block whose left edge is
 * 1 pixel right of the origin and whose top is 4 above the baseline; 'A' is
 * two pixels of half and full coverage; '?' is a single pixel at 200. */
static const unsigned char n_alpha[12] = {255, 255, 255, 255, 255, 255,
                                          255, 255, 255, 255, 255, 255};
static const unsigned char a_alpha[2] = {128, 255};
static const unsigned char q_alpha[1] = {200};
static unsigned char big_alpha[1] = {255};
/* '~' is a 16 by 16 ramp, coverage 0 to 255 in reading order, with its top 12
 * above the baseline so it reaches 4 below. */
#define RAMP_OFFSET 15

static XshimGlyph small_glyphs[GLYPHS];
static XshimGlyph big_glyphs[GLYPHS];
static XshimFontSize sizes[2];
static XshimFont font;

static void build_font(void) {
  memset(small_glyphs, 0, sizeof small_glyphs);
  memset(big_glyphs, 0, sizeof big_glyphs);
  for (int i = 0; i < GLYPHS; i++) small_glyphs[i].advance = 4;
  small_glyphs[IDX(' ')].advance = 3;
  small_glyphs[IDX('N')] = (XshimGlyph){3, 4, 1, 4, 5, 0};
  small_glyphs[IDX('A')] = (XshimGlyph){2, 1, 0, 1, 3, 0};
  small_glyphs[IDX('?')] = (XshimGlyph){1, 1, 0, 1, 2, 0};
  small_glyphs[IDX('~')] = (XshimGlyph){16, 16, 0, 12, 17, RAMP_OFFSET};
  big_glyphs[IDX('N')] = (XshimGlyph){1, 1, 0, 1, 9, 0};
  sizes[0] = (XshimFontSize){8, 6, 2, small_glyphs, NULL};
  sizes[1] = (XshimFontSize){16, 12, 4, big_glyphs, big_alpha};
  font = (XshimFont){2, sizes};
}

/* The three glyphs have different alpha runs; the shim reads each at its
 * glyph's offset, so lay them out in one array per size. */
static unsigned char small_alpha[RAMP_OFFSET + 256];

static void use_font(void) {
  build_font();
  memcpy(small_alpha, n_alpha, 12);
  memcpy(small_alpha + 12, a_alpha, 2);
  memcpy(small_alpha + 14, q_alpha, 1);
  for (int i = 0; i < 256; i++) small_alpha[RAMP_OFFSET + i] = (unsigned char)i;
  small_glyphs[IDX('A')].offset = 12;
  small_glyphs[IDX('?')].offset = 14;
  sizes[0].alpha = small_alpha;
  xshim_set_font(&font);
}

static Canvas cv;
static Display *dpy;
static Window win = XSHIM_WINDOW;
static size_t live_before;

void setUp(void) {
  live_before = __sanitizer_get_current_allocated_bytes();
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&cv, 16, 16, malloc));
  dpy = xshim_open_display(&cv);
  use_font();
}
void tearDown(void) {
  xshim_set_font(NULL);
  xshim_close_display(dpy);
  canvas_free(&cv);
  TEST_ASSERT_EQUAL_UINT64_MESSAGE(live_before,
                                   __sanitizer_get_current_allocated_bytes(),
                                   "test leaked memory");
}

static uint16_t at(int x, int y) { return cv.px[y * cv.w + x]; }
static int count_not(uint16_t colour) {
  int n = 0;
  for (int i = 0; i < cv.w * cv.h; i++) n += cv.px[i] != colour;
  return n;
}

static XftColor colour(const char *name) {
  XftColor c;
  TEST_ASSERT_TRUE(XftColorAllocName(dpy, NULL, 1, name, &c));
  return c;
}

static void draw(XftFont *f, const XftColor *c, int x, int y, const char *s,
                 int len) {
  XftDraw *d = XftDrawCreate(dpy, win, NULL, 1);
  XftDrawStringUtf8(d, c, f, x, y, (const FcChar8 *)s, len);
  XftDrawDestroy(d);
}

void test_the_nearest_size_is_chosen_from_the_first_family_in_the_list(void) {
  XftFont *f = load_xft_font_retry(dpy, 0, "Special Elite 9, Courier 16");
  TEST_ASSERT_NOT_NULL(f);
  TEST_ASSERT_EQUAL_INT(6, f->ascent);
  TEST_ASSERT_EQUAL_INT(2, f->descent);
  TEST_ASSERT_EQUAL_INT(8, f->height);
  XftFontClose(dpy, f);

  f = load_xft_font_retry(dpy, 0, "Special Elite 14");
  TEST_ASSERT_NOT_NULL(f);
  TEST_ASSERT_EQUAL_INT(12, f->ascent);
  XftFontClose(dpy, f);
}

void test_a_tie_between_two_sizes_takes_the_smaller(void) {
  XftFont *f = load_xft_font_retry(dpy, 0, "Special Elite 12");
  TEST_ASSERT_NOT_NULL(f);
  TEST_ASSERT_EQUAL_INT(6, f->ascent);
  XftFontClose(dpy, f);
}

void test_a_name_without_a_size_takes_the_first_size(void) {
  XftFont *f = load_xft_font_retry(dpy, 0, "monospace");
  TEST_ASSERT_NOT_NULL(f);
  TEST_ASSERT_EQUAL_INT(6, f->ascent);
  XftFontClose(dpy, f);
  f = load_xft_font_retry(dpy, 0, NULL);
  TEST_ASSERT_NOT_NULL(f);
  XftFontClose(dpy, f);
}

void test_no_font_installed_gives_no_font(void) {
  xshim_set_font(NULL);
  TEST_ASSERT_NULL(load_xft_font_retry(dpy, 0, "Special Elite 12"));
}

void test_extents_of_one_glyph(void) {
  XftFont *f = load_xft_font_retry(dpy, 0, "x 8");
  XGlyphInfo g;
  XftTextExtentsUtf8(dpy, f, (const FcChar8 *)"N", 1, &g);
  TEST_ASSERT_EQUAL_INT(5, g.xOff);
  TEST_ASSERT_EQUAL_INT(0, g.yOff);
  TEST_ASSERT_EQUAL_INT(3, g.width);
  TEST_ASSERT_EQUAL_INT(4, g.height);
  TEST_ASSERT_EQUAL_INT(-1, g.x);
  TEST_ASSERT_EQUAL_INT(4, g.y);
  XftFontClose(dpy, f);
}

void test_extents_of_two_glyphs_add_advances_and_span_both_inks(void) {
  XftFont *f = load_xft_font_retry(dpy, 0, "x 8");
  XGlyphInfo g;
  XftTextExtentsUtf8(dpy, f, (const FcChar8 *)"N N", 3, &g);
  TEST_ASSERT_EQUAL_INT(5 + 3 + 5, g.xOff);
  /* Ink runs from x 1 (first N) to x 11 (second N, origin 8): 11 wide. */
  TEST_ASSERT_EQUAL_INT(11, g.width);
  TEST_ASSERT_EQUAL_INT(-1, g.x);
  TEST_ASSERT_EQUAL_INT(4, g.height);
  XftFontClose(dpy, f);
}

void test_extents_of_blank_text_have_an_advance_and_no_ink(void) {
  XftFont *f = load_xft_font_retry(dpy, 0, "x 8");
  XGlyphInfo g;
  XftTextExtentsUtf8(dpy, f, (const FcChar8 *)"  ", 2, &g);
  TEST_ASSERT_EQUAL_INT(6, g.xOff);
  TEST_ASSERT_EQUAL_INT(0, g.width);
  TEST_ASSERT_EQUAL_INT(0, g.height);
  XftTextExtentsUtf8(dpy, f, (const FcChar8 *)"", 0, &g);
  TEST_ASSERT_EQUAL_INT(0, g.xOff);
  XftFontClose(dpy, f);
}

void test_a_glyph_covers_exactly_its_ink_box(void) {
  XftFont *f = load_xft_font_retry(dpy, 0, "x 8");
  XftColor white = colour("white");
  draw(f, &white, 2, 6, "N", 1);
  const uint16_t w = rgb565(255, 255, 255);
  TEST_ASSERT_EQUAL_INT(12, count_not(0));
  for (int y = 2; y <= 5; y++)
    for (int x = 3; x <= 5; x++) TEST_ASSERT_EQUAL_HEX16(w, at(x, y));
  XftFontClose(dpy, f);
}

void test_the_next_glyph_starts_one_advance_along(void) {
  XftFont *f = load_xft_font_retry(dpy, 0, "x 8");
  XftColor white = colour("white");
  draw(f, &white, 0, 6, "NN", 2);
  TEST_ASSERT_EQUAL_INT(24, count_not(0));
  TEST_ASSERT_EQUAL_HEX16(0, at(0, 3));
  TEST_ASSERT_NOT_EQUAL_HEX16(0, at(1, 3));
  TEST_ASSERT_NOT_EQUAL_HEX16(0, at(3, 3));
  TEST_ASSERT_EQUAL_HEX16(0, at(4, 3));
  TEST_ASSERT_NOT_EQUAL_HEX16(0, at(6, 3));
  TEST_ASSERT_NOT_EQUAL_HEX16(0, at(8, 3));
  TEST_ASSERT_EQUAL_HEX16(0, at(9, 3));
  XftFontClose(dpy, f);
}

void test_an_opaque_pixel_is_exactly_the_colour_asked_for(void) {
  XftFont *f = load_xft_font_retry(dpy, 0, "x 8");
  XftColor orange = colour("#FF8040");
  draw(f, &orange, 2, 6, "N", 1);
  TEST_ASSERT_EQUAL_HEX16(rgb565(0xFF, 0x80, 0x40), at(3, 2));
  XftFontClose(dpy, f);
}

void test_half_coverage_blends_halfway_with_what_is_underneath(void) {
  XftFont *f = load_xft_font_retry(dpy, 0, "x 8");
  canvas_clear(&cv, rgb565(0, 0, 255));
  XftColor white = colour("white");
  draw(f, &white, 5, 5, "A", 1);
  /* A's two pixels sit at x 5 and 6, one row above the baseline: coverage 128
   * then 255. 128 of 255 white over blue is (128, 128, 255). */
  TEST_ASSERT_EQUAL_HEX16(rgb565(128, 128, 255), at(5, 4));
  TEST_ASSERT_EQUAL_HEX16(rgb565(255, 255, 255), at(6, 4));
  TEST_ASSERT_EQUAL_HEX16(rgb565(0, 0, 255), at(4, 4));
  TEST_ASSERT_EQUAL_HEX16(rgb565(0, 0, 255), at(7, 4));
  XftFontClose(dpy, f);
}

void test_drawing_clips_at_every_edge_without_wrapping_into_the_next_row(void) {
  XftFont *f = load_xft_font_retry(dpy, 0, "x 8");
  XftColor white = colour("white");
  draw(f, &white, -3, 6, "N", 1); /* ink x -2..0: one column shows */
  TEST_ASSERT_EQUAL_INT(4, count_not(0));
  TEST_ASSERT_NOT_EQUAL_HEX16(0, at(0, 2));
  TEST_ASSERT_EQUAL_HEX16(0, at(15, 3));
  canvas_clear(&cv, 0);
  draw(f, &white, 14, 6, "N", 1); /* ink x 15..17: one column shows */
  TEST_ASSERT_EQUAL_INT(4, count_not(0));
  TEST_ASSERT_NOT_EQUAL_HEX16(0, at(15, 2));
  TEST_ASSERT_EQUAL_HEX16(0, at(0, 3));
  canvas_clear(&cv, 0);
  draw(f, &white, 2, 1, "N", 1); /* ink y -3..0: one row shows */
  TEST_ASSERT_EQUAL_INT(3, count_not(0));
  canvas_clear(&cv, 0);
  draw(f, &white, 2, 18, "N", 1); /* ink y 14..17: two rows show */
  TEST_ASSERT_EQUAL_INT(6, count_not(0));
  XftFontClose(dpy, f);
}

void test_wild_coordinates_draw_nothing_and_do_not_overflow(void) {
  XftFont *f = load_xft_font_retry(dpy, 0, "x 8");
  XftColor white = colour("white");
  draw(f, &white, INT_MAX, 6, "N", 1);
  draw(f, &white, INT_MIN, 6, "N", 1);
  draw(f, &white, 2, INT_MAX, "N", 1);
  draw(f, &white, 2, INT_MIN, "N", 1);
  draw(f, &white, INT_MAX - 1, INT_MAX - 1, "NN", 2);
  TEST_ASSERT_EQUAL_INT(0, count_not(0));
  XftFontClose(dpy, f);
}

void test_the_length_limits_how_much_is_drawn(void) {
  XftFont *f = load_xft_font_retry(dpy, 0, "x 8");
  XftColor white = colour("white");
  draw(f, &white, 0, 6, "NN", 1);
  TEST_ASSERT_EQUAL_INT(12, count_not(0));
  canvas_clear(&cv, 0);
  draw(f, &white, 0, 6, "NN", 0);
  draw(f, &white, 0, 6, "NN", -1);
  draw(f, &white, 0, 6, NULL, 3);
  TEST_ASSERT_EQUAL_INT(0, count_not(0));
  XftFontClose(dpy, f);
}

void test_a_character_outside_ascii_draws_the_question_mark_once(void) {
  XftFont *f = load_xft_font_retry(dpy, 0, "x 8");
  XftColor white = colour("white");
  draw(f, &white, 4, 6, "\xC3\xA9", 2); /* e acute: two bytes, one character */
  TEST_ASSERT_EQUAL_INT(1, count_not(0));
  TEST_ASSERT_NOT_EQUAL_HEX16(0, at(4, 5));
  XGlyphInfo g;
  XftTextExtentsUtf8(dpy, f, (const FcChar8 *)"\xC3\xA9", 2, &g);
  TEST_ASSERT_EQUAL_INT(2, g.xOff);
  XftTextExtentsUtf8(dpy, f, (const FcChar8 *)"\xE2\x82\xAC\x01", 4, &g);
  TEST_ASSERT_EQUAL_INT(4, g.xOff);
  XftFontClose(dpy, f);
}

void test_a_pixmap_target_leaves_the_window_alone(void) {
  XftFont *f = load_xft_font_retry(dpy, 0, "x 8");
  XftColor white = colour("white");
  Pixmap pm = XCreatePixmap(dpy, win, 16, 16, 16);
  XGCValues v = {0};
  GC gc = XCreateGC(dpy, win, 0, &v);
  XSetForeground(dpy, gc, 0);
  XFillRectangle(dpy, pm, gc, 0, 0, 16, 16);
  XftDraw *d = XftDrawCreate(dpy, pm, NULL, 1);
  XftDrawStringUtf8(d, &white, f, 2, 6, (const FcChar8 *)"N", 1);
  XftDrawDestroy(d);
  TEST_ASSERT_EQUAL_INT(0, count_not(0));
  XCopyArea(dpy, pm, win, gc, 0, 0, 16, 16, 0, 0);
  TEST_ASSERT_EQUAL_INT(12, count_not(0));
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, pm);
  XftFontClose(dpy, f);
}

void test_a_drawn_glyph_marks_its_pixels_dirty(void) {
  XftFont *f = load_xft_font_retry(dpy, 0, "x 8");
  XftColor white = colour("white");
  canvas_clear_dirty(&cv);
  int x0, x1;
  TEST_ASSERT_FALSE(canvas_dirty_row(&cv, 3, &x0, &x1));
  draw(f, &white, 2, 6, "N", 1);
  TEST_ASSERT_TRUE(canvas_dirty_row(&cv, 3, &x0, &x1));
  TEST_ASSERT_LESS_OR_EQUAL_INT(3, x0);
  TEST_ASSERT_GREATER_OR_EQUAL_INT(5, x1);
  TEST_ASSERT_FALSE(canvas_dirty_row(&cv, 1, &x0, &x1));
  XftFontClose(dpy, f);
}

void test_colour_names_parse_like_the_rest_of_the_shim(void) {
  XftColor c;
  TEST_ASSERT_TRUE(XftColorAllocName(dpy, NULL, 1, "#FF0000", &c));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, c.color.red);
  TEST_ASSERT_EQUAL_HEX16(0, c.color.green);
  TEST_ASSERT_EQUAL_HEX16(0, c.color.blue);
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, c.color.alpha);
  TEST_ASSERT_EQUAL_HEX16(rgb565(255, 0, 0), (uint16_t)c.pixel);
  TEST_ASSERT_TRUE(XftColorAllocName(dpy, NULL, 1, "black", &c));
  TEST_ASSERT_EQUAL_HEX16(0, c.color.red);
  TEST_ASSERT_FALSE(XftColorAllocName(dpy, NULL, 1, "no-such-colour", &c));
}

/* What a channel should become: the colour weighed a out of 255 against what
 * was there, rounded to the nearest. The sum over 255 never ends in exactly
 * .5, so double arithmetic and the shim's integer rounding agree. */
static int weigh(int fg, int under, int a) {
  return (int)((fg * a + under * (255 - a)) / 255.0 + 0.5);
}

void test_every_coverage_blends_to_the_rounded_weighted_mean(void) {
  XftFont *f = load_xft_font_retry(dpy, 0, "x 8");
  XftColor fg = colour("#C86432");
  const uint16_t under = rgb565(255, 130, 0);
  /* What the canvas holds is 565, so the channels come back expanded: 31 of 31
   * red is 255, 32 of 63 green is 130, and 0 is 0. */
  canvas_clear(&cv, under);
  draw(f, &fg, 0, 12, "~", 1);
  for (int i = 0; i < 256; i++) {
    const int x = i % 16, y = i / 16;
    const uint16_t want = rgb565((uint8_t)weigh(0xC8, 255, i),
                                 (uint8_t)weigh(0x64, 130, i),
                                 (uint8_t)weigh(0x32, 0, i));
    char msg[40];
    snprintf(msg, sizeof msg, "coverage %d", i);
    TEST_ASSERT_EQUAL_HEX16_MESSAGE(i ? want : under, at(x, y), msg);
  }
  XftFontClose(dpy, f);
}

void test_text_reaching_below_the_baseline_is_as_tall_as_its_ink(void) {
  XftFont *f = load_xft_font_retry(dpy, 0, "x 8");
  XGlyphInfo g;
  XftTextExtentsUtf8(dpy, f, (const FcChar8 *)"~", 1, &g);
  TEST_ASSERT_EQUAL_INT(16, g.height);
  TEST_ASSERT_EQUAL_INT(12, g.y);
  XftTextExtentsUtf8(dpy, f, (const FcChar8 *)"N~", 2, &g);
  TEST_ASSERT_EQUAL_INT(16, g.height);
  XftFontClose(dpy, f);
}

void test_a_number_stuck_to_the_family_name_is_not_a_size(void) {
  XftFont *f = load_xft_font_retry(dpy, 0, "monospace16");
  TEST_ASSERT_NOT_NULL(f);
  TEST_ASSERT_EQUAL_INT(6, f->ascent);
  XftFontClose(dpy, f);
}

void test_drawing_to_no_drawable_or_a_bitmap_does_nothing(void) {
  XftFont *f = load_xft_font_retry(dpy, 0, "x 8");
  XftColor white = colour("white");
  XftDraw *d = XftDrawCreate(dpy, None, NULL, 1);
  XftDrawStringUtf8(d, &white, f, 2, 6, (const FcChar8 *)"N", 1);
  XftDrawDestroy(d);
  Pixmap bm = XCreatePixmap(dpy, win, 16, 16, 1);
  d = XftDrawCreate(dpy, bm, NULL, 1);
  XftDrawStringUtf8(d, &white, f, 2, 6, (const FcChar8 *)"N", 1);
  XftDrawDestroy(d);
  TEST_ASSERT_EQUAL_INT(0, count_not(0));
  XFreePixmap(dpy, bm);
  XftFontClose(dpy, f);
}

void test_the_screen_number_is_zero(void) {
  TEST_ASSERT_EQUAL_INT(0, screen_number(NULL));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_the_nearest_size_is_chosen_from_the_first_family_in_the_list);
  RUN_TEST(test_a_tie_between_two_sizes_takes_the_smaller);
  RUN_TEST(test_a_name_without_a_size_takes_the_first_size);
  RUN_TEST(test_no_font_installed_gives_no_font);
  RUN_TEST(test_extents_of_one_glyph);
  RUN_TEST(test_extents_of_two_glyphs_add_advances_and_span_both_inks);
  RUN_TEST(test_extents_of_blank_text_have_an_advance_and_no_ink);
  RUN_TEST(test_a_glyph_covers_exactly_its_ink_box);
  RUN_TEST(test_the_next_glyph_starts_one_advance_along);
  RUN_TEST(test_an_opaque_pixel_is_exactly_the_colour_asked_for);
  RUN_TEST(test_half_coverage_blends_halfway_with_what_is_underneath);
  RUN_TEST(test_drawing_clips_at_every_edge_without_wrapping_into_the_next_row);
  RUN_TEST(test_wild_coordinates_draw_nothing_and_do_not_overflow);
  RUN_TEST(test_the_length_limits_how_much_is_drawn);
  RUN_TEST(test_a_character_outside_ascii_draws_the_question_mark_once);
  RUN_TEST(test_a_pixmap_target_leaves_the_window_alone);
  RUN_TEST(test_a_drawn_glyph_marks_its_pixels_dirty);
  RUN_TEST(test_colour_names_parse_like_the_rest_of_the_shim);
  RUN_TEST(test_every_coverage_blends_to_the_rounded_weighted_mean);
  RUN_TEST(test_text_reaching_below_the_baseline_is_as_tall_as_its_ink);
  RUN_TEST(test_a_number_stuck_to_the_family_name_is_not_a_size);
  RUN_TEST(test_drawing_to_no_drawable_or_a_bitmap_does_nothing);
  RUN_TEST(test_the_screen_number_is_zero);
  return UNITY_END();
}
