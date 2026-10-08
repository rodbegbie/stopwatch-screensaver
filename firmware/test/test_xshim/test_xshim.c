#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include "colors.h"
#include "core/canvas.h"
#include "erase.h"
#include "fps.h"
#include "screenhack.h"
#include "utils.h"
#include "x11shim/xshim.h"

static Canvas cv;
static Display *dpy;
static Window win = 1;

void setUp(void) {
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&cv, 16, 16, malloc));
  dpy = xshim_open_display(&cv);
}
void tearDown(void) {
  xshim_close_display(dpy);
  canvas_free(&cv);
}

static int count_set(void) {
  int n = 0;
  for (int i = 0; i < cv.w * cv.h; i++) n += cv.px[i] != 0;
  return n;
}
static uint16_t at(int x, int y) { return cv.px[y * cv.w + x]; }

void test_white_and_black_pixel(void) {
  TEST_ASSERT_EQUAL_HEX(0xFFFF, WhitePixel(dpy, DefaultScreen(dpy)));
  TEST_ASSERT_EQUAL_HEX(0x0000, BlackPixel(dpy, DefaultScreen(dpy)));
}

void test_alloc_color_red_gives_f800_and_white_gives_ffff(void) {
  XColor c = {0};
  c.red = 0xFFFF;
  c.flags = DoRed | DoGreen | DoBlue;
  TEST_ASSERT_TRUE(XAllocColor(dpy, 1, &c) != 0);
  TEST_ASSERT_EQUAL_HEX(0xF800, c.pixel);
  c.red = c.green = c.blue = 0xFFFF;
  TEST_ASSERT_TRUE(XAllocColor(dpy, 1, &c) != 0);
  TEST_ASSERT_EQUAL_HEX(0xFFFF, c.pixel);
}

void test_foreground_via_gc_used_by_fill_rectangle(void) {
  XGCValues v;
  v.foreground = 0xF800;
  GC gc = XCreateGC(dpy, win, GCForeground, &v);
  XFillRectangle(dpy, win, gc, 2, 3, 4, 2);
  TEST_ASSERT_EQUAL_INT(8, count_set());
  TEST_ASSERT_EQUAL_HEX16(0xF800, at(2, 3));
  XSetForeground(dpy, gc, 0x07E0);
  XDrawPoint(dpy, win, gc, 10, 10);
  TEST_ASSERT_EQUAL_HEX16(0x07E0, at(10, 10));
  XFreeGC(dpy, gc);
}

void test_get_window_attributes_reports_canvas_size(void) {
  XWindowAttributes a;
  XGetWindowAttributes(dpy, win, &a);
  TEST_ASSERT_EQUAL_INT(16, a.width);
  TEST_ASSERT_EQUAL_INT(16, a.height);
}

void test_clear_window_uses_black_background(void) {
  XGCValues v;
  v.foreground = 0xFFFF;
  GC gc = XCreateGC(dpy, win, GCForeground, &v);
  XFillRectangle(dpy, win, gc, 0, 0, 16, 16);
  TEST_ASSERT_EQUAL_INT(256, count_set());
  XClearWindow(dpy, win);
  TEST_ASSERT_EQUAL_INT(0, count_set());
  XFreeGC(dpy, gc);
}

void test_draw_line_and_lines_connect_points(void) {
  XGCValues v;
  v.foreground = 0xFFFF;
  GC gc = XCreateGC(dpy, win, GCForeground, &v);
  XDrawLine(dpy, win, gc, 0, 0, 5, 0);
  TEST_ASSERT_EQUAL_INT(6, count_set());
  XClearWindow(dpy, win);
  XPoint pts[3] = {{0, 0}, {4, 0}, {4, 4}};
  XDrawLines(dpy, win, gc, pts, 3, CoordModeOrigin);
  TEST_ASSERT_EQUAL_INT(9, count_set());
  XFreeGC(dpy, gc);
}

void test_fill_arc_full_circle_draws_partial_arc_does_not(void) {
  XGCValues v;
  v.foreground = 0xFFFF;
  GC gc = XCreateGC(dpy, win, GCForeground, &v);
  XFillArc(dpy, win, gc, 2, 2, 7, 7, 0, 360 * 64);
  int full = count_set();
  TEST_ASSERT_TRUE(full > 20);
  XClearWindow(dpy, win);
  XFillArc(dpy, win, gc, 2, 2, 7, 7, 0, 90 * 64);
  TEST_ASSERT_EQUAL_INT(0, count_set());
  XFreeGC(dpy, gc);
}

void test_fill_polygon_fills_triangle(void) {
  XGCValues v;
  v.foreground = 0xFFFF;
  GC gc = XCreateGC(dpy, win, GCForeground, &v);
  XPoint pts[3] = {{0, 0}, {8, 0}, {0, 8}};
  XFillPolygon(dpy, win, gc, pts, 3, Complex, CoordModeOrigin);
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(1, 1));
  TEST_ASSERT_EQUAL_HEX16(0, at(7, 7));
  XFreeGC(dpy, gc);
}

void test_fill_polygon_draws_with_1000_points(void) {
  enum { N = 1000 };
  static XPoint pts[N];
  for (int i = 0; i < N; i++) {
    double a = 2 * 3.14159265358979 * i / N;
    pts[i].x = (short)(8 + (int)(6 * cos(a) + 0.5));
    pts[i].y = (short)(8 + (int)(6 * sin(a) + 0.5));
  }
  XGCValues v;
  v.foreground = 0xFFFF;
  GC gc = XCreateGC(dpy, win, GCForeground, &v);
  XFillPolygon(dpy, win, gc, pts, N, Complex, CoordModeOrigin);
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(8, 8));
  TEST_ASSERT_EQUAL_HEX16(0, at(0, 0));
  XFreeGC(dpy, gc);
}

static const char *const defs[] = {
    ".background:\tblack", "*count:\t600", ".foreground: white",
    "*delay:   10000",     "*ratio: 0.5", "*flag: true", "*name: hello", 0};

void test_resources_parse_star_and_dot_prefixes_and_tabs(void) {
  xshim_set_defaults(defs);
  TEST_ASSERT_EQUAL_INT(600, get_integer_resource(dpy, "count", "Integer"));
  TEST_ASSERT_EQUAL_INT(10000, get_integer_resource(dpy, "delay", "Integer"));
  TEST_ASSERT_TRUE(get_float_resource(dpy, "ratio", "Float") == 0.5);
  TEST_ASSERT_TRUE(get_boolean_resource(dpy, "flag", "Boolean"));
  char *s = get_string_resource(dpy, "name", "Name");
  TEST_ASSERT_EQUAL_STRING("hello", s);
  free(s);
}

void test_resources_missing_integer_returns_zero(void) {
  xshim_set_defaults(defs);
  TEST_ASSERT_EQUAL_INT(0, get_integer_resource(dpy, "nosuch", "Integer"));
}

void test_pixel_resource_black_white_hex(void) {
  xshim_set_defaults(defs);
  TEST_ASSERT_EQUAL_HEX(0x0000, get_pixel_resource(dpy, 1, "background", "Background"));
  TEST_ASSERT_EQUAL_HEX(0xFFFF, get_pixel_resource(dpy, 1, "foreground", "Foreground"));
  static const char *const hex[] = {"*c: #ff0000", 0};
  xshim_set_defaults(hex);
  TEST_ASSERT_EQUAL_HEX(0xF800, get_pixel_resource(dpy, 1, "c", "C"));
}

void test_gc_accepts_function_and_line_width_fields(void) {
  XGCValues v;
  v.function = GXcopy;
  v.line_width = 3;
  v.foreground = 0xFFFF;
  GC gc = XCreateGC(dpy, win, GCForeground | GCFunction | GCLineWidth, &v);
  TEST_ASSERT_NOT_NULL(gc);
  XDrawPoint(dpy, win, gc, 1, 1);
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(1, 1));
  XFreeGC(dpy, gc);
}

void test_window_attributes_have_depth_visual_screen(void) {
  XWindowAttributes a;
  XGetWindowAttributes(dpy, win, &a);
  TEST_ASSERT_EQUAL_INT(16, a.depth);
  Visual *vis = a.visual;
  Screen *scr = a.screen;
  (void)vis;
  (void)scr;
}

void test_button_press_event_fields_exist(void) {
  XEvent e;
  e.type = ButtonPress;
  e.xbutton.button = 2;
  TEST_ASSERT_EQUAL_INT(ButtonPress, e.type);
  TEST_ASSERT_EQUAL_UINT(2, e.xbutton.button);
}

void test_xrectangle_type_exists(void) {
  XRectangle r = {1, 2, 3, 4};
  TEST_ASSERT_EQUAL_INT(3, r.width);
}

void test_progname_is_set(void) {
  TEST_ASSERT_NOT_NULL(progname);
  TEST_ASSERT_TRUE(strlen(progname) > 0);
}

void test_pixel_resource_x11_colour_names(void) {
  static const char *const names[] = {"*a: magenta", "*b: yellow", "*c: green",
                                      "*d: red", "*e: blue", "*f: cyan",
                                      "*g: orange", 0};
  xshim_set_defaults(names);
  TEST_ASSERT_EQUAL_HEX(0xF81F, get_pixel_resource(dpy, 1, "a", "A"));
  TEST_ASSERT_EQUAL_HEX(0xFFE0, get_pixel_resource(dpy, 1, "b", "B"));
  TEST_ASSERT_EQUAL_HEX(0x07E0, get_pixel_resource(dpy, 1, "c", "C"));
  TEST_ASSERT_EQUAL_HEX(0xF800, get_pixel_resource(dpy, 1, "d", "D"));
  TEST_ASSERT_EQUAL_HEX(0x001F, get_pixel_resource(dpy, 1, "e", "E"));
  TEST_ASSERT_EQUAL_HEX(0x07FF, get_pixel_resource(dpy, 1, "f", "F"));
  TEST_ASSERT_EQUAL_HEX(0xFD20, get_pixel_resource(dpy, 1, "g", "G"));
}

void test_pixel_resource_names_are_case_insensitive(void) {
  static const char *const names[] = {"*a: Magenta", 0};
  xshim_set_defaults(names);
  TEST_ASSERT_EQUAL_HEX(0xF81F, get_pixel_resource(dpy, 1, "a", "A"));
}

void test_erase_window_clears_and_reports_done(void) {
  XGCValues v;
  v.foreground = 0xFFFF;
  GC gc = XCreateGC(dpy, win, GCForeground, &v);
  XFillRectangle(dpy, win, gc, 0, 0, 16, 16);
  eraser_state *st = erase_window(dpy, win, NULL);
  TEST_ASSERT_NULL(st);
  TEST_ASSERT_EQUAL_INT(0, count_set());
  eraser_free(NULL);
  XFreeGC(dpy, gc);
}

void test_make_random_colormap_fills_colours_in_rgb565(void) {
  XColor colors[8];
  memset(colors, 0, sizeof(colors));
  int n = 8;
  make_random_colormap(NULL, NULL, 1, colors, &n, True, True, NULL, False);
  TEST_ASSERT_EQUAL_INT(8, n);
  int distinct = 0;
  for (int i = 0; i < n; i++) {
    TEST_ASSERT_EQUAL_HEX(rgb565_from16(colors[i].red, colors[i].green, colors[i].blue),
                          colors[i].pixel);
    TEST_ASSERT_TRUE(colors[i].pixel != 0);
    if (i > 0 && colors[i].pixel != colors[i - 1].pixel) distinct++;
  }
  TEST_ASSERT_TRUE(distinct >= 2);
}

void test_fill_rectangles_fills_each_rectangle_in_the_gc_colour(void) {
  XGCValues v;
  v.foreground = 0xF800;
  GC gc = XCreateGC(dpy, win, GCForeground, &v);
  XRectangle rs[2] = {{1, 1, 2, 2}, {10, 12, 3, 1}};
  XFillRectangles(dpy, win, gc, rs, 2);
  TEST_ASSERT_EQUAL_INT(7, count_set());
  TEST_ASSERT_EQUAL_HEX16(0xF800, at(2, 2));
  TEST_ASSERT_EQUAL_HEX16(0xF800, at(12, 12));
  XFreeGC(dpy, gc);
}

void test_fill_rectangles_with_zero_count_draws_nothing(void) {
  XGCValues v;
  v.foreground = 0xFFFF;
  GC gc = XCreateGC(dpy, win, GCForeground, &v);
  XRectangle r = {0, 0, 4, 4};
  XFillRectangles(dpy, win, gc, &r, 0);
  TEST_ASSERT_EQUAL_INT(0, count_set());
  XFreeGC(dpy, gc);
}

void test_gc_accepts_background_field_and_mask(void) {
  XGCValues v;
  v.foreground = 0xFFFF;
  v.background = 0x0000;
  GC gc = XCreateGC(dpy, win, GCForeground | GCBackground | GCFunction, &v);
  TEST_ASSERT_NOT_NULL(gc);
  XFillRectangle(dpy, win, gc, 0, 0, 2, 2);
  TEST_ASSERT_EQUAL_INT(4, count_set());
  XFreeGC(dpy, gc);
}

void test_event_helper_reports_button_press_only(void) {
  XEvent e;
  memset(&e, 0, sizeof(e));
  e.type = ButtonPress;
  TEST_ASSERT_TRUE(screenhack_event_helper(dpy, win, &e));
  e.type = 0;
  TEST_ASSERT_FALSE(screenhack_event_helper(dpy, win, &e));
}

void test_draw_points_plots_each_point_in_the_gc_colour(void) {
  XGCValues v;
  v.foreground = 0xF800;
  GC gc = XCreateGC(dpy, win, GCForeground, &v);
  XPoint pts[3] = {{1, 1}, {5, 2}, {15, 15}};
  XDrawPoints(dpy, win, gc, pts, 3, CoordModeOrigin);
  TEST_ASSERT_EQUAL_INT(3, count_set());
  TEST_ASSERT_EQUAL_HEX16(0xF800, at(5, 2));
  XFreeGC(dpy, gc);
}

void test_draw_points_with_zero_count_draws_nothing(void) {
  XGCValues v;
  v.foreground = 0xFFFF;
  GC gc = XCreateGC(dpy, win, GCForeground, &v);
  XPoint p = {3, 3};
  XDrawPoints(dpy, win, gc, &p, 0, CoordModeOrigin);
  TEST_ASSERT_EQUAL_INT(0, count_set());
  XFreeGC(dpy, gc);
}

void test_change_gc_sets_foreground_only_when_masked(void) {
  XGCValues v;
  v.foreground = 0x07E0;
  GC gc = XCreateGC(dpy, win, GCForeground, &v);
  v.foreground = 0x001F;
  XChangeGC(dpy, gc, GCFunction, &v);
  XDrawPoint(dpy, win, gc, 1, 1);
  TEST_ASSERT_EQUAL_HEX16(0x07E0, at(1, 1));
  v.foreground = 0xF800;
  XChangeGC(dpy, gc, GCForeground, &v);
  XDrawPoint(dpy, win, gc, 2, 2);
  TEST_ASSERT_EQUAL_HEX16(0xF800, at(2, 2));
  XFreeGC(dpy, gc);
}

void test_draw_rectangle_outlines_a_w_plus_1_by_h_plus_1_box(void) {
  XGCValues v;
  v.foreground = 0xFFFF;
  GC gc = XCreateGC(dpy, win, GCForeground, &v);
  XDrawRectangle(dpy, win, gc, 2, 3, 4, 2);
  TEST_ASSERT_EQUAL_INT(12, count_set());
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(2, 3));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(6, 5));
  TEST_ASSERT_EQUAL_HEX16(0x0000, at(4, 4));
  XFreeGC(dpy, gc);
}

static int distinct_pixels(const XColor *c, int n) {
  int distinct = 0;
  for (int i = 0; i < n; i++) {
    int seen = 0;
    for (int j = 0; j < i; j++) seen |= c[j].pixel == c[i].pixel;
    distinct += !seen;
  }
  return distinct;
}

void test_make_uniform_colormap_gives_distinct_rgb565_colours(void) {
  XColor colors[12];
  memset(colors, 0, sizeof(colors));
  int n = 12;
  Bool writable = True;
  make_uniform_colormap(NULL, NULL, 1, colors, &n, True, &writable, False);
  TEST_ASSERT_EQUAL_INT(12, n);
  TEST_ASSERT_FALSE(writable);
  for (int i = 0; i < n; i++)
    TEST_ASSERT_EQUAL_HEX(rgb565_from16(colors[i].red, colors[i].green, colors[i].blue),
                          colors[i].pixel);
  TEST_ASSERT_EQUAL_INT(12, distinct_pixels(colors, n));
}

void test_make_smooth_colormap_loops_without_jumps(void) {
  for (int trial = 0; trial < 20; trial++) {
    XColor colors[64];
    memset(colors, 0, sizeof(colors));
    int n = 64;
    Bool writable = True;
    make_smooth_colormap(NULL, NULL, 1, colors, &n, True, &writable, False);
    TEST_ASSERT_EQUAL_INT(64, n);
    TEST_ASSERT_FALSE(writable);
    for (int i = 0; i < n; i++) {
      const XColor *a = &colors[i], *b = &colors[(i + 1) % n];
      TEST_ASSERT_EQUAL_HEX(rgb565_from16(a->red, a->green, a->blue), a->pixel);
      TEST_ASSERT_TRUE(abs((int)a->red - (int)b->red) < 0x4000);
      TEST_ASSERT_TRUE(abs((int)a->green - (int)b->green) < 0x4000);
      TEST_ASSERT_TRUE(abs((int)a->blue - (int)b->blue) < 0x4000);
    }
    TEST_ASSERT_TRUE(distinct_pixels(colors, n) >= 16);
  }
}

void test_free_colors_accepts_a_colormap_from_the_helpers(void) {
  XColor colors[4];
  memset(colors, 0, sizeof(colors));
  int n = 4;
  make_uniform_colormap(NULL, NULL, 1, colors, &n, True, NULL, False);
  free_colors(NULL, 1, colors, n);
}

void test_root_window_of_screen_accepts_null_and_names_the_runner_window(void) {
  TEST_ASSERT_EQUAL_UINT(win, RootWindowOfScreen(NULL));
}

void test_select_input_is_accepted_and_ignored(void) {
  TEST_ASSERT_EQUAL_INT(0, XSelectInput(dpy, win, PointerMotionMask));
}

void test_pointer_motion_mask_is_the_x11_bit(void) {
  TEST_ASSERT_EQUAL_INT64(1L << 6, PointerMotionMask);
}

void test_window_attributes_have_your_event_mask(void) {
  XWindowAttributes a = {0};
  a.your_event_mask = 5;
  TEST_ASSERT_EQUAL_INT64(5, a.your_event_mask);
}

void test_ya_rand_init_with_a_seed_repeats_the_sequence(void) {
  ya_rand_init(42);
  long a = LRAND();
  ya_rand_init(42);
  TEST_ASSERT_EQUAL_INT64(a, LRAND());
}

void test_nrand_stays_below_n_and_covers_the_range(void) {
  int seen[10] = {0};
  for (int i = 0; i < 1000; i++) {
    int v = NRAND(10);
    TEST_ASSERT_TRUE(v >= 0 && v < 10);
    seen[v] = 1;
  }
  for (int i = 0; i < 10; i++) TEST_ASSERT_TRUE(seen[i]);
}

void test_lrand_is_non_negative_and_below_maxrand(void) {
  for (int i = 0; i < 1000; i++) {
    TEST_ASSERT_TRUE(LRAND() >= 0);
    TEST_ASSERT_TRUE((double)LRAND() < MAXRAND);
  }
  TEST_ASSERT_TRUE(MAXRAND == 2147483648.0);
}

void test_countof_gives_the_array_length(void) {
  int a[5];
  (void)a;
  TEST_ASSERT_EQUAL_INT(5, countof(a));
}

void test_fps_stand_ins_accept_a_null_state(void) {
  fps_compute(NULL, 0, 0);
  fps_draw(NULL);
  fps_free(NULL);
}

void test_xrm_option_strings_are_writable_like_xscreensavers(void) {
  char option[8] = "-xx";
  char specifier[8] = "x";
  XrmOptionDescRec o = {option, specifier, 0, NULL};
  o.specifier[0] = '.';
  TEST_ASSERT_EQUAL_STRING(".", specifier);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_draw_points_plots_each_point_in_the_gc_colour);
  RUN_TEST(test_draw_points_with_zero_count_draws_nothing);
  RUN_TEST(test_change_gc_sets_foreground_only_when_masked);
  RUN_TEST(test_draw_rectangle_outlines_a_w_plus_1_by_h_plus_1_box);
  RUN_TEST(test_make_uniform_colormap_gives_distinct_rgb565_colours);
  RUN_TEST(test_make_smooth_colormap_loops_without_jumps);
  RUN_TEST(test_free_colors_accepts_a_colormap_from_the_helpers);
  RUN_TEST(test_fill_rectangles_fills_each_rectangle_in_the_gc_colour);
  RUN_TEST(test_fill_rectangles_with_zero_count_draws_nothing);
  RUN_TEST(test_gc_accepts_background_field_and_mask);
  RUN_TEST(test_event_helper_reports_button_press_only);
  RUN_TEST(test_white_and_black_pixel);
  RUN_TEST(test_alloc_color_red_gives_f800_and_white_gives_ffff);
  RUN_TEST(test_foreground_via_gc_used_by_fill_rectangle);
  RUN_TEST(test_get_window_attributes_reports_canvas_size);
  RUN_TEST(test_clear_window_uses_black_background);
  RUN_TEST(test_draw_line_and_lines_connect_points);
  RUN_TEST(test_fill_arc_full_circle_draws_partial_arc_does_not);
  RUN_TEST(test_fill_polygon_fills_triangle);
  RUN_TEST(test_fill_polygon_draws_with_1000_points);
  RUN_TEST(test_resources_parse_star_and_dot_prefixes_and_tabs);
  RUN_TEST(test_resources_missing_integer_returns_zero);
  RUN_TEST(test_pixel_resource_black_white_hex);
  RUN_TEST(test_gc_accepts_function_and_line_width_fields);
  RUN_TEST(test_window_attributes_have_depth_visual_screen);
  RUN_TEST(test_button_press_event_fields_exist);
  RUN_TEST(test_xrectangle_type_exists);
  RUN_TEST(test_progname_is_set);
  RUN_TEST(test_pixel_resource_x11_colour_names);
  RUN_TEST(test_pixel_resource_names_are_case_insensitive);
  RUN_TEST(test_erase_window_clears_and_reports_done);
  RUN_TEST(test_make_random_colormap_fills_colours_in_rgb565);
  RUN_TEST(test_root_window_of_screen_accepts_null_and_names_the_runner_window);
  RUN_TEST(test_select_input_is_accepted_and_ignored);
  RUN_TEST(test_pointer_motion_mask_is_the_x11_bit);
  RUN_TEST(test_window_attributes_have_your_event_mask);
  RUN_TEST(test_ya_rand_init_with_a_seed_repeats_the_sequence);
  RUN_TEST(test_nrand_stays_below_n_and_covers_the_range);
  RUN_TEST(test_lrand_is_non_negative_and_below_maxrand);
  RUN_TEST(test_countof_gives_the_array_length);
  RUN_TEST(test_fps_stand_ins_accept_a_null_state);
  RUN_TEST(test_xrm_option_strings_are_writable_like_xscreensavers);
  return UNITY_END();
}
