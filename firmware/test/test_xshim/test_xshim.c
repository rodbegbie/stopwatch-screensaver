#include <limits.h>
#include <math.h>
#include <sanitizer/allocator_interface.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <unity.h>
#include <zlib.h>

#include "colors.h"
#include "countof.h"
#include "core/canvas.h"
#include "doubletime.h"
#include "easing.h"
#include "erase.h"
#include "fps.h"
#include "pow2.h"
#include "alpha.h"
#include "screenhack.h"
#include "thread_util.h"
#include "utf8wc.h"
#include "utils.h"
#include "xshm.h"
#include "x11shim/arc.h"
#include "x11shim/pixmap.h"
#include "x11shim/stroke.h"
#include "X11/Intrinsic.h"
#include "textclient.h"
#include "hsv.h"
#include "spline.h"
#include "x11shim/xshim.h"
#include "ximage-loader.h"

static Canvas cv;
static Display *dpy;
static Window win = XSHIM_WINDOW;

static size_t live_before;

void setUp(void) {
  live_before = __sanitizer_get_current_allocated_bytes();
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&cv, 16, 16, malloc));
  dpy = xshim_open_display(&cv);
}
void tearDown(void) {
  xshim_close_display(dpy);
  canvas_free(&cv);
  TEST_ASSERT_EQUAL_UINT64_MESSAGE(live_before,
                                   __sanitizer_get_current_allocated_bytes(),
                                   "test leaked memory");
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
  TEST_ASSERT_EQUAL_HEX(px_swap(0xF800), c.pixel);
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

void test_clear_window_uses_the_window_background(void) {
  XSetWindowBackground(dpy, win, 0x07E0);
  XClearWindow(dpy, win);
  TEST_ASSERT_EQUAL_INT(256, count_set());
  TEST_ASSERT_EQUAL_HEX(0x07E0, at(0, 0));
  TEST_ASSERT_EQUAL_HEX(0x07E0, at(15, 15));
  XSetWindowBackground(dpy, win, 0);
  XClearWindow(dpy, win);
  TEST_ASSERT_EQUAL_INT(0, count_set());
}

void test_graphics_functions_have_xs_values(void) {
  TEST_ASSERT_EQUAL_INT(0x3, GXcopy);
  TEST_ASSERT_EQUAL_INT(0x6, GXxor);
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

void test_fill_arc_full_circle_draws(void) {
  XGCValues v;
  v.foreground = 0xFFFF;
  GC gc = XCreateGC(dpy, win, GCForeground, &v);
  XFillArc(dpy, win, gc, 2, 2, 7, 7, 0, 360 * 64);
  TEST_ASSERT_TRUE(count_set() > 20);
  XFreeGC(dpy, gc);
}

void test_fill_arcs_fills_each_full_arc_in_the_gc_colour(void) {
  XGCValues v;
  v.foreground = 0xFFFF;
  GC gc = XCreateGC(dpy, win, GCForeground, &v);
  XArc arcs[2] = {{2, 2, 5, 5, 0, 360 * 64}, {9, 9, 5, 5, 0, 360 * 64}};
  XFillArcs(dpy, win, gc, arcs, 1);
  int one = count_set();
  XClearWindow(dpy, win);
  XFillArcs(dpy, win, gc, arcs, 2);
  TEST_ASSERT_TRUE(one > 10);
  TEST_ASSERT_EQUAL_INT(2 * one, count_set());
  XClearWindow(dpy, win);
  XFillArcs(dpy, win, gc, arcs, 0);
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
  TEST_ASSERT_EQUAL_HEX(px_swap(0xF800), get_pixel_resource(dpy, 1, "c", "C"));
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

void test_a_point_drawn_with_gxxor_flips_the_pixel_and_drawing_it_twice_restores_it(void) {
  XGCValues v = {0};
  v.foreground = 0x00FF;
  GC gc = XCreateGC(dpy, win, GCForeground, &v);
  XSetFunction(dpy, gc, GXxor);
  canvas_clear(&cv, 0x0F0F);
  XDrawPoint(dpy, win, gc, 3, 4);
  TEST_ASSERT_EQUAL_HEX16(0x0FF0, at(3, 4));
  TEST_ASSERT_EQUAL_HEX16(0x0F0F, at(4, 4));
  XDrawPoint(dpy, win, gc, 3, 4);
  TEST_ASSERT_EQUAL_HEX16(0x0F0F, at(3, 4));
  XDrawPoint(dpy, win, gc, -1, 0);
  XDrawPoint(dpy, win, gc, 3, -1);
  XDrawPoint(dpy, win, gc, cv.w, cv.h - 1);
  XDrawPoint(dpy, win, gc, 3, cv.h);
  XSetFunction(dpy, gc, GXor);
  XDrawPoint(dpy, win, gc, 3, 4);
  TEST_ASSERT_EQUAL_HEX16(0x00FF, at(3, 4));
  XSetFunction(dpy, gc, GXcopy);
  XDrawPoint(dpy, win, gc, 3, 4);
  TEST_ASSERT_EQUAL_HEX16(0x00FF, at(3, 4));
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

void test_framework_resources_fall_back_to_builtin_defaults(void) {
  static const char *const none[] = {NULL};
  xshim_set_defaults(none);
  TEST_ASSERT_TRUE(get_float_resource(dpy, "delta3d", "Float") == 1.5);
  TEST_ASSERT_EQUAL_HEX(px_swap(0xF81F), get_pixel_resource(dpy, 0, "both3d", "Color"));
  TEST_ASSERT_EQUAL_HEX(px_swap(0xF800), get_pixel_resource(dpy, 0, "right3d", "Color"));
  TEST_ASSERT_EQUAL_HEX(px_swap(0x001F), get_pixel_resource(dpy, 0, "left3d", "Color"));
  TEST_ASSERT_EQUAL_HEX(0x0000, get_pixel_resource(dpy, 0, "none3d", "Color"));
}

void test_hack_defaults_beat_the_builtin_fallbacks(void) {
  static const char *const own[] = {"*delta3d: 2.5", NULL};
  xshim_set_defaults(own);
  TEST_ASSERT_TRUE(get_float_resource(dpy, "delta3d", "Float") == 2.5);
}

void test_overrides_beat_the_hacks_own_defaults(void) {
  static const char *const own[] = {"*count: -5", "*delay: 20000", NULL};
  static const char *const over[] = {"*count: 2", NULL};
  xshim_set_defaults(own);
  xshim_set_overrides(over);
  TEST_ASSERT_EQUAL_INT(2, get_integer_resource(dpy, "count", "Int"));
  TEST_ASSERT_EQUAL_INT(20000, get_integer_resource(dpy, "delay", "Usecs"));
  xshim_set_overrides(NULL);
  TEST_ASSERT_EQUAL_INT(-5, get_integer_resource(dpy, "count", "Int"));
}

/* xlockmore.c puts "<Class>.foreground: white" in a hack's defaults. */
void test_a_class_prefixed_default_is_found(void) {
  static const char *const own[] = {"UnknownPleasures.foreground: white",
                                    "*count: 80", NULL};
  xshim_set_defaults(own);
  TEST_ASSERT_EQUAL_HEX(0xFFFF,
                        get_pixel_resource(dpy, 1, "foreground", "Foreground"));
  char *s = get_string_resource(dpy, "foreground", "Foreground");
  TEST_ASSERT_NOT_NULL(s);
  TEST_ASSERT_EQUAL_STRING("white", s);
  free(s);
  TEST_ASSERT_EQUAL_INT(80, get_integer_resource(dpy, "count", "Int"));
}

/* xlockmore.c lists "<Class>.background: black" before the hack's own
 * "*background: white" (Substrate), and the hack's own line must win. */
void test_a_plain_default_beats_a_class_prefixed_one_listed_earlier(void) {
  static const char *const own[] = {"Substrate.background: black",
                                    "*background: white", NULL};
  xshim_set_defaults(own);
  TEST_ASSERT_EQUAL_HEX(0xFFFF,
                        get_pixel_resource(dpy, 1, "background", "Background"));
  TEST_ASSERT_EQUAL_HEX(0xFFFF, xshim_background_pixel());
}

void test_an_override_beats_a_class_prefixed_default(void) {
  static const char *const own[] = {"Cls.foreground: white", NULL};
  static const char *const over[] = {"*foreground: black", NULL};
  xshim_set_defaults(own);
  xshim_set_overrides(over);
  TEST_ASSERT_EQUAL_HEX(0x0000,
                        get_pixel_resource(dpy, 1, "foreground", "Foreground"));
  xshim_set_overrides(NULL);
}

void test_a_class_prefix_does_not_match_a_longer_or_different_name(void) {
  static const char *const own[] = {"Cls.foregroundish: white",
                                    "Cls.xforeground: white", NULL};
  xshim_set_defaults(own);
  TEST_ASSERT_NULL(get_string_resource(dpy, "foreground", "Foreground"));
}

void test_progclass_is_set(void) {
  TEST_ASSERT_NOT_NULL(progclass);
  TEST_ASSERT_TRUE(strlen(progclass) > 0);
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
  TEST_ASSERT_EQUAL_HEX(px_swap(0xF81F), get_pixel_resource(dpy, 1, "a", "A"));
  TEST_ASSERT_EQUAL_HEX(px_swap(0xFFE0), get_pixel_resource(dpy, 1, "b", "B"));
  TEST_ASSERT_EQUAL_HEX(px_swap(0x07E0), get_pixel_resource(dpy, 1, "c", "C"));
  TEST_ASSERT_EQUAL_HEX(px_swap(0xF800), get_pixel_resource(dpy, 1, "d", "D"));
  TEST_ASSERT_EQUAL_HEX(px_swap(0x001F), get_pixel_resource(dpy, 1, "e", "E"));
  TEST_ASSERT_EQUAL_HEX(px_swap(0x07FF), get_pixel_resource(dpy, 1, "f", "F"));
  TEST_ASSERT_EQUAL_HEX(px_swap(0xFD20), get_pixel_resource(dpy, 1, "g", "G"));
}

void test_pixel_resource_names_are_case_insensitive(void) {
  static const char *const names[] = {"*a: Magenta", 0};
  xshim_set_defaults(names);
  TEST_ASSERT_EQUAL_HEX(px_swap(0xF81F), get_pixel_resource(dpy, 1, "a", "A"));
}

static double erase_now;
static double erase_clock(void) { return erase_now; }

void test_erase_window_keeps_the_picture_until_the_wipe_ends(void) {
  XGCValues v;
  v.foreground = 0xFFFF;
  GC gc = XCreateGC(dpy, win, GCForeground, &v);
  XFillRectangle(dpy, win, gc, 0, 0, 16, 16);
  const int drawn = count_set();
  erase_now = 50.0;
  xshim_set_clock(erase_clock);

  eraser_state *st = erase_window(dpy, win, NULL);
  TEST_ASSERT_NOT_NULL(st);
  TEST_ASSERT_EQUAL_INT(drawn, count_set());
  st = erase_window(dpy, win, st);
  TEST_ASSERT_NOT_NULL(st);
  erase_now += 2.0;
  st = erase_window(dpy, win, st);
  TEST_ASSERT_NULL(st);
  TEST_ASSERT_EQUAL_INT(0, count_set());

  xshim_set_clock(NULL);
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

static void mean_saturation_and_value(const XColor *c, int n, double *sat,
                                      double *val) {
  double s = 0, v = 0;
  for (int i = 0; i < n; i++) {
    const double r = c[i].red / 65535.0, g = c[i].green / 65535.0,
                 b = c[i].blue / 65535.0;
    const double hi = fmax(r, fmax(g, b)), lo = fmin(r, fmin(g, b));
    s += hi > 0 ? (hi - lo) / hi : 0;
    v += hi;
  }
  *sat = s / n;
  *val = v / n;
}

void test_make_smooth_colormap_is_not_always_a_bright_rainbow(void) {
  srandom(1);
  int muted = 0, dark = 0;
  for (int trial = 0; trial < 200; trial++) {
    XColor colors[64];
    int n = 64;
    Bool writable = False;
    make_smooth_colormap(NULL, NULL, 1, colors, &n, True, &writable, False);
    double sat, val;
    mean_saturation_and_value(colors, n, &sat, &val);
    TEST_ASSERT_TRUE(sat > 0.15);
    TEST_ASSERT_TRUE(val > 0.25);
    if (sat < 0.55) muted++;
    if (val < 0.6) dark++;
  }
  TEST_ASSERT_TRUE(muted >= 20);
  TEST_ASSERT_TRUE(dark >= 20);
}

void test_make_smooth_colormap_does_not_always_span_the_hue_wheel(void) {
  srandom(2);
  int narrow = 0;
  for (int trial = 0; trial < 200; trial++) {
    XColor colors[64];
    int n = 64;
    Bool writable = False;
    make_smooth_colormap(NULL, NULL, 1, colors, &n, True, &writable, False);
    int reds = 0, greens = 0, blues = 0;
    for (int i = 0; i < n; i++) {
      if (colors[i].red > colors[i].green + 0x4000 && colors[i].red > colors[i].blue + 0x4000) reds++;
      if (colors[i].green > colors[i].red + 0x4000 && colors[i].green > colors[i].blue + 0x4000) greens++;
      if (colors[i].blue > colors[i].red + 0x4000 && colors[i].blue > colors[i].green + 0x4000) blues++;
    }
    if (!reds || !greens || !blues) narrow++;
  }
  TEST_ASSERT_TRUE(narrow >= 40);
}

void test_make_smooth_colormap_sometimes_goes_there_and_back(void) {
  srandom(3);
  int mirrored = 0;
  for (int trial = 0; trial < 200; trial++) {
    XColor colors[64];
    int n = 64;
    Bool writable = False;
    make_smooth_colormap(NULL, NULL, 1, colors, &n, True, &writable, False);
    if (colors[5].pixel == colors[59].pixel && colors[17].pixel == colors[47].pixel &&
        colors[5].pixel != colors[17].pixel)
      mirrored++;
  }
  TEST_ASSERT_TRUE(mirrored >= 30);
  TEST_ASSERT_TRUE(mirrored <= 100);
}

void test_make_color_ramp_runs_from_the_first_colour_to_the_second(void) {
  XColor colors[10];
  memset(colors, 0, sizeof(colors));
  int n = 10;
  make_color_ramp(NULL, NULL, 1, 0, 1.0, 1.0, 120, 1.0, 1.0, colors, &n, False,
                  True, NULL);
  TEST_ASSERT_EQUAL_INT(10, n);
  TEST_ASSERT_TRUE(colors[0].red > 0xF000 && colors[0].green < 0x1000);
  TEST_ASSERT_TRUE(colors[9].green > 0xF000 && colors[9].red < 0x1000);
  TEST_ASSERT_TRUE(colors[5].red > 0x8000 && colors[5].green > 0x8000);
  for (int i = 0; i < n; i++)
    TEST_ASSERT_EQUAL_HEX(rgb565_from16(colors[i].red, colors[i].green, colors[i].blue),
                          colors[i].pixel);
}

void test_make_color_ramp_closed_comes_back_to_the_first_colour(void) {
  XColor colors[12];
  memset(colors, 0, sizeof(colors));
  int n = 12;
  make_color_ramp(NULL, NULL, 1, 0, 1.0, 1.0, 120, 1.0, 1.0, colors, &n, True,
                  True, NULL);
  TEST_ASSERT_EQUAL_INT(12, n);
  TEST_ASSERT_TRUE(colors[0].red > 0xF000 && colors[0].green < 0x1000);
  TEST_ASSERT_TRUE(colors[6].green > 0xF000 && colors[6].red < 0x1000);
  for (int i = 0; i < n; i++) {
    const XColor *a = &colors[i], *b = &colors[(i + 1) % n];
    TEST_ASSERT_TRUE(abs((int)a->red - (int)b->red) < 0x6000);
    TEST_ASSERT_TRUE(abs((int)a->green - (int)b->green) < 0x6000);
  }
}

void test_make_color_ramp_takes_the_short_way_round_the_hue_wheel(void) {
  XColor colors[9];
  memset(colors, 0, sizeof(colors));
  int n = 9;
  make_color_ramp(NULL, NULL, 1, 350, 1.0, 1.0, 10, 1.0, 1.0, colors, &n,
                  False, True, NULL);
  TEST_ASSERT_TRUE(colors[4].red > 0xF000);
  TEST_ASSERT_TRUE(colors[4].green < 0x4000);
  TEST_ASSERT_TRUE(colors[4].blue < 0x4000);
}

void test_make_color_ramp_takes_the_short_way_backwards_too(void) {
  XColor colors[9];
  memset(colors, 0, sizeof(colors));
  int n = 9;
  make_color_ramp(NULL, NULL, 1, 10, 1.0, 1.0, 350, 1.0, 1.0, colors, &n,
                  False, True, NULL);
  TEST_ASSERT_TRUE(colors[4].red > 0xF000);
  TEST_ASSERT_TRUE(colors[4].green < 0x4000);
}

void test_make_color_ramp_blends_saturation_and_value_too(void) {
  XColor colors[9];
  memset(colors, 0, sizeof(colors));
  int n = 9;
  make_color_ramp(NULL, NULL, 1, 0, 1.0, 1.0, 0, 0.0, 0.5, colors, &n, False,
                  True, NULL);
  TEST_ASSERT_TRUE(colors[0].red > 0xF000 && colors[0].green < 0x1000);
  for (int c = 0; c < 3; c++) {
    const unsigned short v = c == 0 ? colors[8].red
                             : c == 1 ? colors[8].green
                                      : colors[8].blue;
    TEST_ASSERT_UINT16_WITHIN(0x0400, 0x8000, v);
  }
  TEST_ASSERT_TRUE(colors[4].green > 0x2000 && colors[4].green < 0x6000);
}

/* Piecewise and Interference cycle through a loop of three anchor colours. */
void test_make_color_loop_passes_each_anchor_in_turn_and_closes(void) {
  XColor colors[30];
  memset(colors, 0, sizeof(colors));
  int n = 30;
  make_color_loop(NULL, NULL, 1, 0, 1.0, 1.0, 120, 1.0, 1.0, 240, 1.0, 1.0,
                  colors, &n, True, NULL);
  TEST_ASSERT_EQUAL_INT(30, n);
  TEST_ASSERT_TRUE(colors[0].red > 0xF000 && colors[0].green < 0x1000 && colors[0].blue < 0x1000);
  TEST_ASSERT_TRUE(colors[10].green > 0xF000 && colors[10].red < 0x1000 && colors[10].blue < 0x1000);
  TEST_ASSERT_TRUE(colors[20].blue > 0xF000 && colors[20].red < 0x1000 && colors[20].green < 0x1000);
  for (int i = 0; i < n; i++) {
    const XColor *a = &colors[i], *b = &colors[(i + 1) % n];
    TEST_ASSERT_TRUE(abs((int)a->red - (int)b->red) < 0x4000);
    TEST_ASSERT_TRUE(abs((int)a->green - (int)b->green) < 0x4000);
    TEST_ASSERT_TRUE(abs((int)a->blue - (int)b->blue) < 0x4000);
    TEST_ASSERT_EQUAL_HEX(rgb565_from16(a->red, a->green, a->blue), a->pixel);
  }
}

void test_make_color_loop_takes_the_short_way_and_blends_value(void) {
  XColor colors[6];
  memset(colors, 0, sizeof(colors));
  int n = 6;
  make_color_loop(NULL, NULL, 1, 350, 1.0, 1.0, 10, 1.0, 0.5, 350, 1.0, 1.0,
                  colors, &n, True, NULL);
  for (int i = 0; i < n; i++) {
    TEST_ASSERT_TRUE(colors[i].green < 0x3000);
    TEST_ASSERT_TRUE(colors[i].blue < 0x3000);
  }
  TEST_ASSERT_UINT16_WITHIN(0x0800, 0x8000, colors[2].red);
  TEST_ASSERT_TRUE(colors[1].red > 0x9000 && colors[1].red < 0xF000);
}

void test_a_gc_remembers_its_function_but_draws_copy(void) {
  XGCValues v = {0};
  GC plain = XCreateGC(dpy, win, 0, &v);
  TEST_ASSERT_EQUAL_INT(GXcopy, plain->function);
  v.function = GXor;
  GC ored = XCreateGC(dpy, win, GCFunction, &v);
  TEST_ASSERT_EQUAL_INT(GXor, ored->function);
  XSetFunction(dpy, ored, GXxor);
  TEST_ASSERT_EQUAL_INT(GXxor, ored->function);
  v.function = GXcopy;
  XChangeGC(dpy, ored, GCFunction, &v);
  TEST_ASSERT_EQUAL_INT(GXcopy, ored->function);
  XSetFunction(dpy, ored, GXor);
  XSetForeground(dpy, ored, 0x1234);
  XFillRectangle(dpy, win, ored, 2, 2, 2, 2);
  TEST_ASSERT_EQUAL_HEX(0x1234, at(2, 2));
  XFreeGC(dpy, plain);
  XFreeGC(dpy, ored);
}

/* No expose events ever arrive, so a hack's choice about them changes
 * nothing it draws (Kumppa turns them off on its copying GC). */
void test_set_graphics_exposures_is_accepted_and_draws_nothing(void) {
  XGCValues v = {0};
  GC gc = XCreateGC(dpy, win, 0, &v);
  TEST_ASSERT_EQUAL_INT(0, XSetGraphicsExposures(dpy, gc, False));
  TEST_ASSERT_EQUAL_INT(0, count_set());
  XFreeGC(dpy, gc);
}

/* No images to load: Boxfit's optional grab mode finds loading done at once. */
void test_load_image_async_simple_finishes_at_once_with_nothing(void) {
  xshim_set_grab_image(NULL, 0);
  char *name = (char *)"stale";
  XRectangle geom = {1, 2, 3, 4};
  TEST_ASSERT_NULL(load_image_async_simple(NULL, NULL, win, win, &name, &geom));
  TEST_ASSERT_NULL(name);
  TEST_ASSERT_EQUAL_INT(0, geom.x);
  TEST_ASSERT_EQUAL_INT(0, geom.y);
  TEST_ASSERT_EQUAL_INT(0, geom.width);
  TEST_ASSERT_EQUAL_INT(0, geom.height);
  TEST_ASSERT_NULL(load_image_async_simple(NULL, NULL, win, win, NULL, NULL));
  TEST_ASSERT_EQUAL_INT(0, count_set());
}

/* Interference splits its rows over a thread pool. Here the pool runs its
 * "threads" one after another on the caller, as upstream does without
 * pthreads. */
struct pool_owner {
  int before;
  struct threadpool pool;
};

struct pool_thread {
  unsigned id;
  struct pool_owner *owner;
  int *log;
};

static int g_pool_log[16], g_pool_log_n, g_pool_fail_id = -1;

static int pool_thread_create(void *self_raw, struct threadpool *pool, unsigned id) {
  struct pool_thread *self = (struct pool_thread *)self_raw;
  if ((int)id == g_pool_fail_id) return 12;
  self->id = id;
  self->owner = GET_PARENT_OBJ(struct pool_owner, pool, pool);
  g_pool_log[g_pool_log_n++] = 100 + (int)id;
  return 0;
}

static void pool_thread_destroy(void *self_raw) {
  g_pool_log[g_pool_log_n++] = 200 + (int)((struct pool_thread *)self_raw)->id;
}

static void pool_thread_run(void *self_raw) {
  struct pool_thread *self = (struct pool_thread *)self_raw;
  g_pool_log[g_pool_log_n++] = 300 + (int)self->id + self->owner->before;
}

static const struct threadpool_class kPoolClass = {sizeof(struct pool_thread),
                                                   pool_thread_create,
                                                   pool_thread_destroy};

void test_threadpool_runs_each_thread_in_turn_on_the_caller(void) {
  struct pool_owner owner = {.before = 10};
  g_pool_log_n = 0;
  g_pool_fail_id = -1;
  TEST_ASSERT_EQUAL_INT(0, threadpool_create(&owner.pool, &kPoolClass, dpy, 3));
  TEST_ASSERT_EQUAL_UINT(3, owner.pool.count);
  threadpool_run(&owner.pool, pool_thread_run);
  threadpool_wait(&owner.pool);
  threadpool_run(&owner.pool, pool_thread_run);
  threadpool_wait(&owner.pool);
  threadpool_destroy(&owner.pool);
  const int want[] = {100, 101, 102, 310, 311, 312, 310, 311, 312, 200, 201, 202};
  TEST_ASSERT_EQUAL_INT(12, g_pool_log_n);
  TEST_ASSERT_EQUAL_INT_ARRAY(want, g_pool_log, 12);
}

void test_threadpool_create_failure_undoes_the_threads_it_made(void) {
  struct pool_owner owner = {0};
  g_pool_log_n = 0;
  g_pool_fail_id = 2;
  TEST_ASSERT_EQUAL_INT(12, threadpool_create(&owner.pool, &kPoolClass, dpy, 4));
  g_pool_fail_id = -1;
  const int want[] = {100, 101, 200, 201};
  TEST_ASSERT_EQUAL_INT(4, g_pool_log_n);
  TEST_ASSERT_EQUAL_INT_ARRAY(want, g_pool_log, 4);
}

void test_one_core_and_a_pointer_sized_alignment_without_threads(void) {
  TEST_ASSERT_EQUAL_UINT(1, hardware_concurrency(dpy));
  TEST_ASSERT_EQUAL_UINT(sizeof(void *), thread_memory_alignment(dpy));
  TEST_ASSERT_EQUAL_INT(16, visual_pixmap_depth(NULL, NULL));
}

void test_the_arrow_and_page_keysyms_have_x_values(void) {
  TEST_ASSERT_EQUAL_HEX(0xFF51, XK_Left);
  TEST_ASSERT_EQUAL_HEX(0xFF52, XK_Up);
  TEST_ASSERT_EQUAL_HEX(0xFF53, XK_Right);
  TEST_ASSERT_EQUAL_HEX(0xFF54, XK_Down);
  TEST_ASSERT_EQUAL_HEX(0xFF55, XK_Prior);
  TEST_ASSERT_EQUAL_HEX(0xFF56, XK_Next);
}

void test_visual_depth_is_sixteen(void) {
  TEST_ASSERT_EQUAL_INT(16, visual_depth(NULL, NULL));
}

void test_xshm_image_is_a_16_bit_image_put_where_asked(void) {
  XShmSegmentInfo shm;
  XImage *im = create_xshm_image(dpy, NULL, 16, ZPixmap, &shm, 8, 2);
  TEST_ASSERT_NOT_NULL(im);
  TEST_ASSERT_EQUAL_INT(8, im->width);
  TEST_ASSERT_EQUAL_INT(2, im->height);
  TEST_ASSERT_EQUAL_INT(16, im->bits_per_pixel);
  for (int y = 0; y < 2; y++)
    for (int x = 0; x < 8; x++) TEST_ASSERT_EQUAL_HEX64(0, XGetPixel(im, x, y));
  for (int x = 0; x < 8; x++) XPutPixel(im, x, 1, 0x1234 + x);
  XGCValues v = {0};
  GC gc = XCreateGC(dpy, win, 0, &v);
  put_xshm_image(dpy, win, gc, im, 0, 1, 3, 5, 8, 1, &shm);
  TEST_ASSERT_EQUAL_HEX(0x1234, at(3, 5));
  TEST_ASSERT_EQUAL_HEX(0x123B, at(10, 5));
  TEST_ASSERT_EQUAL_HEX(0, at(2, 5));
  TEST_ASSERT_EQUAL_HEX(0, at(3, 4));
  XFreeGC(dpy, gc);
  destroy_xshm_image(dpy, im, &shm);
}

void test_xshm_image_taller_than_the_window_edge_is_clipped(void) {
  XShmSegmentInfo shm;
  XImage *im = create_xshm_image(dpy, NULL, 16, ZPixmap, &shm, 4, 4);
  for (int y = 0; y < 4; y++)
    for (int x = 0; x < 4; x++) XPutPixel(im, x, y, 0x0800 + y);
  XGCValues v = {0};
  GC gc = XCreateGC(dpy, win, 0, &v);
  put_xshm_image(dpy, win, gc, im, 0, 0, 0, 14, 4, 4, &shm);
  TEST_ASSERT_EQUAL_HEX(0x0800, at(0, 14));
  TEST_ASSERT_EQUAL_HEX(0x0801, at(3, 15));
  XFreeGC(dpy, gc);
  destroy_xshm_image(dpy, im, &shm);
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

void test_ya_random_follows_the_seeded_sequence_and_is_not_constant(void) {
  ya_rand_init(42);
  unsigned long a = ya_random();
  unsigned long b = ya_random();
  ya_rand_init(42);
  TEST_ASSERT_EQUAL_UINT64(a, ya_random());
  TEST_ASSERT_EQUAL_UINT64(b, ya_random());
  TEST_ASSERT_NOT_EQUAL(a, b);
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

void test_gettext_and_underscore_return_their_argument(void) {
  const char *s = "Primary";
  TEST_ASSERT_EQUAL_PTR(s, gettext(s));
  TEST_ASSERT_EQUAL_PTR(s, _(s));
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

/* A 4 by 3 image. Pixel (x, y) is 0x1000 + 4y + x, so every pixel is non-zero
 * and distinct. The mask is row 0 all set, row 1 columns 0 and 2, row 2 none:
 * 6 set bits. */
#define BLOB_W 4
#define BLOB_H 3
#define MASK_SET_BITS 6
static unsigned char blob[8 + 2 * BLOB_W * BLOB_H + BLOB_H];

static void fill_blob(void) {
  memcpy(blob, "565M", 4);
  blob[4] = BLOB_W;
  blob[5] = 0;
  blob[6] = BLOB_H;
  blob[7] = 0;
  for (int i = 0; i < BLOB_W * BLOB_H; i++) {
    blob[8 + 2 * i] = (0x1000 + i) & 0xFF;
    blob[9 + 2 * i] = (0x1000 + i) >> 8;
  }
  blob[8 + 2 * BLOB_W * BLOB_H + 0] = 0xF0;
  blob[8 + 2 * BLOB_W * BLOB_H + 1] = 0xA0;
  blob[8 + 2 * BLOB_W * BLOB_H + 2] = 0x00;
}

static Pixmap load_image(Pixmap *mask) {
  int w, h;
  fill_blob();
  return image_data_to_pixmap(dpy, win, blob, sizeof(blob), &w, &h, mask);
}

/* GL hacks load textures as RGBA bytes, upside down (row 0 is the bottom), as
 * upstream's image_data_to_ximage gives them. Alpha comes from the mask. */
static const unsigned char *rgba_at(const XImage *im, int x, int y) {
  return (const unsigned char *)im->data + (size_t)y * (size_t)im->bytes_per_line + 4 * (size_t)x;
}

void test_image_data_to_ximage_gives_rgba_bytes_bottom_row_first(void) {
  fill_blob();
  XImage *im = image_data_to_ximage(dpy, NULL, blob, sizeof(blob));
  TEST_ASSERT_NOT_NULL(im);
  TEST_ASSERT_EQUAL_INT(BLOB_W, im->width);
  TEST_ASSERT_EQUAL_INT(BLOB_H, im->height);
  TEST_ASSERT_EQUAL_INT(32, im->bits_per_pixel);
  TEST_ASSERT_EQUAL_INT(4 * BLOB_W, im->bytes_per_line);
  /* Source (1, 2) is 0x1009: red 2, green 0, blue 9; its mask row is clear. */
  const unsigned char want_a[4] = {0x10, 0x00, 0x4A, 0x00};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(want_a, rgba_at(im, 1, 0), 4);
  /* Source (0, 0) is 0x1000 and masked in. */
  const unsigned char want_b[4] = {0x10, 0x00, 0x00, 0xFF};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(want_b, rgba_at(im, 0, 2), 4);
  /* Source (2, 1): 0x1006, green 0, mask row 1 has column 2. */
  const unsigned char want_c[4] = {0x10, 0x00, 0x31, 0xFF};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(want_c, rgba_at(im, 2, 1), 4);
  /* Source (1, 1): mask clear. */
  TEST_ASSERT_EQUAL_HEX8(0x00, rgba_at(im, 1, 1)[3]);
  XDestroyImage(im);
  /* Green has six bits: 32 of 63 is 0x82. */
  blob[8 + 2 * 3] = 0x00;
  blob[9 + 2 * 3] = 0x04;
  im = image_data_to_ximage(dpy, NULL, blob, sizeof(blob));
  const unsigned char want_d[4] = {0x00, 0x82, 0x00, 0xFF};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(want_d, rgba_at(im, 3, 2), 4);
  XDestroyImage(im);
}

void test_image_data_to_ximage_refuses_a_bad_blob(void) {
  fill_blob();
  TEST_ASSERT_NULL(image_data_to_ximage(dpy, NULL, blob, sizeof(blob) - 1));
  blob[0] = 'X';
  TEST_ASSERT_NULL(image_data_to_ximage(dpy, NULL, blob, sizeof(blob)));
}

static GC new_gc(unsigned long fg, unsigned long bg) {
  GC gc = XCreateGC(dpy, win, 0, NULL);
  XSetForeground(dpy, gc, fg);
  XSetBackground(dpy, gc, bg);
  return gc;
}

void test_image_data_to_pixmap_makes_a_colour_pixmap_and_a_mask(void) {
  int w = 99, h = 99;
  Pixmap mask = None;
  fill_blob();
  Pixmap p = image_data_to_pixmap(dpy, win, blob, sizeof(blob), &w, &h, &mask);
  TEST_ASSERT_NOT_EQUAL(None, p);
  TEST_ASSERT_NOT_EQUAL(None, mask);
  TEST_ASSERT_EQUAL_INT(BLOB_W, w);
  TEST_ASSERT_EQUAL_INT(BLOB_H, h);

  Window root;
  int x, y;
  unsigned int pw, ph, border, depth;
  TEST_ASSERT_TRUE(XGetGeometry(dpy, p, &root, &x, &y, &pw, &ph, &border, &depth));
  TEST_ASSERT_EQUAL_UINT(BLOB_W, pw);
  TEST_ASSERT_EQUAL_UINT(BLOB_H, ph);
  TEST_ASSERT_EQUAL_UINT(16, depth);
  TEST_ASSERT_TRUE(XGetGeometry(dpy, mask, &root, &x, &y, &pw, &ph, &border, &depth));
  TEST_ASSERT_EQUAL_UINT(BLOB_W, pw);
  TEST_ASSERT_EQUAL_UINT(1, depth);
  XFreePixmap(dpy, p);
  XFreePixmap(dpy, mask);
}

void test_image_data_to_pixmap_without_a_mask_pointer_still_loads_the_colours(void) {
  int w = 99, h = 99;
  fill_blob();
  Pixmap p = image_data_to_pixmap(dpy, win, blob, sizeof(blob), &w, &h, NULL);
  TEST_ASSERT_NOT_EQUAL(None, p);
  TEST_ASSERT_EQUAL_INT(BLOB_W, w);
  TEST_ASSERT_EQUAL_INT(BLOB_H, h);
  XImage *im = XGetImage(dpy, p, 0, 0, BLOB_W, BLOB_H, ~0UL, ZPixmap);
  TEST_ASSERT_NOT_NULL(im);
  XDestroyImage(im);
  XFreePixmap(dpy, p);
}

void test_image_data_to_pixmap_rejects_a_blob_that_is_not_ours(void) {
  fill_blob();
  unsigned char bad_magic[sizeof(blob)];
  memcpy(bad_magic, blob, sizeof(blob));
  bad_magic[0] = 0x89; /* a PNG signature starts like this */
  struct {
    const unsigned char *data;
    unsigned long size;
  } cases[] = {{bad_magic, sizeof(blob)},
               {blob, sizeof(blob) - 1},
               {blob, sizeof(blob) + 1},
               {blob, 7}};
  for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    int w = 99, h = 99;
    Pixmap mask = 12345;
    Pixmap p = image_data_to_pixmap(dpy, win, cases[i].data, cases[i].size, &w, &h, &mask);
    TEST_ASSERT_EQUAL(None, p);
    TEST_ASSERT_EQUAL(None, mask);
    TEST_ASSERT_EQUAL_INT(0, w);
    TEST_ASSERT_EQUAL_INT(0, h);
  }
}

/* The "565Z" form of fill_blob's image: its header, then `body_len` bytes of
 * its pixels and mask (fewer or more than the image needs, for the failure
 * cases) as one zlib stream. Returns the blob's size. */
static unsigned long deflate_blob(unsigned char *out, unsigned long cap, unsigned long body_len) {
  unsigned char body[sizeof(blob) + 1] = {0};
  fill_blob();
  memcpy(body, blob + 8, sizeof(blob) - 8);
  memcpy(out, "565Z", 4);
  memcpy(out + 4, blob + 4, 4);
  uLongf len = cap - 8;
  TEST_ASSERT_EQUAL_INT(Z_OK, compress2(out + 8, &len, body, body_len, 9));
  return 8 + len;
}

void test_a_deflated_blob_loads_the_same_pixels_and_mask_as_the_plain_one(void) {
  unsigned char packed[256];
  const unsigned long size = deflate_blob(packed, sizeof(packed), sizeof(blob) - 8);
  fill_blob();

  XImage *want = image_data_to_ximage(dpy, NULL, blob, sizeof(blob));
  XImage *got = image_data_to_ximage(dpy, NULL, packed, size);
  TEST_ASSERT_NOT_NULL(got);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(want->data, got->data, 4 * BLOB_W * BLOB_H);
  XDestroyImage(want);
  XDestroyImage(got);

  int w = 99, h = 99;
  Pixmap mask = None;
  Pixmap p = image_data_to_pixmap(dpy, win, packed, size, &w, &h, &mask);
  TEST_ASSERT_NOT_EQUAL(None, p);
  TEST_ASSERT_NOT_EQUAL(None, mask);
  TEST_ASSERT_EQUAL_INT(BLOB_W, w);
  TEST_ASSERT_EQUAL_INT(BLOB_H, h);
  XImage *im = XGetImage(dpy, p, 0, 0, BLOB_W, BLOB_H, ~0UL, ZPixmap);
  for (int y = 0; y < BLOB_H; y++)
    for (int x = 0; x < BLOB_W; x++)
      TEST_ASSERT_EQUAL_HEX16(px_swap((uint16_t)(0x1000 + BLOB_W * y + x)),
                              XGetPixel(im, x, y));
  XDestroyImage(im);
  /* Copied through the mask, only its 6 set pixels land on a clear window. */
  XGCValues gcv = {0};
  GC gc = XCreateGC(dpy, win, 0, &gcv);
  XSetClipMask(dpy, gc, mask);
  XCopyArea(dpy, p, win, gc, 0, 0, BLOB_W, BLOB_H, 0, 0);
  TEST_ASSERT_EQUAL_INT(MASK_SET_BITS, count_set());
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, p);
  XFreePixmap(dpy, mask);
}

void test_a_deflated_blob_that_is_corrupt_or_the_wrong_size_is_rejected(void) {
  unsigned char shorter[256], longer[256], corrupt[256];
  const unsigned long body = sizeof(blob) - 8;
  const unsigned long n_short = deflate_blob(shorter, sizeof(shorter), body - 1);
  const unsigned long n_long = deflate_blob(longer, sizeof(longer), body + 1);
  const unsigned long n_corrupt = deflate_blob(corrupt, sizeof(corrupt), body);
  corrupt[n_corrupt / 2] ^= 0xFF;
  struct {
    const unsigned char *data;
    unsigned long size;
  } cases[] = {{shorter, n_short}, {longer, n_long}, {corrupt, n_corrupt},
               {longer, 8}};
  for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    int w = 99, h = 99;
    Pixmap mask = 12345;
    TEST_ASSERT_EQUAL(None, image_data_to_pixmap(dpy, win, cases[i].data, cases[i].size,
                                                 &w, &h, &mask));
    TEST_ASSERT_EQUAL(None, mask);
    TEST_ASSERT_EQUAL_INT(0, w);
    TEST_ASSERT_NULL(image_data_to_ximage(dpy, NULL, cases[i].data, cases[i].size));
  }
  /* A stream cut short before its end. */
  const unsigned long n_whole = deflate_blob(corrupt, sizeof(corrupt), body);
  TEST_ASSERT_NULL(image_data_to_ximage(dpy, NULL, corrupt, n_whole - 5));
}

/* A "565Z" blob of a black w by h image, malloc'd; its size goes in *size. */
static unsigned char *black_deflated_blob(unsigned w, unsigned h, unsigned long *size) {
  const unsigned long body_len = 2UL * w * h + (unsigned long)((w + 7) / 8) * h;
  unsigned char *body = (unsigned char *)calloc(1, body_len);
  uLongf len = compressBound(body_len);
  unsigned char *out = (unsigned char *)malloc(8 + len);
  TEST_ASSERT_EQUAL_INT(Z_OK, compress2(out + 8, &len, body, body_len, 9));
  free(body);
  memcpy(out, "565Z", 4);
  out[4] = w & 0xFF, out[5] = w >> 8, out[6] = h & 0xFF, out[7] = h >> 8;
  *size = 8 + len;
  return out;
}

/* Past INT16_MAX a side is more than the canvas allows, and on the device a
 * 65535 by 65535 blob's size would wrap the 32-bit size_t passed to malloc. */
void test_a_blob_wider_or_taller_than_int16_max_is_rejected(void) {
  const unsigned sizes[][2] = {{32768, 1}, {1, 32768}, {32767, 1}, {1, 32767}};
  for (unsigned i = 0; i < 4; i++) {
    unsigned long size;
    unsigned char *b = black_deflated_blob(sizes[i][0], sizes[i][1], &size);
    XImage *im = image_data_to_ximage(dpy, NULL, b, size);
    if (i < 2) {
      TEST_ASSERT_NULL(im);
    } else {
      TEST_ASSERT_NOT_NULL(im);
      XDestroyImage(im);
    }
    free(b);
  }
}

void test_get_geometry_of_the_window_is_the_canvas_and_of_nothing_is_failure(void) {
  Window root;
  int x, y;
  unsigned int w, h, border, depth;
  TEST_ASSERT_TRUE(XGetGeometry(dpy, win, &root, &x, &y, &w, &h, &border, &depth));
  TEST_ASSERT_EQUAL_UINT(16, w);
  TEST_ASSERT_EQUAL_UINT(16, h);
  TEST_ASSERT_EQUAL_UINT(16, depth);
  TEST_ASSERT_FALSE(XGetGeometry(dpy, None, &root, &x, &y, &w, &h, &border, &depth));
}

/* With a picture set, loading paints it over the whole target and still
 * finishes at once. The picture is scaled to cover the target and cropped
 * evenly at the sides, so it keeps its shape. blob is the 4 by 3 image from
 * fill_blob: on the 16 by 16 window each pixel is 5.33 wide and the picture
 * 21.33 across, so the left and right 2.67 columns are cropped. */
void test_load_image_async_simple_paints_the_grab_image_covering_the_target(void) {
  fill_blob();
  xshim_set_grab_image(blob, sizeof(blob));
  TEST_ASSERT_NULL(load_image_async_simple(NULL, NULL, win, win, NULL, NULL));
  TEST_ASSERT_EQUAL_INT(256, count_set());
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x1000), at(0, 0));
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x1003), at(15, 0));
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x1008), at(0, 15));
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x100B), at(15, 15));
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x1006), at(8, 6));
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x1000), at(2, 0));
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x1001), at(3, 0));
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x1001), at(7, 4));
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x1005), at(7, 5));
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x1002), at(12, 0));
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x1003), at(13, 0));
  xshim_set_grab_image(NULL, 0);
}

/* A 3 wide, 4 high picture is cropped top and bottom instead. Pixel (x, y) is
 * 0x2000 + 3y + x. */
void test_load_image_async_simple_crops_a_tall_grab_image_top_and_bottom(void) {
  static unsigned char tall[8 + 2 * 3 * 4 + 4];
  memset(tall, 0, sizeof(tall));
  memcpy(tall, "565M", 4);
  tall[4] = 3;
  tall[6] = 4;
  for (int i = 0; i < 12; i++) {
    tall[8 + 2 * i] = (0x2000 + i) & 0xFF;
    tall[9 + 2 * i] = (0x2000 + i) >> 8;
  }
  xshim_set_grab_image(tall, sizeof(tall));
  TEST_ASSERT_NULL(load_image_async_simple(NULL, NULL, win, win, NULL, NULL));
  TEST_ASSERT_EQUAL_INT(256, count_set());
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x2000), at(0, 0));
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x2000), at(0, 2));
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x2003), at(0, 3));
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x2008), at(15, 12));
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x200B), at(15, 13));
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x200B), at(15, 15));
  xshim_set_grab_image(NULL, 0);
}

void test_load_image_async_simple_paints_a_pixmap_target_and_not_the_window(void) {
  fill_blob();
  xshim_set_grab_image(blob, sizeof(blob));
  Pixmap target = XCreatePixmap(dpy, win, 8, 6, 16);
  TEST_ASSERT_NULL(load_image_async_simple(NULL, NULL, win, target, NULL, NULL));
  TEST_ASSERT_EQUAL_INT(0, count_set());
  GC gc = new_gc(0xFFFF, 0);
  XCopyArea(dpy, target, win, gc, 0, 0, 8, 6, 0, 0);
  TEST_ASSERT_EQUAL_INT(48, count_set());
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x1000), at(0, 0));
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x100B), at(7, 5));
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, target);
  xshim_set_grab_image(NULL, 0);
}

void test_load_image_async_simple_ignores_a_grab_image_it_cannot_read(void) {
  fill_blob();
  xshim_set_grab_image(blob, sizeof(blob) - 1);
  TEST_ASSERT_NULL(load_image_async_simple(NULL, NULL, win, win, NULL, NULL));
  TEST_ASSERT_EQUAL_INT(0, count_set());
  xshim_set_grab_image(NULL, 0);
}

void test_a_load_already_under_way_does_not_paint_again(void) {
  fill_blob();
  xshim_set_grab_image(blob, sizeof(blob));
  async_load_state *under_way = (async_load_state *)(uintptr_t)1;
  TEST_ASSERT_NULL(load_image_async_simple(under_way, NULL, win, win, NULL, NULL));
  TEST_ASSERT_EQUAL_INT(0, count_set());
  xshim_set_grab_image(NULL, 0);
}

void test_loading_a_grab_image_leaves_no_memory_behind(void) {
  fill_blob();
  xshim_set_grab_image(blob, sizeof(blob));
  load_image_async_simple(NULL, NULL, win, win, NULL, NULL);
  const size_t before = __sanitizer_get_current_allocated_bytes();
  for (int i = 0; i < 20; i++) load_image_async_simple(NULL, NULL, win, win, NULL, NULL);
  TEST_ASSERT_EQUAL_UINT64(before, __sanitizer_get_current_allocated_bytes());
  xshim_set_grab_image(NULL, 0);
}

void test_copy_area_draws_the_image_at_the_destination(void) {
  Pixmap mask = None;
  Pixmap p = load_image(&mask);
  GC gc = new_gc(0xFFFF, 0);
  XCopyArea(dpy, p, win, gc, 0, 0, BLOB_W, BLOB_H, 5, 6);
  TEST_ASSERT_EQUAL_INT(BLOB_W * BLOB_H, count_set());
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x1000), at(5, 6));
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x100B), at(8, 8));
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, p);
  XFreePixmap(dpy, mask);
}

void test_copy_area_copies_only_the_requested_source_rectangle(void) {
  Pixmap mask = None;
  Pixmap p = load_image(&mask);
  GC gc = new_gc(0xFFFF, 0);
  XCopyArea(dpy, p, win, gc, 1, 1, 2, 2, 0, 0);
  TEST_ASSERT_EQUAL_INT(4, count_set());
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x1005), at(0, 0));
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x100A), at(1, 1));
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, p);
  XFreePixmap(dpy, mask);
}

void test_copy_area_is_clipped_at_every_canvas_edge(void) {
  Pixmap mask = None;
  Pixmap p = load_image(&mask);
  GC gc = new_gc(0xFFFF, 0);
  XCopyArea(dpy, p, win, gc, 0, 0, BLOB_W, BLOB_H, -2, -1);
  TEST_ASSERT_EQUAL_INT(2 * 2, count_set());
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x1006), at(0, 0));
  XClearWindow(dpy, win);
  XCopyArea(dpy, p, win, gc, 0, 0, BLOB_W, BLOB_H, 14, 14);
  TEST_ASSERT_EQUAL_INT(2 * 2, count_set());
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x1005), at(15, 15));
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, p);
  XFreePixmap(dpy, mask);
}

void test_copy_area_is_clipped_to_the_source_image(void) {
  Pixmap mask = None;
  Pixmap p = load_image(&mask);
  GC gc = new_gc(0xFFFF, 0);
  XCopyArea(dpy, p, win, gc, 2, 1, 10, 10, 0, 0);
  TEST_ASSERT_EQUAL_INT(2 * 2, count_set());
  XCopyArea(dpy, p, win, gc, -3, -3, 5, 5, 8, 8);
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x1000), at(11, 11));
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, p);
  XFreePixmap(dpy, mask);
}

void test_clip_mask_draws_only_where_the_mask_is_set(void) {
  Pixmap mask = None;
  Pixmap p = load_image(&mask);
  GC gc = new_gc(0xFFFF, 0);
  XSetClipMask(dpy, gc, mask);
  XSetClipOrigin(dpy, gc, 5, 6);
  XCopyArea(dpy, p, win, gc, 0, 0, BLOB_W, BLOB_H, 5, 6);
  TEST_ASSERT_EQUAL_INT(MASK_SET_BITS, count_set());
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x1000), at(5, 6));
  TEST_ASSERT_EQUAL_HEX16(0, at(6, 7));
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x1006), at(7, 7));
  TEST_ASSERT_EQUAL_HEX16(0, at(5, 8));
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, p);
  XFreePixmap(dpy, mask);
}

void test_clip_origin_positions_the_mask_and_outside_the_mask_is_not_drawn(void) {
  Pixmap mask = None;
  Pixmap p = load_image(&mask);
  GC gc = new_gc(0xFFFF, 0);
  XSetClipMask(dpy, gc, mask);
  XSetClipOrigin(dpy, gc, 6, 6);
  XCopyArea(dpy, p, win, gc, 0, 0, BLOB_W, BLOB_H, 5, 6);
  TEST_ASSERT_EQUAL_INT(5, count_set());
  TEST_ASSERT_EQUAL_HEX16(0, at(5, 6));
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x1001), at(6, 6));
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x1007), at(8, 7));
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, p);
  XFreePixmap(dpy, mask);
}

void test_clip_mask_outlives_the_pixmap_it_was_set_from(void) {
  Pixmap mask = None;
  Pixmap p = load_image(&mask);
  GC gc = new_gc(0xFFFF, 0);
  XSetClipMask(dpy, gc, mask);
  XFreePixmap(dpy, mask);
  XCopyArea(dpy, p, win, gc, 0, 0, BLOB_W, BLOB_H, 0, 0);
  TEST_ASSERT_EQUAL_INT(MASK_SET_BITS, count_set());
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, p);
}

void test_clip_mask_none_removes_the_clip(void) {
  Pixmap mask = None;
  Pixmap p = load_image(&mask);
  GC gc = new_gc(0xFFFF, 0);
  XSetClipMask(dpy, gc, mask);
  XSetClipMask(dpy, gc, None);
  XCopyArea(dpy, p, win, gc, 0, 0, BLOB_W, BLOB_H, 0, 0);
  TEST_ASSERT_EQUAL_INT(BLOB_W * BLOB_H, count_set());
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, p);
  XFreePixmap(dpy, mask);
}

void test_copy_plane_draws_set_bits_in_foreground_and_clear_bits_in_background(void) {
  Pixmap mask = None;
  Pixmap p = load_image(&mask);
  GC gc = new_gc(0xF800, 0x001F);
  XCopyPlane(dpy, mask, win, gc, 0, 0, BLOB_W, BLOB_H, 0, 0, 1);
  TEST_ASSERT_EQUAL_INT(BLOB_W * BLOB_H, count_set());
  TEST_ASSERT_EQUAL_HEX16(0xF800, at(0, 0));
  TEST_ASSERT_EQUAL_HEX16(0xF800, at(3, 0));
  TEST_ASSERT_EQUAL_HEX16(0xF800, at(0, 1));
  TEST_ASSERT_EQUAL_HEX16(0x001F, at(1, 1));
  TEST_ASSERT_EQUAL_HEX16(0xF800, at(2, 1));
  TEST_ASSERT_EQUAL_HEX16(0x001F, at(3, 1));
  TEST_ASSERT_EQUAL_HEX16(0x001F, at(0, 2));
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, p);
  XFreePixmap(dpy, mask);
}

void test_a_gc_made_or_changed_with_a_background_in_its_mask_keeps_it(void) {
  Pixmap bits = XCreatePixmap(dpy, win, 2, 1, 1);
  XGCValues v = {0};
  v.foreground = 0xF800;
  v.background = 0x5678;
  GC gc = XCreateGC(dpy, win, GCForeground | GCBackground, &v);
  XCopyPlane(dpy, bits, win, gc, 0, 0, 2, 1, 0, 0, 1);
  TEST_ASSERT_EQUAL_HEX16(0x5678, at(0, 0));
  v.background = 0x0123;
  XChangeGC(dpy, gc, GCBackground, &v);
  XCopyPlane(dpy, bits, win, gc, 0, 0, 2, 1, 0, 0, 1);
  TEST_ASSERT_EQUAL_HEX16(0x0123, at(1, 0));
  v.background = 0x7777;
  XChangeGC(dpy, gc, GCForeground, &v);
  XCopyPlane(dpy, bits, win, gc, 0, 0, 2, 1, 0, 0, 1);
  TEST_ASSERT_EQUAL_HEX16(0x0123, at(0, 0));
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, bits);
}

void test_copy_plane_honours_the_clip_mask(void) {
  Pixmap mask = None;
  Pixmap p = load_image(&mask);
  GC gc = new_gc(0xF800, 0x001F);
  XSetClipMask(dpy, gc, mask);
  XCopyPlane(dpy, mask, win, gc, 0, 0, BLOB_W, BLOB_H, 0, 0, 1);
  TEST_ASSERT_EQUAL_INT(MASK_SET_BITS, count_set());
  TEST_ASSERT_EQUAL_HEX16(0, at(1, 1));
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, p);
  XFreePixmap(dpy, mask);
}

void test_copy_with_the_wrong_kind_of_source_draws_nothing(void) {
  Pixmap mask = None;
  Pixmap p = load_image(&mask);
  GC gc = new_gc(0xF800, 0x001F);
  XCopyArea(dpy, mask, win, gc, 0, 0, BLOB_W, BLOB_H, 0, 0);
  XCopyPlane(dpy, p, win, gc, 0, 0, BLOB_W, BLOB_H, 0, 0, 1);
  XCopyPlane(dpy, mask, win, gc, 0, 0, BLOB_W, BLOB_H, 0, 0, 2);
  XCopyArea(dpy, None, win, gc, 0, 0, BLOB_W, BLOB_H, 0, 0);
  TEST_ASSERT_EQUAL_INT(0, count_set());
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, p);
  XFreePixmap(dpy, mask);
}

void test_pixmaps_and_clip_masks_leave_no_memory_behind(void) {
  const size_t before = __sanitizer_get_current_allocated_bytes();
  for (int i = 0; i < 20; i++) {
    Pixmap mask = None;
    Pixmap p = load_image(&mask);
    GC gc = new_gc(0xFFFF, 0);
    XSetClipMask(dpy, gc, mask);
    XSetClipMask(dpy, gc, mask); /* replacing a clip must free the old one */
    XCopyArea(dpy, p, win, gc, 0, 0, BLOB_W, BLOB_H, 0, 0);
    XFreePixmap(dpy, mask);
    XFreePixmap(dpy, p);
    XFreeGC(dpy, gc); /* still holding a clip */
  }
  TEST_ASSERT_EQUAL_UINT64(before, __sanitizer_get_current_allocated_bytes());
}

void test_free_pixmap_of_none_and_sync_are_harmless(void) {
  TEST_ASSERT_EQUAL_INT(0, XFreePixmap(dpy, None));
  TEST_ASSERT_EQUAL_INT(0, XSync(dpy, False));
}

void test_clear_area_clears_only_its_rectangle_to_the_background(void) {
  XGCValues v;
  v.foreground = 0xFFFF;
  GC gc = XCreateGC(dpy, win, GCForeground, &v);
  XFillRectangle(dpy, win, gc, 0, 0, 16, 16);
  XClearArea(dpy, win, 4, 5, 3, 2, False);
  TEST_ASSERT_EQUAL_INT(256 - 6, count_set());
  TEST_ASSERT_EQUAL_HEX16(0, at(4, 5));
  TEST_ASSERT_EQUAL_HEX16(0, at(6, 6));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(7, 5));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(4, 7));
  XSetWindowBackground(dpy, win, 0x07E0);
  XClearArea(dpy, win, 0, 0, 1, 1, False);
  TEST_ASSERT_EQUAL_HEX16(0x07E0, at(0, 0));
  XSetWindowBackground(dpy, win, 0);
  XFreeGC(dpy, gc);
}

void test_clear_area_of_zero_size_extends_to_the_window_edge(void) {
  XGCValues v;
  v.foreground = 0xFFFF;
  GC gc = XCreateGC(dpy, win, GCForeground, &v);
  XFillRectangle(dpy, win, gc, 0, 0, 16, 16);
  XClearArea(dpy, win, 10, 12, 0, 0, False);
  TEST_ASSERT_EQUAL_INT(256 - 6 * 4, count_set());
  TEST_ASSERT_EQUAL_HEX16(0, at(15, 15));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(9, 12));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(10, 11));
  XFreeGC(dpy, gc);
}

void test_clear_area_clips_a_negative_origin_and_a_huge_size(void) {
  XGCValues v;
  v.foreground = 0xFFFF;
  GC gc = XCreateGC(dpy, win, GCForeground, &v);
  XFillRectangle(dpy, win, gc, 0, 0, 16, 16);
  XClearArea(dpy, win, -2, -2, 4, 4, False);
  TEST_ASSERT_EQUAL_INT(256 - 4, count_set());
  XClearArea(dpy, win, 12, 14, 0xFFFFFFFFu, 0xFFFFFFFFu, False);
  TEST_ASSERT_EQUAL_INT(256 - 4 - 4 * 2, count_set());
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(11, 14));
  XFreeGC(dpy, gc);
}

static GC bitmap_gc(Pixmap bitmap, unsigned long bit) {
  XGCValues v;
  v.foreground = bit;
  return XCreateGC(dpy, bitmap, GCForeground, &v);
}

/* Stamps a bitmap onto the 16 by 16 window in white, so count_set() and at()
 * show which bits were set. */
static void stamp_bitmap(Pixmap bitmap) {
  XGCValues v;
  v.foreground = 0xFFFF;
  v.background = 0;
  GC gc = XCreateGC(dpy, win, GCForeground | GCBackground, &v);
  XClearWindow(dpy, win);
  XCopyPlane(dpy, bitmap, win, gc, 0, 0, 16, 16, 0, 0, 1);
  XFreeGC(dpy, gc);
}

void test_fill_rectangle_in_a_bitmap_sets_its_bits_and_leaves_the_window(void) {
  Pixmap p = XCreatePixmap(dpy, win, 16, 16, 1);
  GC gc = bitmap_gc(p, 1);
  XClearWindow(dpy, win);
  XFillRectangle(dpy, p, gc, 2, 3, 5, 4);
  TEST_ASSERT_EQUAL_INT(0, count_set());
  stamp_bitmap(p);
  TEST_ASSERT_EQUAL_INT(20, count_set());
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(2, 3));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(6, 6));
  TEST_ASSERT_EQUAL_HEX16(0, at(7, 3));
  TEST_ASSERT_EQUAL_HEX16(0, at(2, 7));
  TEST_ASSERT_EQUAL_HEX16(0, at(1, 3));
  TEST_ASSERT_EQUAL_HEX16(0, at(2, 2));
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, p);
}

void test_fill_rectangle_in_a_bitmap_with_foreground_zero_clears_bits(void) {
  Pixmap p = XCreatePixmap(dpy, win, 16, 16, 1);
  GC one = bitmap_gc(p, 1), zero = bitmap_gc(p, 0);
  XFillRectangle(dpy, p, one, 0, 0, 16, 16);
  XFillRectangle(dpy, p, zero, 4, 4, 3, 3);
  stamp_bitmap(p);
  TEST_ASSERT_EQUAL_INT(256 - 9, count_set());
  TEST_ASSERT_EQUAL_HEX16(0, at(5, 5));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(7, 5));
  XFreeGC(dpy, one);
  XFreeGC(dpy, zero);
  XFreePixmap(dpy, p);
}

void test_fill_rectangle_in_a_bitmap_clips_to_the_bitmap(void) {
  Pixmap p = XCreatePixmap(dpy, win, 16, 16, 1);
  GC gc = bitmap_gc(p, 1);
  XFillRectangle(dpy, p, gc, -3, -3, 6, 6);
  XFillRectangle(dpy, p, gc, 14, 15, 0xFFFFFFFFu, 0xFFFFFFFFu);
  stamp_bitmap(p);
  TEST_ASSERT_EQUAL_INT(9 + 2, count_set());
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(2, 2));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(15, 15));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(14, 15));
  TEST_ASSERT_EQUAL_HEX16(0, at(3, 3));
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, p);
}

void test_fill_polygon_in_a_bitmap_fills_a_triangle_by_pixel_centres(void) {
  Pixmap p = XCreatePixmap(dpy, win, 16, 16, 1);
  GC gc = bitmap_gc(p, 1);
  XPoint tri[3] = {{0, 0}, {8, 0}, {0, 8}};
  XFillPolygon(dpy, p, gc, tri, 3, Convex, CoordModeOrigin);
  stamp_bitmap(p);
  TEST_ASSERT_EQUAL_INT(28, count_set());
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(6, 0));
  TEST_ASSERT_EQUAL_HEX16(0, at(7, 0));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(0, 6));
  TEST_ASSERT_EQUAL_HEX16(0, at(0, 7));
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, p);
}

void test_fill_polygon_in_a_bitmap_starts_each_row_at_its_left_edge(void) {
  Pixmap p = XCreatePixmap(dpy, win, 16, 16, 1);
  GC gc = bitmap_gc(p, 1);
  XPoint tri[3] = {{8, 0}, {8, 8}, {0, 8}};
  XFillPolygon(dpy, p, gc, tri, 3, Convex, CoordModeOrigin);
  stamp_bitmap(p);
  TEST_ASSERT_EQUAL_INT(36, count_set());
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(7, 0));
  TEST_ASSERT_EQUAL_HEX16(0, at(6, 0));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(0, 7));
  TEST_ASSERT_EQUAL_HEX16(0, at(0, 6));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(1, 6));
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, p);
}

void test_fill_polygon_in_a_bitmap_leaves_a_notch_empty(void) {
  Pixmap p = XCreatePixmap(dpy, win, 16, 16, 1);
  GC gc = bitmap_gc(p, 1);
  XPoint u[8] = {{0, 0}, {8, 0}, {8, 8}, {6, 8}, {6, 2}, {2, 2}, {2, 8}, {0, 8}};
  XFillPolygon(dpy, p, gc, u, 8, Nonconvex, CoordModeOrigin);
  stamp_bitmap(p);
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(4, 1));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(1, 5));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(7, 5));
  TEST_ASSERT_EQUAL_HEX16(0, at(4, 5));
  TEST_ASSERT_EQUAL_INT(8 * 2 + 2 * 6 * 2, count_set());
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, p);
}

void test_fill_polygon_in_a_bitmap_clips_to_the_bitmap(void) {
  Pixmap p = XCreatePixmap(dpy, win, 16, 16, 1);
  GC gc = bitmap_gc(p, 1);
  XPoint big[4] = {{-40, -40}, {60, -40}, {60, 60}, {-40, 60}};
  XFillPolygon(dpy, p, gc, big, 4, Convex, CoordModeOrigin);
  stamp_bitmap(p);
  TEST_ASSERT_EQUAL_INT(256, count_set());
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, p);
}

void test_dashed_line_styles_draw_solid(void) {
  XGCValues v;
  v.foreground = 0xFFFF;
  GC gc = XCreateGC(dpy, win, GCForeground, &v);
  XSetLineAttributes(dpy, gc, 1, LineOnOffDash, CapNotLast, JoinMiter);
  XDrawLine(dpy, win, gc, 0, 0, 5, 0);
  TEST_ASSERT_EQUAL_INT(6, count_set());
  XClearWindow(dpy, win);
  XSetLineAttributes(dpy, gc, 1, LineDoubleDash, CapNotLast, JoinMiter);
  XDrawLine(dpy, win, gc, 0, 0, 5, 0);
  TEST_ASSERT_EQUAL_INT(6, count_set());
  XFreeGC(dpy, gc);
}

void test_max_request_size_leaves_room_for_long_polylines(void) {
  /* Lisa draws (size - 3) / 2 points per XDrawLines call. */
  TEST_ASSERT_TRUE((XMaxRequestSize(dpy) - 3) / 2 >= 1000);
}

void test_parse_color_reads_hex_and_names_into_16_bit_channels(void) {
  XColor c = {0};
  TEST_ASSERT_TRUE(XParseColor(dpy, 1, "#9C542B", &c) != 0);
  TEST_ASSERT_EQUAL_HEX16(0x9C9C, c.red);
  TEST_ASSERT_EQUAL_HEX16(0x5454, c.green);
  TEST_ASSERT_EQUAL_HEX16(0x2B2B, c.blue);
  TEST_ASSERT_TRUE(XParseColor(dpy, 1, "White", &c) != 0);
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, c.red);
  TEST_ASSERT_TRUE(XAllocColor(dpy, 1, &c) != 0);
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, (uint16_t)c.pixel);
  TEST_ASSERT_TRUE(XParseColor(dpy, 1, "#F80000", &c) != 0);
  TEST_ASSERT_TRUE(XAllocColor(dpy, 1, &c) != 0);
  TEST_ASSERT_EQUAL_HEX16(px_swap(0xF800), (uint16_t)c.pixel);
}

void test_parse_color_rejects_unknown_specs(void) {
  XColor c = {0};
  TEST_ASSERT_EQUAL_INT(0, XParseColor(dpy, 1, "notacolour", &c));
  TEST_ASSERT_EQUAL_INT(0, XParseColor(dpy, 1, "#12", &c));
  TEST_ASSERT_EQUAL_INT(0, XParseColor(dpy, 1, "#GG0000", &c));
}

void test_gc_defaults_are_width_0_butt_miter(void) {
  XGCValues v;
  v.foreground = 1;
  GC gc = XCreateGC(dpy, win, GCForeground, &v);
  TEST_ASSERT_EQUAL_INT(0, gc->line_width);
  TEST_ASSERT_EQUAL_INT(CapButt, gc->cap_style);
  TEST_ASSERT_EQUAL_INT(JoinMiter, gc->join_style);
  XFreeGC(dpy, gc);
}

void test_create_gc_reads_line_width_cap_and_join_from_the_mask(void) {
  XGCValues v;
  v.foreground = 1;
  v.line_width = 4;
  v.cap_style = CapRound;
  v.join_style = JoinBevel;
  GC gc = XCreateGC(dpy, win,
                    GCForeground | GCLineWidth | GCCapStyle | GCJoinStyle, &v);
  TEST_ASSERT_EQUAL_INT(4, gc->line_width);
  TEST_ASSERT_EQUAL_INT(CapRound, gc->cap_style);
  TEST_ASSERT_EQUAL_INT(JoinBevel, gc->join_style);
  XFreeGC(dpy, gc);
}

void test_gc_accepts_a_plane_mask_and_draws_in_the_foreground_regardless(void) {
  XGCValues v;
  v.foreground = 0x1234;
  v.plane_mask = 0x0F0F;
  GC gc = XCreateGC(dpy, win, GCForeground | GCPlaneMask, &v);
  canvas_clear(&cv, 0);
  XFillRectangle(dpy, win, gc, 2, 2, 3, 3);
  TEST_ASSERT_EQUAL_UINT16(0x1234, cv.px[3 * cv.w + 3]);
  XFreeGC(dpy, gc);
}

void test_allocate_alpha_colors_reports_no_colour_planes(void) {
  int nplanes = 5;
  unsigned long *masks = (unsigned long *)0x1;
  unsigned long base = 99;
  allocate_alpha_colors(NULL, NULL, 1, &nplanes, True, &masks, &base);
  TEST_ASSERT_EQUAL_INT(0, nplanes);
  TEST_ASSERT_NULL(masks);
}

void test_no_writable_colour_cells_can_be_allocated_or_stored(void) {
  int ncolors = 8;
  unsigned long pixels[9] = {7};
  allocate_writable_colors(NULL, 1, pixels, &ncolors);
  TEST_ASSERT_EQUAL_INT(0, ncolors);
  XColor c = {0};
  c.pixel = 5;
  c.red = 0x1234;
  XStoreColors(dpy, 1, &c, 1);
  TEST_ASSERT_EQUAL_UINT16(0x1234, c.red);
  TEST_ASSERT_EQUAL_UINT64(5, c.pixel);
}

void test_change_gc_updates_only_the_masked_line_fields(void) {
  XGCValues v;
  v.foreground = 1;
  GC gc = XCreateGC(dpy, win, GCForeground, &v);
  v.line_width = 5;
  v.cap_style = CapProjecting;
  v.join_style = JoinRound;
  XChangeGC(dpy, gc, GCLineWidth, &v);
  TEST_ASSERT_EQUAL_INT(5, gc->line_width);
  TEST_ASSERT_EQUAL_INT(CapButt, gc->cap_style);
  TEST_ASSERT_EQUAL_INT(JoinMiter, gc->join_style);
  XChangeGC(dpy, gc, GCCapStyle | GCJoinStyle, &v);
  TEST_ASSERT_EQUAL_INT(CapProjecting, gc->cap_style);
  TEST_ASSERT_EQUAL_INT(JoinRound, gc->join_style);
  XFreeGC(dpy, gc);
}

void test_set_line_attributes_sets_all_three(void) {
  XGCValues v;
  v.foreground = 1;
  GC gc = XCreateGC(dpy, win, GCForeground, &v);
  XSetLineAttributes(dpy, gc, 7, LineSolid, CapRound, JoinBevel);
  TEST_ASSERT_EQUAL_INT(7, gc->line_width);
  TEST_ASSERT_EQUAL_INT(CapRound, gc->cap_style);
  TEST_ASSERT_EQUAL_INT(JoinBevel, gc->join_style);
  XFreeGC(dpy, gc);
}

/* Wide-line tests draw on a 64 by 64 canvas, in white on black. */
static void use_canvas(int n) {
  canvas_free(&cv);
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&cv, n, n, malloc));
  dpy->canvas = &cv;
}

static GC wide_gc(int width, int cap, int join) {
  XGCValues v;
  v.foreground = 0xFFFF;
  GC gc = XCreateGC(dpy, win, GCForeground, &v);
  XSetLineAttributes(dpy, gc, width, LineSolid, cap, join);
  return gc;
}

void test_wide_horizontal_line_butt_caps(void) {
  use_canvas(64);
  GC gc = wide_gc(3, CapButt, JoinMiter);
  XDrawLine(dpy, win, gc, 10, 10, 20, 10);
  TEST_ASSERT_EQUAL_INT(33, count_set());
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(10, 9));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(20, 11));
  TEST_ASSERT_EQUAL_HEX16(0, at(9, 10));
  TEST_ASSERT_EQUAL_HEX16(0, at(21, 10));
  TEST_ASSERT_EQUAL_HEX16(0, at(15, 8));
  TEST_ASSERT_EQUAL_HEX16(0, at(15, 12));
  XFreeGC(dpy, gc);
}

void test_wide_even_width_starts_half_before(void) {
  use_canvas(64);
  GC gc = wide_gc(2, CapButt, JoinMiter);
  XDrawLine(dpy, win, gc, 10, 10, 20, 10);
  TEST_ASSERT_EQUAL_INT(22, count_set());
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(15, 9));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(15, 10));
  TEST_ASSERT_EQUAL_HEX16(0, at(15, 11));
  TEST_ASSERT_EQUAL_HEX16(0, at(15, 8));
  XFreeGC(dpy, gc);
}

void test_wide_vertical_line_butt_caps(void) {
  use_canvas(64);
  GC gc = wide_gc(3, CapButt, JoinMiter);
  XDrawLine(dpy, win, gc, 30, 40, 30, 20);
  TEST_ASSERT_EQUAL_INT(63, count_set());
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(29, 20));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(31, 40));
  TEST_ASSERT_EQUAL_HEX16(0, at(30, 19));
  TEST_ASSERT_EQUAL_HEX16(0, at(30, 41));
  XFreeGC(dpy, gc);
}

void test_projecting_caps_extend_half_the_width(void) {
  use_canvas(64);
  GC gc = wide_gc(3, CapProjecting, JoinMiter);
  XDrawLine(dpy, win, gc, 10, 10, 20, 10);
  TEST_ASSERT_EQUAL_INT(39, count_set());
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(9, 9));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(21, 11));
  TEST_ASSERT_EQUAL_HEX16(0, at(8, 10));
  TEST_ASSERT_EQUAL_HEX16(0, at(22, 10));
  XFreeGC(dpy, gc);
}

void test_round_caps_width_6_have_tip_but_no_corner(void) {
  use_canvas(64);
  GC gc = wide_gc(6, CapRound, JoinMiter);
  XDrawLine(dpy, win, gc, 20, 20, 40, 20);
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(18, 20));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(42, 20));
  TEST_ASSERT_EQUAL_HEX16(0, at(17, 17));
  TEST_ASSERT_EQUAL_HEX16(0, at(43, 23));
  XFreeGC(dpy, gc);
}

void test_diagonal_wide_line_covers_the_centre_line_and_is_about_width_thick(void) {
  use_canvas(64);
  GC gc = wide_gc(4, CapButt, JoinMiter);
  XDrawLine(dpy, win, gc, 5, 5, 40, 25);
  int first_miss = -1;
  for (int i = 0; i <= 35 && first_miss < 0; i++)
    if (at(5 + i, 5 + (i * 20 + 17) / 35) != 0xFFFF) first_miss = i;
  TEST_ASSERT_EQUAL_INT(-1, first_miss);
  const int n = count_set();
  TEST_ASSERT_TRUE_MESSAGE(n >= 137 && n <= 185, "area is about length times width");
  XFreeGC(dpy, gc);
}

void test_zero_length_wide_line_round_cap_is_a_dot_butt_is_nothing_wider_than_a_point(void) {
  use_canvas(64);
  GC round = wide_gc(6, CapRound, JoinMiter);
  XDrawLine(dpy, win, round, 30, 30, 30, 30);
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(30, 30));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(28, 30));
  TEST_ASSERT_TRUE(count_set() > 10);
  XFreeGC(dpy, round);
  use_canvas(64);
  GC butt = wide_gc(6, CapButt, JoinMiter);
  XDrawLine(dpy, win, butt, 30, 30, 30, 30);
  TEST_ASSERT_TRUE(count_set() <= 1);
  XFreeGC(dpy, butt);
}

/* A round cap is a disc of the line's width. The stroker may keep its own table
 * of disc rows, so every width and every position (partly off the canvas too)
 * must give exactly the pixels canvas_fill_ellipse gives. */
void test_round_cap_dot_matches_canvas_fill_ellipse_at_every_width_and_position(void) {
  enum { N = 64 };
  Canvas want;
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&want, N, N, malloc));
  int mismatches = 0;
  use_canvas(N);
  for (int w = 2; w <= 40; w++) {
    GC gc = wide_gc(w, CapRound, JoinMiter);
    for (int pos = -w - 2; pos <= N + w + 2; pos++) {
      canvas_clear(&cv, 0);
      canvas_clear(&want, 0);
      canvas_fill_ellipse(&want, pos - w / 2, 20 - w / 2, w, w, 0xFFFF);
      XDrawLine(dpy, win, gc, pos, 20, pos, 20);
      if (memcmp(want.px, cv.px, sizeof(uint16_t) * N * N) != 0) mismatches++;
      canvas_clear(&cv, 0);
      canvas_clear(&want, 0);
      canvas_fill_ellipse(&want, 30 - w / 2, pos - w / 2, w, w, 0xFFFF);
      XDrawLine(dpy, win, gc, 30, pos, 30, pos);
      if (memcmp(want.px, cv.px, sizeof(uint16_t) * N * N) != 0) mismatches++;
    }
    XFreeGC(dpy, gc);
  }
  canvas_free(&want);
  TEST_ASSERT_EQUAL_INT(0, mismatches);
}

void test_wide_line_far_off_canvas_draws_nothing_and_does_not_overflow(void) {
  use_canvas(64);
  GC gc = wide_gc(5, CapRound, JoinMiter);
  XDrawLine(dpy, win, gc, INT_MAX, INT_MAX, INT_MAX - 10, INT_MAX);
  XDrawLine(dpy, win, gc, INT_MIN, INT_MIN, INT_MIN + 10, INT_MIN + 3);
  TEST_ASSERT_EQUAL_INT(0, count_set());
  XFreeGC(dpy, gc);
}

void test_wide_line_across_the_whole_int_range_still_crosses_the_canvas(void) {
  use_canvas(64);
  GC gc = wide_gc(5, CapButt, JoinMiter);
  XDrawLine(dpy, win, gc, -INT_MAX, 5, INT_MAX, 5);
  TEST_ASSERT_EQUAL_INT(64 * 5, count_set());
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(0, 3));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(63, 7));
  TEST_ASSERT_EQUAL_HEX16(0, at(0, 2));
  TEST_ASSERT_EQUAL_HEX16(0, at(0, 8));
  XFreeGC(dpy, gc);
  use_canvas(64);
  GC diag = wide_gc(3, CapButt, JoinMiter);
  XDrawLine(dpy, win, diag, -INT_MAX, -INT_MAX, INT_MAX, INT_MAX);
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(32, 32));
  TEST_ASSERT_EQUAL_HEX16(0, at(32, 40));
  XFreeGC(dpy, diag);
}

void test_an_enormous_width_is_bounded_not_a_hang_or_overflow(void) {
  use_canvas(64);
  GC gc = wide_gc(INT_MAX, CapButt, JoinMiter);
  XDrawLine(dpy, win, gc, 10, 10, 20, 10);
  XDrawLine(dpy, win, gc, 10, 10, 20, 30);
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(15, 0));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(15, 63));
  XFreeGC(dpy, gc);
}

void test_wide_line_clips_at_the_canvas_edge_and_marks_dirty_rows(void) {
  use_canvas(64);
  GC gc = wide_gc(3, CapButt, JoinMiter);
  canvas_clear_dirty(&cv);
  XDrawLine(dpy, win, gc, 0, 0, 30, 0);
  int x0, x1;
  TEST_ASSERT_TRUE(canvas_dirty_row(&cv, 0, &x0, &x1));
  TEST_ASSERT_TRUE(canvas_dirty_row(&cv, 1, &x0, &x1));
  TEST_ASSERT_FALSE(canvas_dirty_row(&cv, 2, &x0, &x1));
  TEST_ASSERT_EQUAL_INT(0, x0);
  TEST_ASSERT_EQUAL_INT(30, x1);
  TEST_ASSERT_EQUAL_INT(62, count_set());
  XFreeGC(dpy, gc);
}

void test_diagonal_wide_line_marks_dirty_rows_it_touches(void) {
  use_canvas(64);
  GC gc = wide_gc(4, CapButt, JoinMiter);
  canvas_clear_dirty(&cv);
  XDrawLine(dpy, win, gc, 5, 5, 40, 25);
  int x0, x1;
  for (int y = 0; y < 64; y++) {
    int touched = 0;
    for (int x = 0; x < 64; x++) touched |= at(x, y) != 0;
    TEST_ASSERT_EQUAL_INT(touched, canvas_dirty_row(&cv, y, &x0, &x1));
  }
  XFreeGC(dpy, gc);
}

void test_width_0_and_1_match_the_old_line(void) {
  for (int width = 0; width <= 1; width++) {
    use_canvas(64);
    canvas_line(&cv, 3, 2, 12, 9, 0xFFFF);
    canvas_line(&cv, 50, 60, 4, 8, 0xFFFF);
    uint16_t *want = (uint16_t *)malloc(64 * 64 * sizeof(uint16_t));
    memcpy(want, cv.px, 64 * 64 * sizeof(uint16_t));
    use_canvas(64);
    GC gc = wide_gc(width, CapRound, JoinRound);
    XDrawLine(dpy, win, gc, 3, 2, 12, 9);
    XDrawLine(dpy, win, gc, 50, 60, 4, 8);
    TEST_ASSERT_EQUAL_MEMORY(want, cv.px, 64 * 64 * sizeof(uint16_t));
    free(want);
    XFreeGC(dpy, gc);
  }
}

void test_a_45_degree_wide_line_is_symmetric_about_its_diagonal(void) {
  for (int width = 2; width <= 5; width++) {
    use_canvas(64);
    GC gc = wide_gc(width, CapButt, JoinMiter);
    XDrawLine(dpy, win, gc, 10, 10, 40, 40);
    for (int y = 0; y < 64; y++)
      for (int x = 0; x < y; x++) TEST_ASSERT_EQUAL_HEX16(at(x, y), at(y, x));
    TEST_ASSERT_TRUE(count_set() > 30 * width);
    XFreeGC(dpy, gc);
  }
}

void test_an_enormous_width_with_round_caps_neither_hangs_nor_overflows(void) {
  use_canvas(64);
  GC gc = wide_gc(INT_MAX, CapRound, JoinMiter);
  XDrawLine(dpy, win, gc, 10, 10, 20, 10);
  XDrawLine(dpy, win, gc, 10, 10, 20, 30);
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(15, 10));
  XFreeGC(dpy, gc);
}

static void draw_path(GC gc, const XPoint *pts, int n) {
  XDrawLines(dpy, win, gc, (XPoint *)pts, n, CoordModeOrigin);
}

static uint16_t *snapshot(void) {
  uint16_t *copy = (uint16_t *)malloc((size_t)cv.w * cv.h * sizeof(uint16_t));
  memcpy(copy, cv.px, (size_t)cv.w * cv.h * sizeof(uint16_t));
  return copy;
}

void test_miter_join_fills_the_outer_corner(void) {
  use_canvas(64);
  GC gc = wide_gc(4, CapButt, JoinMiter);
  const XPoint path[] = {{10, 30}, {30, 30}, {30, 10}};
  draw_path(gc, path, 3);
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(30, 30));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(31, 31));
  TEST_ASSERT_EQUAL_HEX16(0, at(32, 31));
  TEST_ASSERT_EQUAL_HEX16(0, at(31, 32));
  XFreeGC(dpy, gc);
}

void test_bevel_join_leaves_the_outer_corner_unset(void) {
  use_canvas(64);
  GC gc = wide_gc(4, CapButt, JoinBevel);
  const XPoint path[] = {{10, 30}, {30, 30}, {30, 10}};
  draw_path(gc, path, 3);
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(30, 30));
  TEST_ASSERT_EQUAL_HEX16(0, at(31, 31));
  XFreeGC(dpy, gc);
}

void test_round_join_fills_a_disc_at_the_vertex(void) {
  use_canvas(64);
  GC gc = wide_gc(8, CapButt, JoinRound);
  const XPoint path[] = {{10, 30}, {30, 30}, {30, 10}};
  draw_path(gc, path, 3);
  TEST_ASSERT_EQUAL_HEX16_MESSAGE(0xFFFF, at(32, 32), "inside the disc, outside a bevel");
  TEST_ASSERT_EQUAL_HEX16_MESSAGE(0, at(33, 33), "outside the disc, inside a miter");
  XFreeGC(dpy, gc);
  use_canvas(64);
  GC miter = wide_gc(8, CapButt, JoinMiter);
  draw_path(miter, path, 3);
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(33, 33));
  XFreeGC(dpy, miter);
}

void test_sharp_angle_miter_falls_back_to_bevel(void) {
  use_canvas(64);
  GC gc = wide_gc(4, CapButt, JoinMiter);
  const XPoint path[] = {{10, 30}, {50, 30}, {10, 32}};
  draw_path(gc, path, 3);
  for (int y = 0; y < 64; y++)
    for (int x = 54; x < 64; x++) TEST_ASSERT_EQUAL_HEX16(0, at(x, y));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(49, 30));
  XFreeGC(dpy, gc);
}

void test_polyline_with_two_points_equals_a_segment(void) {
  for (int width = 2; width <= 5; width++) {
    const XPoint lines[][2] = {{{10, 10}, {40, 10}}, {{10, 10}, {10, 40}},
                               {{10, 10}, {40, 25}}};
    for (int k = 0; k < 3; k++) {
      use_canvas(64);
      GC gc = wide_gc(width, CapProjecting, JoinMiter);
      XDrawLine(dpy, win, gc, lines[k][0].x, lines[k][0].y, lines[k][1].x, lines[k][1].y);
      uint16_t *want = snapshot();
      use_canvas(64);
      draw_path(gc, lines[k], 2);
      TEST_ASSERT_EQUAL_MEMORY(want, cv.px, (size_t)64 * 64 * sizeof(uint16_t));
      free(want);
      XFreeGC(dpy, gc);
    }
  }
}

void test_polyline_n_0_and_1_draw_nothing_wide(void) {
  use_canvas(64);
  GC butt = wide_gc(4, CapButt, JoinMiter);
  const XPoint one[] = {{30, 30}};
  draw_path(butt, one, 0);
  draw_path(butt, one, 1);
  TEST_ASSERT_EQUAL_INT(0, count_set());
  XFreeGC(dpy, butt);
  GC round = wide_gc(6, CapRound, JoinMiter);
  draw_path(round, one, 1);
  TEST_ASSERT_TRUE(count_set() > 10);
  XFreeGC(dpy, round);
}

void test_repeated_points_do_not_change_the_picture(void) {
  use_canvas(64);
  GC gc = wide_gc(3, CapRound, JoinRound);
  const XPoint plain[] = {{10, 10}, {20, 10}};
  draw_path(gc, plain, 2);
  uint16_t *want = snapshot();
  use_canvas(64);
  const XPoint repeated[] = {{10, 10}, {10, 10}, {20, 10}, {20, 10}, {20, 10}};
  draw_path(gc, repeated, 5);
  TEST_ASSERT_EQUAL_MEMORY(want, cv.px, (size_t)64 * 64 * sizeof(uint16_t));
  free(want);
  XFreeGC(dpy, gc);
}

void test_180_degree_turn_has_no_gap(void) {
  use_canvas(64);
  GC gc = wide_gc(3, CapButt, JoinMiter);
  const XPoint path[] = {{10, 20}, {30, 20}, {10, 20}};
  draw_path(gc, path, 3);
  TEST_ASSERT_EQUAL_INT(63, count_set());
  XFreeGC(dpy, gc);
}

void test_collinear_points_draw_the_same_as_one_segment(void) {
  const XPoint lines[][3] = {{{10, 20}, {25, 20}, {40, 20}},
                             {{20, 10}, {20, 25}, {20, 40}},
                             {{5, 5}, {15, 15}, {25, 25}},
                             {{5, 5}, {17, 11}, {29, 17}}};
  int bad = -1;
  for (int width = 2; width <= 6; width++)
    for (int k = 0; k < 4; k++) {
      use_canvas(64);
      GC gc = wide_gc(width, CapButt, JoinMiter);
      XDrawLine(dpy, win, gc, lines[k][0].x, lines[k][0].y, lines[k][2].x, lines[k][2].y);
      uint16_t *want = snapshot();
      use_canvas(64);
      draw_path(gc, lines[k], 3);
      if (bad < 0 && memcmp(want, cv.px, (size_t)64 * 64 * sizeof(uint16_t)) != 0)
        bad = width * 10 + k;
      free(want);
      XFreeGC(dpy, gc);
    }
  TEST_ASSERT_EQUAL_INT_MESSAGE(-1, bad, "first differing case is width * 10 + line");
}

void test_draw_rectangle_wide_has_mitered_corners(void) {
  use_canvas(64);
  GC gc = wide_gc(3, CapButt, JoinMiter);
  XDrawRectangle(dpy, win, gc, 10, 10, 20, 20);
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(9, 9));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(31, 9));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(31, 31));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(9, 31));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(20, 10));
  TEST_ASSERT_EQUAL_HEX16(0, at(20, 20));
  TEST_ASSERT_EQUAL_HEX16(0, at(12, 12));
  TEST_ASSERT_EQUAL_HEX16(0, at(8, 8));
  XFreeGC(dpy, gc);
}

void test_width_0_and_1_polyline_and_rectangle_match_the_old_ones(void) {
  for (int width = 0; width <= 1; width++) {
    use_canvas(64);
    canvas_line(&cv, 5, 5, 20, 9, 0xFFFF);
    canvas_line(&cv, 20, 9, 12, 30, 0xFFFF);
    canvas_line(&cv, 40, 40, 50, 40, 0xFFFF);
    canvas_line(&cv, 50, 40, 50, 55, 0xFFFF);
    canvas_line(&cv, 50, 55, 40, 55, 0xFFFF);
    canvas_line(&cv, 40, 55, 40, 40, 0xFFFF);
    uint16_t *want = snapshot();
    use_canvas(64);
    GC gc = wide_gc(width, CapRound, JoinRound);
    const XPoint path[] = {{5, 5}, {20, 9}, {12, 30}};
    draw_path(gc, path, 3);
    XDrawRectangle(dpy, win, gc, 40, 40, 10, 15);
    TEST_ASSERT_EQUAL_MEMORY(want, cv.px, (size_t)64 * 64 * sizeof(uint16_t));
    free(want);
    XFreeGC(dpy, gc);
  }
}

void test_draw_segments_draws_each_segment_independently(void) {
  use_canvas(64);
  XGCValues v;
  v.foreground = 0xFFFF;
  GC gc = XCreateGC(dpy, win, GCForeground, &v);
  XSegment segs[] = {{5, 5, 15, 5}, {5, 10, 5, 20}};
  XDrawSegments(dpy, win, gc, segs, 2);
  TEST_ASSERT_EQUAL_INT(22, count_set());
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(15, 5));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(5, 20));
  XFreeGC(dpy, gc);
}

void test_draw_segments_with_zero_count_draws_nothing(void) {
  use_canvas(64);
  XGCValues v;
  v.foreground = 0xFFFF;
  GC gc = XCreateGC(dpy, win, GCForeground, &v);
  XDrawSegments(dpy, win, gc, NULL, 0);
  TEST_ASSERT_EQUAL_INT(0, count_set());
  XFreeGC(dpy, gc);
}

void test_draw_segments_wide_uses_caps_not_joins(void) {
  use_canvas(64);
  GC gc = wide_gc(4, CapButt, JoinMiter);
  XSegment segs[] = {{10, 30, 30, 30}, {30, 30, 30, 10}};
  XDrawSegments(dpy, win, gc, segs, 2);
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(29, 29));
  TEST_ASSERT_EQUAL_HEX16_MESSAGE(0, at(31, 31), "no join between separate segments");
  XFreeGC(dpy, gc);
}

static void reference_ellipse_check(int x, int y, unsigned w, unsigned h) {
  const int n = arc_point_count(w, h, 360 * 64);
  int *xy = (int *)malloc((size_t)n * 2 * sizeof(int));
  TEST_ASSERT_EQUAL_INT(n, arc_points(x, y, w, h, 0, 360 * 64, xy, n));
  const double cx = x + w / 2.0, cy = y + h / 2.0, rx = w / 2.0, ry = h / 2.0;
  const double rmin = rx < ry ? rx : ry;
  int worst_x100 = 0;
  for (int i = 0; i < n; i++) {
    const double u = (xy[2 * i] - cx) / rx, v = (xy[2 * i + 1] - cy) / ry;
    const int off = (int)(fabs(sqrt(u * u + v * v) - 1.0) * rmin * 100);
    if (off > worst_x100) worst_x100 = off;
  }
  free(xy);
  TEST_ASSERT_TRUE_MESSAGE(worst_x100 <= 75, "a point is more than 0.75 px off the ellipse");
}

/* arc_points as it was before it learned to rotate instead of calling sinf
 * and cosf at every point, copied from main: every arc below is compared with
 * it, point for point. arc_point_count is unchanged, so both use it. */
#define REFERENCE_FULL_SWEEP (360 * 64)
#define REFERENCE_DEGREES_TO_RADIANS 0.017453292519943295f
static int reference_sweep_of(int angle2) {
  if (angle2 > REFERENCE_FULL_SWEEP) return REFERENCE_FULL_SWEEP;
  if (angle2 < -REFERENCE_FULL_SWEEP) return -REFERENCE_FULL_SWEEP;
  return angle2;
}


static int reference_arc_points(int x, int y, unsigned w, unsigned h, int angle1, int angle2,
               int *xy, int max_points) {
  int n = arc_point_count(w, h, angle2);
  if (n > max_points) n = max_points;
  if (n < 2) return n < 0 ? 0 : n;
  const float cx = (float)x + (float)w / 2.0f, cy = (float)y + (float)h / 2.0f;
  const float rx = (float)w / 2.0f, ry = (float)h / 2.0f;
  const float start = (float)angle1 / 64.0f * REFERENCE_DEGREES_TO_RADIANS;
  const float sweep = (float)reference_sweep_of(angle2) / 64.0f * REFERENCE_DEGREES_TO_RADIANS;
  for (int i = 0; i < n; i++) {
    const float a = start + sweep * (float)i / (float)(n - 1);
    xy[2 * i] = (int)lrintf(cx + rx * cosf(a));
    xy[2 * i + 1] = (int)lrintf(cy - ry * sinf(a));
  }
  return n;
}

/* Random arcs, by `kind`: 0 small boxes like a hack's dots and petals, 1 boxes
 * of any size up to the canvas, 2 huge angles, 3 huge coordinates and sizes, 4
 * coordinates up to a million, where float's own spacing is coarse.
 * Returns how many arcs differed from the reference. */
static int arcs_that_differ_from_the_reference(int kind, int count) {
  srandom(1);
  int differing = 0;
  static int fast[2 * ARC_MAX_POINTS], ref[2 * ARC_MAX_POINTS];
  for (int i = 0; i < count; i++) {
    int x = (int)(random() % 600) - 100, y = (int)(random() % 600) - 100;
    unsigned w = 1 + (unsigned)(random() % 60), h = 1 + (unsigned)(random() % 60);
    int angle1 = (int)(random() % (720 * 64)) - 360 * 64;
    int angle2 = (int)(random() % (800 * 64)) - 400 * 64;
    if (kind == 1) {
      w = 1 + (unsigned)(random() % 900);
      h = 1 + (unsigned)(random() % 900);
    } else if (kind == 2) {
      angle1 = (int)(random() % 2000000000) - 1000000000;
      angle2 = (int)(random() % 2000000000) - 1000000000;
    } else if (kind == 4) {
      x = (int)(random() % 2000000) - 1000000;
      y = (int)(random() % 2000000) - 1000000;
      w = 1 + (unsigned)(random() % 1000);
      h = 1 + (unsigned)(random() % 1000);
    } else if (kind == 3) {
      x = (int)(random() % 2000000000) - 1000000000;
      y = (int)(random() % 2000000000) - 1000000000;
      w = 1 + (unsigned)(random() % 2000000000);
      h = 1 + (unsigned)(random() % 2000000000);
    }
    const int nf = arc_points(x, y, w, h, angle1, angle2, fast, ARC_MAX_POINTS);
    const int nr = reference_arc_points(x, y, w, h, angle1, angle2, ref, ARC_MAX_POINTS);
    differing += nf != nr || memcmp(fast, ref, sizeof(int) * 2 * (size_t)(nr > 0 ? nr : 0)) != 0;
  }
  return differing;
}

void test_small_arcs_match_the_reference(void) {
  TEST_ASSERT_EQUAL_INT(0, arcs_that_differ_from_the_reference(0, 40000));
}

void test_arcs_of_any_size_match_the_reference(void) {
  TEST_ASSERT_EQUAL_INT(0, arcs_that_differ_from_the_reference(1, 4000));
}

void test_arcs_with_huge_angles_match_the_reference(void) {
  TEST_ASSERT_EQUAL_INT(0, arcs_that_differ_from_the_reference(2, 10000));
}

void test_arcs_with_coordinates_up_to_a_million_match_the_reference(void) {
  TEST_ASSERT_EQUAL_INT(0, arcs_that_differ_from_the_reference(4, 20000));
}

/* Found in review of #131: float spacing near 9e5 is about 0.06, which the
 * tolerance did not allow for, so point 582 came out a pixel off. */
void test_an_arc_far_from_the_canvas_matches_the_reference_at_every_point(void) {
  static int fast[2 * ARC_MAX_POINTS], ref[2 * ARC_MAX_POINTS];
  const int nf = arc_points(901474, -723879, 859, 396, -88971, 14837, fast, ARC_MAX_POINTS);
  const int nr = reference_arc_points(901474, -723879, 859, 396, -88971, 14837, ref, ARC_MAX_POINTS);
  TEST_ASSERT_EQUAL_INT(nr, nf);
  for (int i = 0; i < nr; i++) {
    TEST_ASSERT_EQUAL_INT_MESSAGE(ref[2 * i], fast[2 * i], "x");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ref[2 * i + 1], fast[2 * i + 1], "y");
  }
}

void test_arcs_with_huge_coordinates_and_sizes_match_the_reference(void) {
  TEST_ASSERT_EQUAL_INT(0, arcs_that_differ_from_the_reference(3, 10000));
}

void test_arc_points_lie_within_a_pixel_of_the_ellipse(void) {
  reference_ellipse_check(10, 10, 40, 40);
  reference_ellipse_check(5, 5, 60, 30);
  reference_ellipse_check(0, 0, 11, 25);
}

void test_arc_points_start_and_end_at_the_requested_angles(void) {
  int xy[2 * 2048];
  int n = arc_point_count(40, 40, 90 * 64);
  TEST_ASSERT_TRUE(n >= 2 && n <= 2048);
  TEST_ASSERT_EQUAL_INT(n, arc_points(10, 10, 40, 40, 90 * 64, 90 * 64, xy, n));
  TEST_ASSERT_EQUAL_INT(30, xy[0]);
  TEST_ASSERT_EQUAL_INT(10, xy[1]);
  TEST_ASSERT_EQUAL_INT(10, xy[2 * (n - 1)]);
  TEST_ASSERT_EQUAL_INT(30, xy[2 * (n - 1) + 1]);
}

void test_negative_angle2_runs_clockwise(void) {
  int xy[2 * 2048];
  int n = arc_point_count(40, 40, -90 * 64);
  TEST_ASSERT_EQUAL_INT(n, arc_points(10, 10, 40, 40, 0, -90 * 64, xy, n));
  TEST_ASSERT_EQUAL_INT(50, xy[0]);
  TEST_ASSERT_EQUAL_INT(30, xy[1]);
  TEST_ASSERT_EQUAL_INT(30, xy[2 * (n - 1)]);
  TEST_ASSERT_EQUAL_INT(50, xy[2 * (n - 1) + 1]);
}

void test_angle2_above_360_degrees_is_a_full_ellipse(void) {
  TEST_ASSERT_EQUAL_INT(arc_point_count(40, 40, 360 * 64), arc_point_count(40, 40, 720 * 64));
  TEST_ASSERT_EQUAL_INT(arc_point_count(40, 40, 360 * 64), arc_point_count(40, 40, -720 * 64));
  TEST_ASSERT_EQUAL_INT(0, arc_point_count(40, 40, 0));
}

void test_draw_arc_quarter_draws_only_that_quadrant(void) {
  use_canvas(64);
  XGCValues v;
  v.foreground = 0xFFFF;
  GC gc = XCreateGC(dpy, win, GCForeground, &v);
  XDrawArc(dpy, win, gc, 10, 10, 40, 40, 0, 90 * 64);
  TEST_ASSERT_TRUE(count_set() > 20);
  for (int y = 0; y < 64; y++)
    for (int x = 0; x < 64; x++)
      if (at(x, y)) TEST_ASSERT_TRUE_MESSAGE(x >= 30 && y <= 30, "pixel outside the quadrant");
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(50, 30));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(30, 10));
  XFreeGC(dpy, gc);
}

void test_draw_arc_full_ellipse_is_a_closed_outline(void) {
  use_canvas(64);
  XGCValues v;
  v.foreground = 0xFFFF;
  GC gc = XCreateGC(dpy, win, GCForeground, &v);
  XDrawArc(dpy, win, gc, 10, 10, 40, 40, 0, 360 * 64);
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(10, 30));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(50, 30));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(30, 10));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(30, 50));
  TEST_ASSERT_EQUAL_HEX16(0, at(30, 30));
  XFreeGC(dpy, gc);
}

void test_draw_arc_wide_uses_the_stroke_code(void) {
  use_canvas(64);
  XGCValues v;
  v.foreground = 0xFFFF;
  GC thin = XCreateGC(dpy, win, GCForeground, &v);
  XDrawArc(dpy, win, thin, 10, 10, 40, 40, 0, 360 * 64);
  const int one = count_set();
  use_canvas(64);
  GC wide = wide_gc(3, CapButt, JoinRound);
  XDrawArc(dpy, win, wide, 10, 10, 40, 40, 0, 360 * 64);
  TEST_ASSERT_TRUE(count_set() > 2 * one);
  XFreeGC(dpy, thin);
  XFreeGC(dpy, wide);
}

/* A pixel well inside the band is drawn and one well outside it is not; only
 * the pixels at an edge may go either way. */
void test_stroke_circle_fills_the_band_between_its_two_radii(void) {
  use_canvas(200);
  GC gc = wide_gc(30, CapButt, JoinMiter);
  stroke_circle(&cv, gc, 20, 20, 160);
  for (int y = 0; y < cv.h; y++)
    for (int x = 0; x < cv.w; x++) {
      const float r = hypotf((float)x + 0.5f - 100.0f, (float)y + 0.5f - 100.0f);
      if (r < 93.0f && r > 67.0f)
        TEST_ASSERT_EQUAL_HEX16_MESSAGE(0xFFFF, at(x, y), "a pixel in the band is missing");
      if (r > 97.0f || r < 63.0f)
        TEST_ASSERT_EQUAL_HEX16_MESSAGE(0, at(x, y), "a pixel outside the band is set");
    }
  XFreeGC(dpy, gc);
}

/* The wide polyline round a full circle is the picture the ring stands in for.
 * It has gaps the ring does not, so it is the ring that must cover it. */
void test_stroke_circle_covers_what_the_wide_arc_draws(void) {
  use_canvas(200);
  GC gc = wide_gc(30, CapButt, JoinMiter);
  XDrawArc(dpy, win, gc, 20, 20, 160, 160, 0, 360 * 64);
  uint16_t *reference = (uint16_t *)malloc((size_t)cv.w * cv.h * sizeof(uint16_t));
  memcpy(reference, cv.px, (size_t)cv.w * cv.h * sizeof(uint16_t));
  use_canvas(200);
  stroke_circle(&cv, gc, 20, 20, 160);
  for (int y = 0; y < cv.h; y++)
    for (int x = 0; x < cv.w; x++) {
      if (reference[y * cv.w + x] == 0 || at(x, y) != 0) continue;
      const float r = hypotf((float)x + 0.5f - 100.0f, (float)y + 0.5f - 100.0f);
      TEST_ASSERT_TRUE_MESSAGE(fabsf(r - 95.0f) < 1.6f || fabsf(r - 65.0f) < 1.6f,
                               "the wide arc draws a pixel the ring leaves out");
    }
  free(reference);
  XFreeGC(dpy, gc);
}

/* Issue #56: the polyline left (57, 31), at radius 80.6, empty. */
void test_wide_full_arc_fills_every_pixel_in_its_band(void) {
  use_canvas(200);
  GC gc = wide_gc(30, CapButt, JoinMiter);
  XDrawArc(dpy, win, gc, 20, 20, 160, 160, 0, 360 * 64);
  int gaps = 0;
  for (int y = 0; y < cv.h; y++)
    for (int x = 0; x < cv.w; x++) {
      const float r = hypotf((float)x + 0.5f - 100.0f, (float)y + 0.5f - 100.0f);
      if (r < 93.0f && r > 67.0f && at(x, y) != 0xFFFF) gaps++;
    }
  TEST_ASSERT_EQUAL_INT_MESSAGE(0, gaps, "pixels inside the band are empty");
  XFreeGC(dpy, gc);
}

void test_stroke_circle_narrower_than_its_line_is_a_filled_disc(void) {
  use_canvas(64);
  GC gc = wide_gc(30, CapButt, JoinMiter);
  stroke_circle(&cv, gc, 22, 22, 20);
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(32, 32));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(32, 10));
  TEST_ASSERT_EQUAL_HEX16(0, at(2, 2));
  XFreeGC(dpy, gc);
}

void test_stroke_circle_is_clipped_to_the_canvas(void) {
  use_canvas(64);
  GC gc = wide_gc(50, CapButt, JoinMiter);
  stroke_circle(&cv, gc, -18, -18, 100);
  TEST_ASSERT_EQUAL_HEX16(0, at(32, 32));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(2, 32));
  stroke_circle(&cv, gc, -300, -300, 699);
  stroke_circle(&cv, gc, 2000000000, 2000000000, 699);
  XFreeGC(dpy, gc);
}

/* A GC with an alpha blends over what is already on the canvas. Red over blue
 * at 16/32 is halfway: about 15.5 of 31 in each of the two channels. */
static void check_halfway_red_over_blue(uint16_t stored) {
  const uint16_t v = px_swap(stored);
  const int r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31;
  TEST_ASSERT_TRUE_MESSAGE(r >= 15 && r <= 16, "red is halfway");
  TEST_ASSERT_TRUE_MESSAGE(b >= 15 && b <= 16, "blue is halfway");
  TEST_ASSERT_EQUAL_INT(0, g);
}

void test_wide_line_with_alpha_blends_over_the_canvas(void) {
  use_canvas(64);
  canvas_clear(&cv, px_swap(0x001F));
  canvas_clear_dirty(&cv);
  GC gc = wide_gc(8, CapButt, JoinMiter);
  gc->foreground = px_swap(0xF800);
  gc->alpha = 16;
  stroke_segment(&cv, gc, 10, 30, 50, 30);
  check_halfway_red_over_blue(at(30, 30));
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x001F), at(30, 50));
  int x0, x1;
  TEST_ASSERT_TRUE(canvas_dirty_row(&cv, 30, &x0, &x1));
  TEST_ASSERT_TRUE(x0 <= 10 && x1 >= 49);
  XFreeGC(dpy, gc);
}

void test_wide_line_without_alpha_still_overwrites(void) {
  use_canvas(64);
  canvas_clear(&cv, px_swap(0x001F));
  GC gc = wide_gc(8, CapButt, JoinMiter);
  gc->foreground = px_swap(0xF800);
  stroke_segment(&cv, gc, 10, 30, 50, 30);
  TEST_ASSERT_EQUAL_HEX16(px_swap(0xF800), at(30, 30));
  XFreeGC(dpy, gc);
}

void test_stroke_circle_with_alpha_blends_each_pixel_once(void) {
  use_canvas(200);
  canvas_clear(&cv, px_swap(0x001F));
  GC gc = wide_gc(30, CapButt, JoinMiter);
  gc->foreground = px_swap(0xF800);
  gc->alpha = 16;
  stroke_circle(&cv, gc, 20, 20, 160);
  check_halfway_red_over_blue(at(100, 20 + 5));
  check_halfway_red_over_blue(at(20 + 5, 100));
  XFreeGC(dpy, gc);
}

void test_alpha_does_not_leak_into_the_next_gc(void) {
  use_canvas(64);
  canvas_clear(&cv, px_swap(0x001F));
  GC blended = wide_gc(8, CapButt, JoinMiter);
  blended->foreground = px_swap(0xF800);
  blended->alpha = 16;
  stroke_segment(&cv, blended, 10, 30, 50, 30);
  GC opaque = wide_gc(8, CapButt, JoinMiter);
  opaque->foreground = px_swap(0xF800);
  stroke_segment(&cv, opaque, 10, 10, 50, 10);
  TEST_ASSERT_EQUAL_HEX16(px_swap(0xF800), at(30, 10));
  XFreeGC(dpy, blended);
  XFreeGC(dpy, opaque);
}

/* The quads of a polyline overlap at its joins and where it crosses itself,
 * and a blended overlap would show darker or brighter than the rest. */
void test_polyline_with_alpha_blends_a_join_once(void) {
  use_canvas(64);
  canvas_clear(&cv, px_swap(0x001F));
  GC gc = wide_gc(20, CapButt, JoinMiter);
  gc->foreground = px_swap(0xF800);
  gc->alpha = 16;
  const int xy[] = {10, 50, 40, 50, 40, 10};
  stroke_polyline(&cv, gc, xy, 3, 0);
  check_halfway_red_over_blue(at(25, 50));
  check_halfway_red_over_blue(at(35, 45));
  XFreeGC(dpy, gc);
}

/* Pins the pixels of many random wide segments and polylines, each drawn on a
 * cleared canvas and hashed alone: drawn together, a later stroke covers the
 * pixels where an earlier one would differ. A change to how fill_convex finds
 * each row's span shows up as a moved hash. */
void test_wide_strokes_of_random_lines_match_the_pinned_hash(void) {
  use_canvas(200);
  srandom(1);
  uint64_t h = 1469598103934665603ull;
  for (int i = 0; i < 6000; i++) {
    canvas_clear(&cv, 0);
    const int width = 2 + (int)(random() % 7);
    const int cap = (int)(random() % 3);
    const int join = (int)(random() % 3);
    GC gc = wide_gc(width, cap, join);
    gc->foreground = 0xFFFF;
    int xy[8];
    for (int k = 0; k < 8; k++) xy[k] = (int)(random() % 260) - 30;
    if (i % 2) stroke_segment(&cv, gc, xy[0], xy[1], xy[2], xy[3]);
    else stroke_polyline(&cv, gc, xy, 4, i % 3 == 0);
    XFreeGC(dpy, gc);
    for (size_t k = 0; k < (size_t)cv.w * cv.h; k++) h = (h ^ cv.px[k]) * 1099511628211ull;
  }
  TEST_ASSERT_EQUAL_UINT64(0x275c89dc98bc3901ull, h);
}

void test_polyline_with_alpha_blends_a_self_crossing_once(void) {
  use_canvas(64);
  canvas_clear(&cv, px_swap(0x001F));
  GC gc = wide_gc(8, CapButt, JoinMiter);
  gc->foreground = px_swap(0xF800);
  gc->alpha = 16;
  const int xy[] = {10, 10, 50, 50, 50, 10, 10, 50};
  stroke_polyline(&cv, gc, xy, 4, 0);
  check_halfway_red_over_blue(at(30, 30));
  check_halfway_red_over_blue(at(20, 20));
  XFreeGC(dpy, gc);
}

void test_round_capped_segment_with_alpha_blends_its_cap_once(void) {
  use_canvas(64);
  canvas_clear(&cv, px_swap(0x001F));
  GC gc = wide_gc(12, CapRound, JoinMiter);
  gc->foreground = px_swap(0xF800);
  gc->alpha = 16;
  stroke_segment(&cv, gc, 20, 30, 44, 30);
  check_halfway_red_over_blue(at(21, 30));
  check_halfway_red_over_blue(at(32, 30));
  check_halfway_red_over_blue(at(14, 30));
  check_halfway_red_over_blue(at(49, 30));
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x001F), at(13, 30));
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x001F), at(50, 30));
  XFreeGC(dpy, gc);
}

/* A disc wider than the cached rows (16) took a separate path that ignored
 * alpha, so a wide round cap stayed opaque beside a blended body. */
void test_wide_round_cap_with_alpha_blends_like_the_body(void) {
  use_canvas(64);
  canvas_clear(&cv, px_swap(0x001F));
  GC gc = wide_gc(20, CapRound, JoinMiter);
  gc->foreground = px_swap(0xF800);
  gc->alpha = 16;
  stroke_segment(&cv, gc, 24, 30, 44, 30);
  check_halfway_red_over_blue(at(32, 30));
  check_halfway_red_over_blue(at(15, 30));
  check_halfway_red_over_blue(at(24, 30));
  check_halfway_red_over_blue(at(53, 30));
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x001F), at(13, 30));
  XFreeGC(dpy, gc);
}

void test_fill_arc_half_fills_a_half_disc(void) {
  use_canvas(64);
  XGCValues v;
  v.foreground = 0xFFFF;
  GC gc = XCreateGC(dpy, win, GCForeground, &v);
  XFillArc(dpy, win, gc, 10, 10, 40, 40, 0, 180 * 64);
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(30, 15));
  TEST_ASSERT_EQUAL_HEX16(0, at(30, 45));
  const int n = count_set();
  TEST_ASSERT_TRUE_MESSAGE(n >= 565 && n <= 690, "about half of pi * 20 * 20");
  XFreeGC(dpy, gc);
}

void test_fill_arc_full_still_matches_the_old_ellipse(void) {
  use_canvas(64);
  canvas_fill_ellipse(&cv, 10, 12, 31, 25, 0xFFFF);
  uint16_t *want = snapshot();
  use_canvas(64);
  XGCValues v;
  v.foreground = 0xFFFF;
  GC gc = XCreateGC(dpy, win, GCForeground, &v);
  XFillArc(dpy, win, gc, 10, 12, 31, 25, 0, 360 * 64);
  TEST_ASSERT_EQUAL_MEMORY(want, cv.px, (size_t)64 * 64 * sizeof(uint16_t));
  free(want);
  XFreeGC(dpy, gc);
}

void test_zero_size_and_zero_sweep_arcs_draw_nothing_much(void) {
  use_canvas(64);
  XGCValues v;
  v.foreground = 0xFFFF;
  GC gc = XCreateGC(dpy, win, GCForeground, &v);
  XDrawArc(dpy, win, gc, 20, 20, 0, 0, 0, 360 * 64);
  XFillArc(dpy, win, gc, 20, 20, 0, 0, 0, 180 * 64);
  XDrawArc(dpy, win, gc, 10, 10, 40, 40, 0, 0);
  XFillArc(dpy, win, gc, 10, 10, 40, 40, 0, 0);
  TEST_ASSERT_TRUE(count_set() <= 1);
  XFreeGC(dpy, gc);
}

void test_huge_arcs_return_without_hanging_or_overflowing(void) {
  use_canvas(64);
  GC gc = wide_gc(3, CapRound, JoinRound);
  XDrawArc(dpy, win, gc, -1000000, -1000000, 65535, 65535, 0, 360 * 64);
  XDrawArc(dpy, win, gc, INT_MAX, INT_MAX, 65535, 65535, 0, 90 * 64);
  XFillArc(dpy, win, gc, -1000000, -1000000, 65535, 65535, 0, 270 * 64);
  XFillArc(dpy, win, gc, INT_MIN, INT_MIN, 65535, 65535, 0, 270 * 64);
  XFreeGC(dpy, gc);
}

void test_fill_arc_quarter_is_a_pie_slice_including_the_centre(void) {
  use_canvas(64);
  XGCValues v;
  v.foreground = 0xFFFF;
  GC gc = XCreateGC(dpy, win, GCForeground, &v);
  XFillArc(dpy, win, gc, 10, 10, 40, 40, 0, 90 * 64);
  TEST_ASSERT_EQUAL_HEX16_MESSAGE(0xFFFF, at(32, 28), "near the centre, far from the chord");
  TEST_ASSERT_EQUAL_HEX16(0, at(25, 28));
  TEST_ASSERT_EQUAL_HEX16(0, at(32, 35));
  const int n = count_set();
  TEST_ASSERT_TRUE_MESSAGE(n >= 280 && n <= 350, "about a quarter of pi * 20 * 20");
  XFreeGC(dpy, gc);
}

static int count_colour(uint16_t colour) {
  int n = 0;
  for (int i = 0; i < cv.w * cv.h; i++) n += cv.px[i] == colour;
  return n;
}

void test_create_pixmap_is_zeroed_and_reports_its_geometry(void) {
  canvas_clear(&cv, 0xFFFF);
  Pixmap p = XCreatePixmap(dpy, win, 5, 7, 16);
  TEST_ASSERT_NOT_EQUAL(None, p);
  Window root;
  int x, y;
  unsigned int w, h, border, depth;
  TEST_ASSERT_TRUE(XGetGeometry(dpy, p, &root, &x, &y, &w, &h, &border, &depth));
  TEST_ASSERT_EQUAL_UINT(5, w);
  TEST_ASSERT_EQUAL_UINT(7, h);
  TEST_ASSERT_EQUAL_UINT(16, depth);
  GC gc = new_gc(0xF800, 0x001F);
  XCopyArea(dpy, p, win, gc, 0, 0, 5, 7, 2, 2);
  TEST_ASSERT_EQUAL_INT(35, count_colour(0));
  TEST_ASSERT_EQUAL_HEX16(0, at(2, 2));
  TEST_ASSERT_EQUAL_HEX16(0, at(6, 8));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(7, 2));
  XFreePixmap(dpy, p);

  Pixmap bits = XCreatePixmap(dpy, win, 5, 7, 1);
  TEST_ASSERT_TRUE(XGetGeometry(dpy, bits, &root, &x, &y, &w, &h, &border, &depth));
  TEST_ASSERT_EQUAL_UINT(1, depth);
  canvas_clear(&cv, 0xFFFF);
  XCopyPlane(dpy, bits, win, gc, 0, 0, 5, 7, 2, 2, 1);
  TEST_ASSERT_EQUAL_INT(35, count_colour(0x001F));
  TEST_ASSERT_EQUAL_INT(0, count_colour(0xF800));
  XFreePixmap(dpy, bits);
  XFreeGC(dpy, gc);
}

void test_create_pixmap_rejects_zero_and_huge_sizes(void) {
  TEST_ASSERT_EQUAL(None, XCreatePixmap(dpy, win, 0, 5, 16));
  TEST_ASSERT_EQUAL(None, XCreatePixmap(dpy, win, 5, 0, 16));
  TEST_ASSERT_EQUAL(None, XCreatePixmap(dpy, win, 0, 0, 1));
  TEST_ASSERT_EQUAL(None, XCreatePixmap(dpy, win, 4001, 1000, 16));
  TEST_ASSERT_EQUAL(None, XCreatePixmap(dpy, win, 65535, 65535, 16));
  TEST_ASSERT_EQUAL(None, XCreatePixmap(dpy, win, 65535, 65535, 1));
}

/* Kumppa rotates the picture by copying blocks of the window onto the window,
 * one pixel over: X copies as if the whole source were read first. */
static void number_canvas(void) {
  for (int i = 0; i < cv.w * cv.h; i++) cv.px[i] = (uint16_t)(i + 1);
}

static void reference_copy(uint16_t *ref, int sx, int sy, int w, int h, int dx,
                           int dy) {
  uint16_t before[16 * 16];
  memcpy(before, cv.px, sizeof before);
  memcpy(ref, cv.px, sizeof before);
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++) {
      const int x = dx + i, y = dy + j, px = sx + i, py = sy + j;
      if (x < 0 || y < 0 || x >= 16 || y >= 16) continue;
      if (px < 0 || py < 0 || px >= 16 || py >= 16) continue;
      ref[y * 16 + x] = before[py * 16 + px];
    }
}

static void check_window_copy(int sx, int sy, int w, int h, int dx, int dy) {
  uint16_t ref[16 * 16];
  number_canvas();
  reference_copy(ref, sx, sy, w, h, dx, dy);
  GC gc = new_gc(0xFFFF, 0);
  XCopyArea(dpy, win, win, gc, sx, sy, (unsigned)w, (unsigned)h, dx, dy);
  XFreeGC(dpy, gc);
  TEST_ASSERT_EQUAL_HEX16_ARRAY(ref, cv.px, 16 * 16);
}

void test_copy_area_from_the_window_to_itself_apart(void) {
  check_window_copy(0, 0, 4, 3, 9, 10);
}

void test_copy_area_from_the_window_to_itself_overlapping_each_way(void) {
  check_window_copy(2, 2, 10, 9, 3, 3);
  check_window_copy(2, 2, 10, 9, 1, 1);
  check_window_copy(2, 2, 10, 9, 3, 1);
  check_window_copy(2, 2, 10, 9, 1, 3);
  check_window_copy(0, 0, 16, 16, 0, 1);
  check_window_copy(0, 0, 16, 16, 1, 0);
}

void test_copy_area_from_the_window_clips_at_every_edge(void) {
  check_window_copy(-3, -2, 8, 8, 12, 11);
  check_window_copy(10, 11, 9, 9, -4, -5);
  check_window_copy(0, 0, 0xFFFF, 0xFFFF, 2, 2);
}

void test_copy_area_from_the_window_marks_the_rows_it_wrote(void) {
  number_canvas();
  canvas_clear_dirty(&cv);
  GC gc = new_gc(0xFFFF, 0);
  XCopyArea(dpy, win, win, gc, 0, 0, 3, 2, 5, 7);
  XFreeGC(dpy, gc);
  int x0, x1;
  TEST_ASSERT_FALSE(canvas_dirty_row(&cv, 6, &x0, &x1));
  TEST_ASSERT_TRUE(canvas_dirty_row(&cv, 7, &x0, &x1));
  TEST_ASSERT_EQUAL_INT(5, x0);
  TEST_ASSERT_EQUAL_INT(7, x1);
  TEST_ASSERT_TRUE(canvas_dirty_row(&cv, 8, &x0, &x1));
  TEST_ASSERT_FALSE(canvas_dirty_row(&cv, 9, &x0, &x1));
}

void test_copy_area_from_the_window_into_a_pixmap_and_back(void) {
  number_canvas();
  uint16_t before[16 * 16];
  memcpy(before, cv.px, sizeof before);
  Pixmap p = XCreatePixmap(dpy, win, 4, 4, 16);
  GC gc = new_gc(0xFFFF, 0);
  XCopyArea(dpy, win, p, gc, 5, 6, 4, 4, 0, 0);
  memset(cv.px, 0, sizeof before);
  XCopyArea(dpy, p, win, gc, 0, 0, 4, 4, 1, 2);
  TEST_ASSERT_EQUAL_HEX16(before[6 * 16 + 5], at(1, 2));
  TEST_ASSERT_EQUAL_HEX16(before[9 * 16 + 8], at(4, 5));
  TEST_ASSERT_EQUAL_INT(16, count_set());
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, p);
}

void test_copy_area_into_a_colour_pixmap_then_out_to_the_canvas(void) {
  Pixmap mask = None;
  Pixmap src = load_image(&mask);
  Pixmap dst = XCreatePixmap(dpy, win, 6, 5, 16);
  GC gc = new_gc(0xFFFF, 0);
  XCopyArea(dpy, src, dst, gc, 0, 0, BLOB_W, BLOB_H, 1, 1);
  TEST_ASSERT_EQUAL_INT(0, count_set());
  XCopyArea(dpy, dst, win, gc, 0, 0, 6, 5, 3, 3);
  TEST_ASSERT_EQUAL_INT(BLOB_W * BLOB_H, count_set());
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x1000), at(4, 4));
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x100B), at(7, 6));
  TEST_ASSERT_EQUAL_HEX16(0, at(3, 3));
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, src);
  XFreePixmap(dpy, mask);
  XFreePixmap(dpy, dst);
}

void test_copy_area_into_a_bitmap_copies_bits(void) {
  Pixmap mask = None;
  Pixmap src = load_image(&mask);
  Pixmap bits = XCreatePixmap(dpy, win, 8, 5, 1);
  GC gc = new_gc(0xF800, 0x001F);
  XCopyArea(dpy, mask, bits, gc, 0, 0, BLOB_W, BLOB_H, 2, 1);
  XCopyPlane(dpy, bits, win, gc, 0, 0, 8, 5, 0, 0, 1);
  TEST_ASSERT_EQUAL_INT(MASK_SET_BITS, count_colour(0xF800));
  TEST_ASSERT_EQUAL_HEX16(0xF800, at(2, 1));
  TEST_ASSERT_EQUAL_HEX16(0x001F, at(3, 2));
  TEST_ASSERT_EQUAL_HEX16(0xF800, at(4, 2));
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, src);
  XFreePixmap(dpy, mask);
  XFreePixmap(dpy, bits);
}

void test_copy_area_between_different_depths_does_nothing(void) {
  Pixmap mask = None;
  Pixmap src = load_image(&mask);
  Pixmap bits = XCreatePixmap(dpy, win, 8, 5, 1);
  Pixmap colour = XCreatePixmap(dpy, win, 8, 5, 16);
  GC gc = new_gc(0xF800, 0x001F);
  XCopyArea(dpy, src, bits, gc, 0, 0, BLOB_W, BLOB_H, 0, 0);
  XCopyPlane(dpy, bits, win, gc, 0, 0, 8, 5, 0, 0, 1);
  TEST_ASSERT_EQUAL_INT(0, count_colour(0xF800));
  XCopyArea(dpy, mask, colour, gc, 0, 0, BLOB_W, BLOB_H, 0, 0);
  canvas_clear(&cv, 0xFFFF);
  XCopyArea(dpy, colour, win, gc, 0, 0, 8, 5, 0, 0);
  TEST_ASSERT_EQUAL_INT(0, count_colour(0xF800));
  TEST_ASSERT_EQUAL_INT(0, count_colour(px_swap(0x1000)));
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, src);
  XFreePixmap(dpy, mask);
  XFreePixmap(dpy, bits);
  XFreePixmap(dpy, colour);
}

void test_copy_area_clips_at_destination_edges(void) {
  Pixmap mask = None;
  Pixmap src = load_image(&mask);
  Pixmap dst = XCreatePixmap(dpy, win, 4, 4, 16);
  GC gc = new_gc(0xFFFF, 0);
  XCopyArea(dpy, src, dst, gc, 0, 0, BLOB_W, BLOB_H, 2, 2);
  XCopyArea(dpy, dst, win, gc, 0, 0, 4, 4, 0, 0);
  TEST_ASSERT_EQUAL_INT(4, count_set());
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x1000), at(2, 2));
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x1005), at(3, 3));
  XFreePixmap(dpy, dst);

  dst = XCreatePixmap(dpy, win, 4, 4, 16);
  canvas_clear(&cv, 0);
  XCopyArea(dpy, src, dst, gc, 0, 0, BLOB_W, BLOB_H, -2, -1);
  XCopyArea(dpy, dst, win, gc, 0, 0, 4, 4, 0, 0);
  TEST_ASSERT_EQUAL_INT(4, count_set());
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x1006), at(0, 0));
  XCopyArea(dpy, src, dst, gc, 0, 0, BLOB_W, BLOB_H, 1000000, -1000000);
  XCopyArea(dpy, src, dst, gc, 0, 0, 0xFFFFFFFFu, 0xFFFFFFFFu, INT_MIN, INT_MIN);
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, src);
  XFreePixmap(dpy, mask);
  XFreePixmap(dpy, dst);
}

void test_copy_area_with_a_clip_mask_skips_masked_pixels(void) {
  Pixmap mask = None;
  Pixmap src = load_image(&mask);
  Pixmap dst = XCreatePixmap(dpy, win, 6, 5, 16);
  GC gc = new_gc(0xFFFF, 0);
  XSetClipMask(dpy, gc, mask);
  XCopyArea(dpy, src, dst, gc, 0, 0, BLOB_W, BLOB_H, 0, 0);
  XSetClipMask(dpy, gc, None);
  XCopyArea(dpy, dst, win, gc, 0, 0, 6, 5, 0, 0);
  TEST_ASSERT_EQUAL_INT(MASK_SET_BITS, count_set());
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, src);
  XFreePixmap(dpy, mask);
  XFreePixmap(dpy, dst);
}

void test_scale_pixmap_loop_like_pacman_fills_the_destination(void) {
  Pixmap mask = None;
  Pixmap src = load_image(&mask);
  GC gc = new_gc(0xFFFF, 0);
  const int dwidth = 8, dheight = 6, swidth = BLOB_W, sheight = BLOB_H;
  const float xscale = (float)swidth / (float)dwidth;
  const float yscale = (float)sheight / (float)dheight;
  Pixmap dest = XCreatePixmap(dpy, win, dwidth, dheight, 16);
  Pixmap temp = XCreatePixmap(dpy, win, dwidth, sheight, 16);
  int j = 0;
  int end = dwidth * xscale;
  for (float i = 0; i <= end; i += xscale)
    XCopyArea(dpy, src, temp, gc, i, 0, 1, sheight, j++, 0);
  j = 0;
  end = dheight * yscale;
  for (float i = 0; i <= end; i += yscale)
    XCopyArea(dpy, temp, dest, gc, 0, i, dwidth, 1, 0, j++);
  XFreePixmap(dpy, temp);
  XCopyArea(dpy, dest, win, gc, 0, 0, dwidth, dheight, 0, 0);
  TEST_ASSERT_EQUAL_INT(dwidth * dheight, count_set());
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x1000), at(0, 0));
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x1006), at(5, 3));
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x100B), at(7, 5));
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, src);
  XFreePixmap(dpy, mask);
  XFreePixmap(dpy, dest);
}

void test_fill_rectangle_into_a_pixmap_lands_in_the_pixmap(void) {
  Pixmap p = XCreatePixmap(dpy, win, 8, 8, 16);
  GC gc = new_gc(0xFFFF, 0);
  XFillRectangle(dpy, p, gc, 1, 1, 2, 2);
  TEST_ASSERT_EQUAL_INT(0, count_set());
  XCopyArea(dpy, p, win, gc, 0, 0, 8, 8, 0, 0);
  TEST_ASSERT_EQUAL_INT(4, count_set());
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(1, 1));
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, p);
}

static void draw_point(Drawable d, GC gc) { XDrawPoint(dpy, d, gc, 3, 4); }
static void draw_points(Drawable d, GC gc) {
  XPoint pts[] = {{1, 1}, {5, 9}, {14, 2}};
  XDrawPoints(dpy, d, gc, pts, 3, CoordModeOrigin);
}
static void draw_thin_line(Drawable d, GC gc) { XDrawLine(dpy, d, gc, 0, 2, 15, 11); }
static void draw_wide_line(Drawable d, GC gc) {
  XSetLineAttributes(dpy, gc, 5, LineSolid, CapRound, JoinRound);
  XDrawLine(dpy, d, gc, 2, 3, 12, 10);
}
static void draw_segments(Drawable d, GC gc) {
  XSegment segs[] = {{0, 0, 7, 7}, {9, 1, 9, 14}};
  XDrawSegments(dpy, d, gc, segs, 2);
}
static void draw_thin_lines(Drawable d, GC gc) {
  XPoint pts[] = {{1, 1}, {12, 3}, {6, 14}};
  XDrawLines(dpy, d, gc, pts, 3, CoordModeOrigin);
}
static void draw_wide_lines(Drawable d, GC gc) {
  XSetLineAttributes(dpy, gc, 3, LineSolid, CapButt, JoinMiter);
  XPoint pts[] = {{1, 1}, {12, 3}, {6, 14}};
  XDrawLines(dpy, d, gc, pts, 3, CoordModeOrigin);
}
static void draw_thin_rectangle(Drawable d, GC gc) { XDrawRectangle(dpy, d, gc, 2, 3, 9, 7); }
static void draw_wide_rectangle(Drawable d, GC gc) {
  XSetLineAttributes(dpy, gc, 3, LineSolid, CapButt, JoinMiter);
  XDrawRectangle(dpy, d, gc, 2, 3, 9, 7);
}
static void fill_rectangles(Drawable d, GC gc) {
  XRectangle r[] = {{1, 1, 3, 4}, {8, 6, 5, 2}};
  XFillRectangles(dpy, d, gc, r, 2);
}
static void draw_thin_arc(Drawable d, GC gc) { XDrawArc(dpy, d, gc, 1, 2, 12, 10, 0, 360 * 64); }
static void draw_wide_quarter_arc(Drawable d, GC gc) {
  XSetLineAttributes(dpy, gc, 4, LineSolid, CapRound, JoinRound);
  XDrawArc(dpy, d, gc, -6, -6, 20, 20, 270 * 64, 90 * 64);
}
static void fill_full_arc(Drawable d, GC gc) { XFillArc(dpy, d, gc, 2, 1, 11, 13, 0, 360 * 64); }
static void fill_partial_arc(Drawable d, GC gc) { XFillArc(dpy, d, gc, 1, 1, 14, 14, 30 * 64, 200 * 64); }
static void fill_polygon(Drawable d, GC gc) {
  XPoint pts[] = {{2, 1}, {14, 5}, {7, 15}, {1, 9}};
  XFillPolygon(dpy, d, gc, pts, 4, Complex, CoordModeOrigin);
}

void test_every_primitive_draws_the_same_into_a_pixmap_as_on_the_window(void) {
  void (*const draw[])(Drawable, GC) = {
      draw_point,          draw_points,         draw_thin_line,
      draw_wide_line,      draw_segments,       draw_thin_lines,
      draw_wide_lines,     draw_thin_rectangle, draw_wide_rectangle,
      fill_rectangles,     draw_thin_arc,       draw_wide_quarter_arc,
      fill_full_arc,       fill_partial_arc,    fill_polygon};
  uint16_t on_window[16 * 16];
  for (size_t i = 0; i < sizeof draw / sizeof draw[0]; i++) {
    canvas_clear(&cv, 0);
    GC gc = new_gc(0xF81F, 0);
    draw[i](win, gc);
    TEST_ASSERT_TRUE_MESSAGE(count_set() > 0, "shape drew nothing on the window");
    memcpy(on_window, cv.px, sizeof on_window);
    XFreeGC(dpy, gc);

    canvas_clear(&cv, 0);
    gc = new_gc(0xF81F, 0);
    Pixmap p = XCreatePixmap(dpy, win, (unsigned)cv.w, (unsigned)cv.h, 16);
    draw[i](p, gc);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, count_set(), "shape landed on the window");
    XCopyArea(dpy, p, win, gc, 0, 0, (unsigned)cv.w, (unsigned)cv.h, 0, 0);
    TEST_ASSERT_EQUAL_MEMORY(on_window, cv.px, sizeof on_window);
    XFreePixmap(dpy, p);
    XFreeGC(dpy, gc);
  }
}

void test_drawing_into_a_pixmap_clips_at_the_pixmap_edges(void) {
  GC gc = new_gc(0xFFFF, 0);
  Pixmap small = XCreatePixmap(dpy, win, 10, 6, 16);
  XFillRectangle(dpy, small, gc, -5, -5, 100, 100);
  XSetLineAttributes(dpy, gc, 3, LineSolid, CapRound, JoinRound);
  XDrawLine(dpy, small, gc, -50, 3, 50, 3);
  XCopyArea(dpy, small, win, gc, 0, 0, 10, 6, 0, 0);
  TEST_ASSERT_EQUAL_INT(60, count_set());
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(9, 5));
  TEST_ASSERT_EQUAL_HEX16(0, at(10, 5));
  TEST_ASSERT_EQUAL_HEX16(0, at(9, 6));
  XFreePixmap(dpy, small);

  canvas_clear(&cv, 0);
  Pixmap big = XCreatePixmap(dpy, win, (unsigned)cv.w + 40, (unsigned)cv.h + 40, 16);
  XFillRectangle(dpy, big, gc, cv.w + 10, 0, 5, 5);
  TEST_ASSERT_EQUAL_INT(0, count_set());
  XCopyArea(dpy, big, win, gc, cv.w + 10, 0, 5, 5, 0, 0);
  TEST_ASSERT_EQUAL_INT(25, count_set());
  XFreePixmap(dpy, big);
  XFreeGC(dpy, gc);
}

void test_primitives_on_none_draw_nothing_and_a_bitmap_line_stays_off_the_window(void) {
  GC gc = new_gc(0xF800, 0x001F);
  Pixmap bits = XCreatePixmap(dpy, win, 8, 8, 1);
  GC one = bitmap_gc(bits, 1);
  XDrawLine(dpy, bits, one, 0, 0, 7, 7);
  XFillRectangle(dpy, None, gc, 0, 0, 8, 8);
  XDrawLine(dpy, None, gc, 0, 0, 7, 7);
  XFillArc(dpy, None, gc, 0, 0, 8, 8, 0, 360 * 64);
  TEST_ASSERT_EQUAL_INT(0, count_set());
  XCopyPlane(dpy, bits, win, gc, 0, 0, 8, 8, 0, 0, 1);
  TEST_ASSERT_EQUAL_INT(56, count_colour(0x001F));
  TEST_ASSERT_EQUAL_INT(8, count_colour(0xF800));
  XFreePixmap(dpy, bits);
  XFreeGC(dpy, one);
  XFreeGC(dpy, gc);
}

/* Reads a bitmap back through the window, as the tests above do. */
static void expect_bits(Pixmap bitmap, const char *rows[], int n) {
  stamp_bitmap(bitmap);
  for (int y = 0; y < n; y++)
    for (int x = 0; rows[y][x]; x++)
      TEST_ASSERT_EQUAL_HEX16_MESSAGE(rows[y][x] == '#' ? 0xFFFF : 0, at(x, y),
                                      rows[y]);
}

void test_a_thin_line_on_a_bitmap_sets_every_pixel_of_the_diagonal(void) {
  Pixmap p = XCreatePixmap(dpy, win, 16, 16, 1);
  GC clear = bitmap_gc(p, 0), one = bitmap_gc(p, 1);
  XFillRectangle(dpy, p, clear, 0, 0, 16, 16);
  XDrawLine(dpy, p, one, 1, 1, 6, 6);
  stamp_bitmap(p);
  TEST_ASSERT_EQUAL_INT(6, count_set());
  for (int i = 1; i <= 6; i++) TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(i, i));
  XFreeGC(dpy, clear);
  XFreeGC(dpy, one);
  XFreePixmap(dpy, p);
}

void test_a_thin_line_on_a_bitmap_with_foreground_zero_clears_bits_and_clips(void) {
  Pixmap p = XCreatePixmap(dpy, win, 16, 16, 1);
  GC clear = bitmap_gc(p, 0), one = bitmap_gc(p, 1);
  XFillRectangle(dpy, p, one, 0, 0, 16, 16);
  XDrawLine(dpy, p, clear, -20, 4, 40, 4);
  stamp_bitmap(p);
  TEST_ASSERT_EQUAL_INT(240, count_set());
  TEST_ASSERT_EQUAL_HEX16(0, at(0, 4));
  TEST_ASSERT_EQUAL_HEX16(0, at(15, 4));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(7, 3));
  XFreeGC(dpy, clear);
  XFreeGC(dpy, one);
  XFreePixmap(dpy, p);
}

void test_a_wide_round_capped_line_on_a_bitmap_is_a_band_with_rounded_ends(void) {
  Pixmap p = XCreatePixmap(dpy, win, 16, 16, 1);
  GC clear = bitmap_gc(p, 0), one = bitmap_gc(p, 1);
  XFillRectangle(dpy, p, clear, 0, 0, 16, 16);
  XSetLineAttributes(dpy, one, 4, LineSolid, CapRound, JoinRound);
  XDrawLine(dpy, p, one, 4, 6, 10, 6);
  const char *rows[] = {
      "................", "................", "................", "................",
      "...########.....", "..##########....", "..##########....", "...########.....",
      "................", "................"};
  expect_bits(p, rows, 10);
  stamp_bitmap(p);
  TEST_ASSERT_EQUAL_HEX16(0, at(1, 5));
  TEST_ASSERT_EQUAL_HEX16(0, at(12, 5));
  TEST_ASSERT_EQUAL_HEX16(0, at(2, 4));
  TEST_ASSERT_EQUAL_HEX16(0, at(6, 3));
  TEST_ASSERT_EQUAL_HEX16(0, at(6, 8));
  XFreeGC(dpy, clear);
  XFreeGC(dpy, one);
  XFreePixmap(dpy, p);
}

void test_thin_lines_on_a_bitmap_run_in_any_direction_and_hit_both_endpoints(void) {
  const int lines[][4] = {{7, 3, 1, 3}, {2, 0, 2, 7}, {2, 7, 2, 0}, {0, 0, 7, 3},
                          {7, 3, 0, 0}, {1, 6, 6, 1}, {3, 3, 3, 3}};
  const int counts[] = {7, 8, 8, 8, 8, 6, 1};
  for (size_t i = 0; i < sizeof lines / sizeof lines[0]; i++) {
    Pixmap p = XCreatePixmap(dpy, win, 16, 16, 1);
    GC clear = bitmap_gc(p, 0), one = bitmap_gc(p, 1);
    XFillRectangle(dpy, p, clear, 0, 0, 16, 16);
    XDrawLine(dpy, p, one, lines[i][0], lines[i][1], lines[i][2], lines[i][3]);
    stamp_bitmap(p);
    TEST_ASSERT_EQUAL_INT_MESSAGE(counts[i], count_set(), "pixels in the line");
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(lines[i][0], lines[i][1]));
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(lines[i][2], lines[i][3]));
    XFreeGC(dpy, clear);
    XFreeGC(dpy, one);
    XFreePixmap(dpy, p);
  }
}

void test_a_shallow_thin_line_on_a_bitmap_rounds_halves_up(void) {
  Pixmap p = XCreatePixmap(dpy, win, 16, 16, 1);
  GC clear = bitmap_gc(p, 0), one = bitmap_gc(p, 1);
  XFillRectangle(dpy, p, clear, 0, 0, 16, 16);
  XDrawLine(dpy, p, one, 0, 0, 7, 3);
  const char *rows[] = {"##......", "..##....", "....##..", "......##"};
  expect_bits(p, rows, 4);
  XFreeGC(dpy, clear);
  XFreeGC(dpy, one);
  XFreePixmap(dpy, p);
}

void test_a_wide_odd_width_line_on_a_bitmap_is_centred_on_its_pixel_row(void) {
  Pixmap p = XCreatePixmap(dpy, win, 16, 16, 1);
  GC clear = bitmap_gc(p, 0), one = bitmap_gc(p, 1);
  XFillRectangle(dpy, p, clear, 0, 0, 16, 16);
  XSetLineAttributes(dpy, one, 3, LineSolid, CapButt, JoinMiter);
  XDrawLine(dpy, p, one, 4, 6, 10, 6);
  stamp_bitmap(p);
  TEST_ASSERT_EQUAL_INT(18, count_set());
  for (int y = 5; y <= 7; y++) TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(6, y));
  TEST_ASSERT_EQUAL_HEX16(0, at(6, 4));
  TEST_ASSERT_EQUAL_HEX16(0, at(6, 8));
  XFreeGC(dpy, clear);
  XFreeGC(dpy, one);
  XFreePixmap(dpy, p);
}

void test_a_wide_projecting_line_on_a_bitmap_reaches_half_a_width_past_each_end(void) {
  Pixmap p = XCreatePixmap(dpy, win, 16, 16, 1);
  GC clear = bitmap_gc(p, 0), one = bitmap_gc(p, 1);
  XFillRectangle(dpy, p, clear, 0, 0, 16, 16);
  XSetLineAttributes(dpy, one, 2, LineSolid, CapProjecting, JoinMiter);
  XDrawLine(dpy, p, one, 4, 6, 10, 6);
  stamp_bitmap(p);
  TEST_ASSERT_EQUAL_INT(16, count_set());
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(3, 5));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(10, 6));
  TEST_ASSERT_EQUAL_HEX16(0, at(2, 5));
  TEST_ASSERT_EQUAL_HEX16(0, at(11, 6));
  XFreeGC(dpy, clear);
  XFreeGC(dpy, one);
  XFreePixmap(dpy, p);
}

void test_a_wide_butt_capped_line_on_a_bitmap_ends_square_at_its_endpoints(void) {
  Pixmap p = XCreatePixmap(dpy, win, 16, 16, 1);
  GC clear = bitmap_gc(p, 0), one = bitmap_gc(p, 1);
  XFillRectangle(dpy, p, clear, 0, 0, 16, 16);
  XSetLineAttributes(dpy, one, 2, LineSolid, CapButt, JoinMiter);
  XDrawLine(dpy, p, one, 4, 6, 10, 6);
  stamp_bitmap(p);
  TEST_ASSERT_EQUAL_INT(12, count_set());
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(4, 5));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(9, 6));
  TEST_ASSERT_EQUAL_HEX16(0, at(3, 5));
  TEST_ASSERT_EQUAL_HEX16(0, at(10, 6));
  XFreeGC(dpy, clear);
  XFreeGC(dpy, one);
  XFreePixmap(dpy, p);
}

void test_a_wide_line_on_a_bitmap_clips_at_the_bitmap_edges(void) {
  Pixmap p = XCreatePixmap(dpy, win, 8, 8, 1);
  GC clear = bitmap_gc(p, 0), one = bitmap_gc(p, 1);
  XFillRectangle(dpy, p, clear, 0, 0, 8, 8);
  XSetLineAttributes(dpy, one, 4, LineSolid, CapRound, JoinRound);
  XDrawLine(dpy, p, one, -50, 3, 50, 3);
  stamp_bitmap(p);
  TEST_ASSERT_EQUAL_INT(32, count_set());
  XFreeGC(dpy, clear);
  XFreeGC(dpy, one);
  XFreePixmap(dpy, p);
}

void test_get_image_of_a_bitmap_gives_a_one_bit_image_of_its_bits(void) {
  Pixmap p = XCreatePixmap(dpy, win, 16, 8, 1);
  GC clear = bitmap_gc(p, 0), one = bitmap_gc(p, 1);
  XFillRectangle(dpy, p, clear, 0, 0, 16, 8);
  XFillRectangle(dpy, p, one, 3, 2, 2, 1);
  XFillRectangle(dpy, p, one, 15, 7, 1, 1);
  XImage *im = XGetImage(dpy, p, 0, 0, 16, 8, 1, XYPixmap);
  TEST_ASSERT_NOT_NULL(im);
  TEST_ASSERT_EQUAL_INT(1, im->depth);
  TEST_ASSERT_EQUAL_INT(1, im->bits_per_pixel);
  TEST_ASSERT_EQUAL_INT(16, im->width);
  TEST_ASSERT_EQUAL_INT(8, im->height);
  int set = 0;
  for (int y = 0; y < 8; y++)
    for (int x = 0; x < 16; x++) set += (int)XGetPixel(im, x, y);
  TEST_ASSERT_EQUAL_INT(3, set);
  TEST_ASSERT_EQUAL_HEX64(1, XGetPixel(im, 3, 2));
  TEST_ASSERT_EQUAL_HEX64(1, XGetPixel(im, 4, 2));
  TEST_ASSERT_EQUAL_HEX64(1, XGetPixel(im, 15, 7));
  TEST_ASSERT_EQUAL_HEX64(0, XGetPixel(im, 5, 2));
  XDestroyImage(im);

  im = XGetImage(dpy, p, 3, 2, 4, 2, 1, XYPixmap);
  TEST_ASSERT_NOT_NULL(im);
  TEST_ASSERT_EQUAL_HEX64(1, XGetPixel(im, 0, 0));
  TEST_ASSERT_EQUAL_HEX64(1, XGetPixel(im, 1, 0));
  TEST_ASSERT_EQUAL_HEX64(0, XGetPixel(im, 2, 0));
  XDestroyImage(im);

  im = XGetImage(dpy, p, 14, 6, 4, 4, 1, XYPixmap);
  TEST_ASSERT_NOT_NULL(im);
  TEST_ASSERT_EQUAL_HEX64(1, XGetPixel(im, 1, 1));
  TEST_ASSERT_EQUAL_HEX64(0, XGetPixel(im, 3, 3));
  XDestroyImage(im);
  TEST_ASSERT_NULL(XGetImage(dpy, p, 0, 0, 4, 4, 1, ZPixmap));
  XFreeGC(dpy, clear);
  XFreeGC(dpy, one);
  XFreePixmap(dpy, p);
}

void test_put_image_of_a_bitmap_onto_a_bitmap_uses_the_low_bit_of_each_colour(void) {
  XImage *im = XCreateImage(dpy, NULL, 1, XYBitmap, 0, NULL, 4, 1, 8, 0);
  im->data = (char *)calloc(1, (size_t)im->bytes_per_line);
  XPutPixel(im, 1, 0, 1);
  XPutPixel(im, 3, 0, 1);
  Pixmap p = XCreatePixmap(dpy, win, 16, 16, 1);
  GC clear = bitmap_gc(p, 0), one = bitmap_gc(p, 1);
  XFillRectangle(dpy, p, clear, 0, 0, 16, 16);
  XSetBackground(dpy, one, 0);
  XPutImage(dpy, p, one, im, 0, 0, 5, 6, 4, 1);
  stamp_bitmap(p);
  TEST_ASSERT_EQUAL_INT(2, count_set());
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(6, 6));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(8, 6));
  XFillRectangle(dpy, p, one, 0, 0, 16, 16);
  XPutImage(dpy, p, clear, im, 0, 0, 5, 6, 4, 1);
  XSetBackground(dpy, clear, 1);
  XPutImage(dpy, p, clear, im, 0, 0, 5, 7, 4, 1);
  stamp_bitmap(p);
  TEST_ASSERT_EQUAL_HEX16(0, at(5, 6));
  TEST_ASSERT_EQUAL_HEX16(0, at(6, 6));
  TEST_ASSERT_EQUAL_HEX16(0, at(8, 6));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(5, 7));
  TEST_ASSERT_EQUAL_HEX16(0, at(6, 7));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(7, 7));
  TEST_ASSERT_EQUAL_HEX16(0, at(8, 7));
  XFreeGC(dpy, clear);
  XFreeGC(dpy, one);
  XFreePixmap(dpy, p);
  XDestroyImage(im);
}

void test_copy_plane_between_bitmaps_maps_set_bits_to_foreground_and_clear_to_background(void) {
  Pixmap src = XCreatePixmap(dpy, win, 8, 8, 1);
  Pixmap dst = XCreatePixmap(dpy, win, 8, 8, 1);
  GC clear = bitmap_gc(src, 0), one = bitmap_gc(src, 1);
  XFillRectangle(dpy, src, clear, 0, 0, 8, 8);
  XFillRectangle(dpy, src, one, 1, 1, 2, 2);
  XFillRectangle(dpy, dst, clear, 0, 0, 8, 8);
  XSetBackground(dpy, one, 0);
  XCopyPlane(dpy, src, dst, one, 0, 0, 4, 4, 2, 2, 1);
  stamp_bitmap(dst);
  TEST_ASSERT_EQUAL_INT(4, count_set());
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(3, 3));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(4, 4));
  TEST_ASSERT_EQUAL_HEX16(0, at(2, 2));

  XFillRectangle(dpy, dst, clear, 0, 0, 8, 8);
  XSetForeground(dpy, clear, 0);
  XSetBackground(dpy, clear, 1);
  XCopyPlane(dpy, src, dst, clear, 0, 0, 4, 4, 0, 0, 1);
  stamp_bitmap(dst);
  TEST_ASSERT_EQUAL_INT(16 - 4, count_set());
  TEST_ASSERT_EQUAL_HEX16(0, at(1, 1));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(0, 0));
  XFreeGC(dpy, clear);
  XFreeGC(dpy, one);
  XFreePixmap(dpy, src);
  XFreePixmap(dpy, dst);
}

void test_clipped_fill_draws_only_where_the_mask_has_bits(void) {
  Pixmap mask = XCreatePixmap(dpy, win, 4, 4, 1);
  GC clear = bitmap_gc(mask, 0), one = bitmap_gc(mask, 1);
  XFillRectangle(dpy, mask, clear, 0, 0, 4, 4);
  XFillRectangle(dpy, mask, one, 1, 1, 1, 1);
  XFillRectangle(dpy, mask, one, 2, 2, 2, 1);
  GC gc = new_gc(0xF81F, 0);
  canvas_clear(&cv, 0);
  XSetClipMask(dpy, gc, mask);
  XSetClipOrigin(dpy, gc, 3, 5);
  TEST_ASSERT_TRUE(xshim_fill_rect_clipped(dpy, win, gc, 0, 0, 16, 16));
  TEST_ASSERT_EQUAL_INT(3, count_set());
  TEST_ASSERT_EQUAL_HEX16(0xF81F, at(4, 6));
  TEST_ASSERT_EQUAL_HEX16(0xF81F, at(5, 7));
  TEST_ASSERT_EQUAL_HEX16(0xF81F, at(6, 7));
  XSetClipMask(dpy, gc, None);
  TEST_ASSERT_FALSE(xshim_fill_rect_clipped(dpy, win, gc, 0, 0, 16, 16));
  TEST_ASSERT_EQUAL_INT(3, count_set());
  XFreeGC(dpy, gc);
  XFreeGC(dpy, clear);
  XFreeGC(dpy, one);
  XFreePixmap(dpy, mask);
}

void test_clipped_fill_clips_to_the_canvas_and_the_rectangle(void) {
  Pixmap mask = XCreatePixmap(dpy, win, 16, 16, 1);
  GC one = bitmap_gc(mask, 1);
  XFillRectangle(dpy, mask, one, 0, 0, 16, 16);
  GC gc = new_gc(0xF81F, 0);
  canvas_clear(&cv, 0);
  XSetClipMask(dpy, gc, mask);
  XSetClipOrigin(dpy, gc, 8, 8);
  xshim_fill_rect_clipped(dpy, win, gc, 4, 4, 8, 8);
  TEST_ASSERT_EQUAL_INT(16, count_set());
  xshim_fill_rect_clipped(dpy, win, gc, -100, -100, 100000, 100000);
  TEST_ASSERT_EQUAL_INT(64, count_set());
  XFreeGC(dpy, gc);
  XFreeGC(dpy, one);
  XFreePixmap(dpy, mask);
}

void test_copy_plane_between_bitmaps_honours_the_clip_mask(void) {
  Pixmap src = XCreatePixmap(dpy, win, 8, 8, 1);
  Pixmap dst = XCreatePixmap(dpy, win, 8, 8, 1);
  Pixmap mask = XCreatePixmap(dpy, win, 2, 2, 1);
  GC clear = bitmap_gc(src, 0), one = bitmap_gc(src, 1);
  XFillRectangle(dpy, src, one, 0, 0, 8, 8);
  XFillRectangle(dpy, dst, clear, 0, 0, 8, 8);
  XFillRectangle(dpy, mask, clear, 0, 0, 2, 2);
  XFillRectangle(dpy, mask, one, 1, 0, 1, 1);
  XSetBackground(dpy, one, 0);
  XSetClipMask(dpy, one, mask);
  XSetClipOrigin(dpy, one, 3, 4);
  XCopyPlane(dpy, src, dst, one, 0, 0, 8, 8, 0, 0, 1);
  stamp_bitmap(dst);
  TEST_ASSERT_EQUAL_INT(1, count_set());
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(4, 4));
  XFreeGC(dpy, clear);
  XFreeGC(dpy, one);
  XFreePixmap(dpy, src);
  XFreePixmap(dpy, dst);
  XFreePixmap(dpy, mask);
}

void test_copy_plane_into_a_colour_pixmap_lands_in_the_pixmap(void) {
  GC gc = new_gc(0xF800, 0x001F);
  Pixmap bits = XCreatePixmap(dpy, win, 4, 2, 1);
  Pixmap colour = XCreatePixmap(dpy, win, 8, 8, 16);
  XCopyPlane(dpy, bits, colour, gc, 0, 0, 4, 2, 1, 1, 1);
  TEST_ASSERT_EQUAL_INT(0, count_set());
  XCopyArea(dpy, colour, win, gc, 0, 0, 8, 8, 0, 0);
  TEST_ASSERT_EQUAL_INT(8, count_colour(0x001F));
  TEST_ASSERT_EQUAL_HEX16(0x001F, at(1, 1));
  TEST_ASSERT_EQUAL_HEX16(0, at(0, 0));

  canvas_clear(&cv, 0);
  Pixmap mask = None;
  Pixmap image = load_image(&mask);
  XCopyPlane(dpy, mask, None, gc, 0, 0, BLOB_W, BLOB_H, 0, 0, 1);
  TEST_ASSERT_EQUAL_INT(0, count_set());
  XFreePixmap(dpy, image);
  XFreePixmap(dpy, mask);
  XFreePixmap(dpy, colour);
  XFreePixmap(dpy, bits);
  XFreeGC(dpy, gc);
}

void test_root_window_is_the_window_the_shim_draws_on(void) {
  const Window root = RootWindowOfScreen(NULL);
  unsigned int w = 0, h = 0;
  TEST_ASSERT_TRUE(XGetGeometry(dpy, root, NULL, NULL, NULL, &w, &h, NULL, NULL));
  TEST_ASSERT_EQUAL_UINT(cv.w, w);
  GC gc = new_gc(0xFFFF, 0);
  XFillRectangle(dpy, root, gc, 0, 0, 2, 2);
  TEST_ASSERT_EQUAL_INT(4, count_set());
  XFreeGC(dpy, gc);
}

void test_create_pixmap_wider_than_int16_returns_none(void) {
  TEST_ASSERT_EQUAL(None, XCreatePixmap(dpy, win, 40000, 2, 16));
  TEST_ASSERT_EQUAL(None, XCreatePixmap(dpy, win, 2, 40000, 16));
}

void test_pixmaps_drawn_into_leave_no_memory_behind(void) {
  const size_t before = __sanitizer_get_current_allocated_bytes();
  for (int i = 0; i < 50; i++) {
    GC gc = new_gc(0xFFFF, 0);
    Pixmap p = XCreatePixmap(dpy, win, 64, 64, 16);
    XSetLineAttributes(dpy, gc, 5, LineSolid, CapRound, JoinRound);
    XDrawLine(dpy, p, gc, 0, 0, 63, 40);
    XFillArc(dpy, p, gc, 10, 10, 30, 30, 0, 360 * 64);
    XFreeGC(dpy, gc);
  }
  xshim_release_pixmaps(dpy);
  TEST_ASSERT_EQUAL_UINT64(before, __sanitizer_get_current_allocated_bytes());
}

void test_copy_area_within_one_pixmap_downwards_reads_before_it_overwrites(void) {
  Pixmap p = XCreatePixmap(dpy, win, 8, 6, 16);
  const uint16_t colours[3] = {0x1111, 0x2222, 0x3333};
  GC gc = new_gc(0, 0);
  for (int y = 0; y < 3; y++) {
    XSetForeground(dpy, gc, colours[y]);
    XFillRectangle(dpy, p, gc, 0, y, 8, 1);
  }
  XCopyArea(dpy, p, p, gc, 0, 0, 8, 3, 0, 1);
  XCopyArea(dpy, p, win, gc, 0, 0, 8, 6, 0, 0);
  TEST_ASSERT_EQUAL_HEX16(0x1111, at(0, 0));
  TEST_ASSERT_EQUAL_HEX16(0x1111, at(3, 1));
  TEST_ASSERT_EQUAL_HEX16(0x2222, at(7, 2));
  TEST_ASSERT_EQUAL_HEX16(0x3333, at(0, 3));
  TEST_ASSERT_EQUAL_HEX16(0, at(0, 4));
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, p);
}

void test_copy_area_from_a_pixmap_marks_only_the_copied_rows_dirty(void) {
  Pixmap p = XCreatePixmap(dpy, win, 5, 3, 16);
  GC gc = new_gc(0xFFFF, 0);
  canvas_clear_dirty(&cv);
  XCopyArea(dpy, p, win, gc, 0, 0, 5, 3, 4, 7);
  int x0, x1;
  for (int y = 7; y <= 9; y++) {
    TEST_ASSERT_TRUE(canvas_dirty_row(&cv, y, &x0, &x1));
    TEST_ASSERT_EQUAL_INT(4, x0);
    TEST_ASSERT_EQUAL_INT(8, x1);
  }
  TEST_ASSERT_FALSE(canvas_dirty_row(&cv, 6, &x0, &x1));
  TEST_ASSERT_FALSE(canvas_dirty_row(&cv, 10, &x0, &x1));
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, p);
}

void test_copy_area_within_one_pixmap_reads_before_it_overwrites(void) {
  Pixmap mask = None;
  Pixmap src = load_image(&mask);
  Pixmap p = XCreatePixmap(dpy, win, 8, 3, 16);
  GC gc = new_gc(0xFFFF, 0);
  XCopyArea(dpy, src, p, gc, 0, 0, BLOB_W, BLOB_H, 0, 0);
  XCopyArea(dpy, p, p, gc, 0, 0, BLOB_W, BLOB_H, 2, 0);
  XCopyArea(dpy, p, win, gc, 0, 0, 8, 3, 0, 0);
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x1000), at(2, 0));
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x1002), at(4, 0));
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x1003), at(5, 0));
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x100B), at(5, 2));
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, src);
  XFreePixmap(dpy, mask);
  XFreePixmap(dpy, p);
}

void test_collinear_points_with_projecting_caps_draw_the_same_as_one_segment(void) {
  const XPoint lines[][3] = {{{10, 20}, {25, 20}, {40, 20}},
                             {{40, 20}, {25, 20}, {10, 20}},
                             {{20, 10}, {20, 25}, {20, 40}},
                             {{20, 40}, {20, 25}, {20, 10}}};
  int bad = -1;
  for (int width = 2; width <= 9; width++)
    for (int k = 0; k < 4; k++) {
      use_canvas(64);
      GC gc = wide_gc(width, CapProjecting, JoinMiter);
      XDrawLine(dpy, win, gc, lines[k][0].x, lines[k][0].y, lines[k][2].x, lines[k][2].y);
      uint16_t *want = snapshot();
      use_canvas(64);
      draw_path(gc, lines[k], 3);
      if (bad < 0 && memcmp(want, cv.px, (size_t)64 * 64 * sizeof(uint16_t)) != 0)
        bad = width * 10 + k;
      free(want);
      XFreeGC(dpy, gc);
    }
  TEST_ASSERT_EQUAL_INT_MESSAGE(-1, bad, "first differing case is width * 10 + line");
}

void test_a_slanted_projecting_cap_reaches_half_the_width_past_the_end(void) {
  use_canvas(64);
  GC gc = wide_gc(5, CapProjecting, JoinMiter);
  XDrawLine(dpy, win, gc, 10, 10, 30, 30);
  /* Along the diagonal a pixel centre (x, y) is 1.41 * (x - 30.5) past the end. */
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(31, 31));
  TEST_ASSERT_EQUAL_HEX16_MESSAGE(0, at(32, 32), "2.83 past the end is beyond half the width (2.5)");
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(9, 9));
  TEST_ASSERT_EQUAL_HEX16(0, at(8, 8));
  XFreeGC(dpy, gc);
}

void test_set_fill_style_is_accepted_and_drawing_stays_solid(void) {
  use_canvas(64);
  XGCValues v;
  v.foreground = 0xFFFF;
  GC gc = XCreateGC(dpy, win, GCForeground, &v);
  TEST_ASSERT_EQUAL_INT(0, FillSolid);
  TEST_ASSERT_EQUAL_INT(0, XSetFillStyle(dpy, gc, FillSolid));
  XSetFillStyle(dpy, gc, FillStippled);
  XFillRectangle(dpy, win, gc, 2, 2, 3, 3);
  TEST_ASSERT_EQUAL_INT(9, count_set());
  XFreeGC(dpy, gc);
}

void test_query_pointer_reports_no_pointer(void) {
  Window root = 7, child = 7;
  int rx = 7, ry = 7, wx = 7, wy = 7;
  unsigned int mask = 7;
  TEST_ASSERT_FALSE(XQueryPointer(dpy, win, &root, &child, &rx, &ry, &wx, &wy, &mask));
  TEST_ASSERT_EQUAL_INT(0, rx + ry + wx + wy);
  TEST_ASSERT_EQUAL_UINT(0, mask);
  TEST_ASSERT_EQUAL_UINT(0, (unsigned)child);
}

void test_release_pixmaps_frees_every_live_pixmap_once(void) {
  Pixmap mask = None;
  Pixmap loaded = load_image(&mask);
  Pixmap made = XCreatePixmap(dpy, win, 6, 5, 16);
  Pixmap bits = XCreatePixmap(dpy, win, 6, 5, 1);
  TEST_ASSERT_NOT_EQUAL(None, loaded);
  TEST_ASSERT_NOT_EQUAL(None, made);
  XFreePixmap(dpy, bits);
  xshim_release_pixmaps(dpy);
  xshim_release_pixmaps(dpy);
  /* tearDown checks that nothing was left allocated; AddressSanitizer would
   * catch a pixmap freed both by the hack and by the release. */
}

void test_close_display_releases_pixmaps_a_hack_left_behind(void) {
  Pixmap mask = None;
  load_image(&mask);
  XCreatePixmap(dpy, win, 8, 8, 16);
}

void test_pixmaps_freed_in_any_order_unlink_cleanly(void) {
  Pixmap a = XCreatePixmap(dpy, win, 2, 2, 16);
  Pixmap b = XCreatePixmap(dpy, win, 2, 2, 16);
  Pixmap c = XCreatePixmap(dpy, win, 2, 2, 16);
  XFreePixmap(dpy, b);
  XFreePixmap(dpy, a);
  Pixmap d = XCreatePixmap(dpy, win, 2, 2, 16);
  XFreePixmap(dpy, c);
  xshim_release_pixmaps(dpy);
  (void)d;
}

void test_copy_area_into_a_pixmap_that_failed_to_allocate_leaves_the_canvas_alone(void) {
  Pixmap mask = None;
  Pixmap src = load_image(&mask);
  Pixmap failed = XCreatePixmap(dpy, win, 65535, 65535, 16);
  TEST_ASSERT_EQUAL(None, failed);
  GC gc = new_gc(0xFFFF, 0);
  XCopyArea(dpy, src, failed, gc, 0, 0, BLOB_W, BLOB_H, 3, 3);
  TEST_ASSERT_EQUAL_INT(0, count_set());
  XCopyArea(dpy, src, win, gc, 0, 0, BLOB_W, BLOB_H, 3, 3);
  TEST_ASSERT_EQUAL_INT(BLOB_W * BLOB_H, count_set());
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, src);
  XFreePixmap(dpy, mask);
}

static int hook_calls;
static Display *hook_arg;
static void counting_hook(Display *d) {
  hook_calls++;
  hook_arg = d;
}

/* The GL layer registers one so its context is freed when a hack stops. */
void test_release_hook_runs_once_with_the_display_and_is_then_cleared(void) {
  hook_calls = 0;
  hook_arg = NULL;
  xshim_set_release_hook(counting_hook);
  xshim_release_pixmaps(dpy);
  xshim_release_pixmaps(dpy);
  TEST_ASSERT_EQUAL_INT(1, hook_calls);
  TEST_ASSERT_EQUAL_PTR(dpy, hook_arg);
}

void test_create_image_fills_in_geometry_and_computes_the_row_size(void) {
  XImage *im = XCreateImage(dpy, NULL, 32, ZPixmap, 0, NULL, 7, 3, 32, 0);
  TEST_ASSERT_NOT_NULL(im);
  TEST_ASSERT_EQUAL_INT(7, im->width);
  TEST_ASSERT_EQUAL_INT(3, im->height);
  TEST_ASSERT_EQUAL_INT(32, im->depth);
  TEST_ASSERT_EQUAL_INT(32, im->bits_per_pixel);
  TEST_ASSERT_EQUAL_INT(28, im->bytes_per_line);
  TEST_ASSERT_NULL(im->data);
  XDestroyImage(im);
}

void test_create_image_keeps_a_row_size_the_caller_gave(void) {
  XImage *im = XCreateImage(dpy, NULL, 32, ZPixmap, 0, NULL, 7, 3, 32, 64);
  TEST_ASSERT_NOT_NULL(im);
  TEST_ASSERT_EQUAL_INT(64, im->bytes_per_line);
  XDestroyImage(im);
}

void test_image_pixels_round_trip_through_a_caller_allocated_buffer(void) {
  XImage *im = XCreateImage(dpy, NULL, 32, ZPixmap, 0, NULL, 4, 2, 32, 0);
  TEST_ASSERT_NOT_NULL(im);
  im->data = malloc((size_t)im->height * im->bytes_per_line);
  TEST_ASSERT_NOT_NULL(im->data);
  memset(im->data, 0, (size_t)im->height * im->bytes_per_line);
  XPutPixel(im, 3, 1, 0xFF123456UL);
  XPutPixel(im, 0, 0, 0x00000001UL);
  TEST_ASSERT_EQUAL_HEX64(0xFF123456UL, XGetPixel(im, 3, 1));
  TEST_ASSERT_EQUAL_HEX64(0x00000001UL, XGetPixel(im, 0, 0));
  TEST_ASSERT_EQUAL_HEX64(0UL, XGetPixel(im, 1, 0));
  XDestroyImage(im);
}

void test_image_pixel_outside_the_image_reads_zero_and_writes_nothing(void) {
  XImage *im = XCreateImage(dpy, NULL, 32, ZPixmap, 0, NULL, 2, 2, 32, 0);
  TEST_ASSERT_NOT_NULL(im);
  im->data = calloc((size_t)im->height, (size_t)im->bytes_per_line);
  TEST_ASSERT_NOT_NULL(im->data);
  XPutPixel(im, 2, 0, 0xFFFFFFFFUL);
  XPutPixel(im, 0, 2, 0xFFFFFFFFUL);
  XPutPixel(im, -1, 0, 0xFFFFFFFFUL);
  TEST_ASSERT_EQUAL_HEX64(0UL, XGetPixel(im, 2, 0));
  TEST_ASSERT_EQUAL_HEX64(0UL, XGetPixel(im, 0, -1));
  for (int i = 0; i < im->height * im->bytes_per_line; i++)
    TEST_ASSERT_EQUAL_UINT8(0, (uint8_t)im->data[i]);
  XDestroyImage(im);
}

void test_get_pixel_on_an_image_without_data_reads_zero(void) {
  XImage *im = XCreateImage(dpy, NULL, 32, ZPixmap, 0, NULL, 2, 2, 32, 0);
  TEST_ASSERT_NOT_NULL(im);
  TEST_ASSERT_EQUAL_HEX64(0UL, XGetPixel(im, 0, 0));
  XPutPixel(im, 0, 0, 1UL);
  XDestroyImage(im);
}

static void plot(Drawable d, int x, int y, unsigned long pixel) {
  XGCValues v = {0};
  GC gc = XCreateGC(dpy, d, 0, &v);
  XSetForeground(dpy, gc, pixel);
  XDrawPoint(dpy, d, gc, x, y);
  XFreeGC(dpy, gc);
}

void test_get_image_copies_a_pixmap_region_as_32_bit_pixels(void) {
  Pixmap p = XCreatePixmap(dpy, win, 4, 4, 16);
  plot(p, 1, 1, 0xABCD);
  plot(p, 2, 2, 0x1234);
  XImage *im = XGetImage(dpy, p, 1, 1, 2, 2, ~0UL, ZPixmap);
  TEST_ASSERT_NOT_NULL(im);
  TEST_ASSERT_EQUAL_INT(2, im->width);
  TEST_ASSERT_EQUAL_INT(2, im->height);
  TEST_ASSERT_EQUAL_INT(32, im->bits_per_pixel);
  TEST_ASSERT_EQUAL_HEX64(0xABCD, XGetPixel(im, 0, 0));
  TEST_ASSERT_EQUAL_HEX64(0, XGetPixel(im, 1, 0));
  TEST_ASSERT_EQUAL_HEX64(0x1234, XGetPixel(im, 1, 1));
  XDestroyImage(im);
  XFreePixmap(dpy, p);
}

void test_get_image_from_the_window_reads_the_canvas(void) {
  plot(win, 5, 6, 0x07E0);
  XImage *im = XGetImage(dpy, win, 4, 5, 3, 3, ~0UL, ZPixmap);
  TEST_ASSERT_NOT_NULL(im);
  TEST_ASSERT_EQUAL_HEX64(0x07E0, XGetPixel(im, 1, 1));
  XDestroyImage(im);
}

void test_get_image_refuses_a_bitmap_format_none_and_an_empty_region(void) {
  Pixmap p = XCreatePixmap(dpy, win, 4, 4, 16);
  TEST_ASSERT_NULL(XGetImage(dpy, p, 0, 0, 2, 2, ~0UL, XYPixmap));
  TEST_ASSERT_NULL(XGetImage(dpy, None, 0, 0, 2, 2, ~0UL, ZPixmap));
  TEST_ASSERT_NULL(XGetImage(dpy, p, 0, 0, 0, 2, ~0UL, ZPixmap));
  XFreePixmap(dpy, p);
}

void test_get_image_reads_zero_where_the_region_hangs_off_the_drawable(void) {
  Pixmap p = XCreatePixmap(dpy, win, 4, 4, 16);
  plot(p, 0, 0, 0x00FF);
  plot(p, 3, 3, 0x0F0F);
  XImage *im = XGetImage(dpy, p, -1, -1, 6, 6, ~0UL, ZPixmap);
  TEST_ASSERT_NOT_NULL(im);
  TEST_ASSERT_EQUAL_HEX64(0, XGetPixel(im, 0, 0));
  TEST_ASSERT_EQUAL_HEX64(0x00FF, XGetPixel(im, 1, 1));
  TEST_ASSERT_EQUAL_HEX64(0x0F0F, XGetPixel(im, 4, 4));
  TEST_ASSERT_EQUAL_HEX64(0, XGetPixel(im, 5, 5));
  XDestroyImage(im);
  XFreePixmap(dpy, p);
}

void test_put_image_writes_the_low_16_bits_into_a_pixmap(void) {
  Pixmap p = XCreatePixmap(dpy, win, 4, 4, 16);
  XImage *im = XCreateImage(dpy, NULL, 24, ZPixmap, 0, NULL, 2, 2, 32, 0);
  im->data = (char *)calloc(2, (size_t)im->bytes_per_line);
  XPutPixel(im, 0, 0, 0x1ABCD);
  XPutPixel(im, 1, 1, 0x4321);
  XGCValues v = {0};
  GC gc = XCreateGC(dpy, p, 0, &v);
  XPutImage(dpy, p, gc, im, 0, 0, 1, 2, 2, 2);
  XImage *back = XGetImage(dpy, p, 0, 0, 4, 4, ~0UL, ZPixmap);
  TEST_ASSERT_EQUAL_HEX64(0xABCD, XGetPixel(back, 1, 2));
  TEST_ASSERT_EQUAL_HEX64(0x4321, XGetPixel(back, 2, 3));
  TEST_ASSERT_EQUAL_HEX64(0, XGetPixel(back, 0, 0));
  XDestroyImage(back);
  XDestroyImage(im);
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, p);
}

void test_put_image_on_the_window_clips_and_marks_the_rows_dirty(void) {
  canvas_clear_dirty(&cv);
  XImage *im = XCreateImage(dpy, NULL, 24, ZPixmap, 0, NULL, 4, 4, 32, 0);
  im->data = (char *)calloc(4, (size_t)im->bytes_per_line);
  for (int y = 0; y < 4; y++)
    for (int x = 0; x < 4; x++) XPutPixel(im, x, y, 0x0100 + y * 4 + x);
  XGCValues v = {0};
  GC gc = XCreateGC(dpy, win, 0, &v);
  XPutImage(dpy, win, gc, im, 0, 0, 14, 14, 4, 4);
  TEST_ASSERT_EQUAL_HEX(0x0100, at(14, 14));
  TEST_ASSERT_EQUAL_HEX(0x0105, at(15, 15));
  int x0, x1;
  TEST_ASSERT_TRUE(canvas_dirty_row(&cv, 15, &x0, &x1));
  TEST_ASSERT_EQUAL_INT(14, x0);
  TEST_ASSERT_EQUAL_INT(15, x1);
  XPutImage(dpy, win, gc, im, 0, 0, -10, -10, 4, 4);
  XPutImage(dpy, win, gc, im, 3, 3, 100, 100, 50, 50);
  XFreeGC(dpy, gc);
  XDestroyImage(im);
}

/* A sprite enters and leaves past every edge, so a 7x7 image is put where 1,
 * 3, 4 or all 7 of its columns and rows land on the canvas. The pixels inside
 * must come from the matching part of the image, and the canvas outside must
 * keep what it held. */
void test_put_image_clips_at_every_edge_and_copies_the_matching_source_pixels(void) {
  enum { S = 7 };
  static const int pos[] = {-6, -3, 4, 13, 15};
  GC gc = XCreateGC(dpy, win, 0, NULL);
  XImage *im = XCreateImage(dpy, NULL, 16, ZPixmap, 0, NULL, S, S, 16, 0);
  TEST_ASSERT_NOT_NULL(im);
  uint16_t *pixels = malloc(S * S * sizeof *pixels);
  TEST_ASSERT_NOT_NULL(pixels);
  for (int i = 0; i < S * S; i++) pixels[i] = (uint16_t)(0x1000 + i);
  im->data = (char *)pixels;

  for (size_t a = 0; a < sizeof pos / sizeof pos[0]; a++) {
    for (size_t b = 0; b < sizeof pos / sizeof pos[0]; b++) {
      const int x = pos[a], y = pos[b];
      for (long i = 0; i < (long)cv.w * cv.h; i++)
        cv.px[i] = (uint16_t)(0x8001 + i % 251);
      XPutImage(dpy, win, gc, im, 0, 0, x, y, S, S);
      for (int row = 0; row < cv.h; row++) {
        for (int col = 0; col < cv.w; col++) {
          const long i = (long)row * cv.w + col;
          const int inside = col >= x && col < x + S && row >= y && row < y + S;
          const uint16_t want = inside
                                    ? (uint16_t)(0x1000 + (row - y) * S + (col - x))
                                    : (uint16_t)(0x8001 + i % 251);
          if (cv.px[i] != want) {
            char msg[80];
            snprintf(msg, sizeof msg, "image at (%d, %d): pixel (%d, %d)", x, y,
                     col, row);
            TEST_FAIL_MESSAGE(msg);
          }
        }
      }
    }
  }
  XDestroyImage(im);
  XFreeGC(dpy, gc);
}

/* A padded stride (7 pixels in a 16 byte row) and a source offset, clipped at
 * the canvas's right and bottom edges: each copied row must start at the
 * source offset in its own padded row, and stop at the clip. The pixel after
 * the clip on the same row and the first pixel of the next row stay untouched. */
void test_put_image_reads_a_padded_16_bit_source_from_its_offset(void) {
  enum { S = 7, STRIDE_PX = 8, OFF = 2 };
  GC gc = XCreateGC(dpy, win, 0, NULL);
  XImage *im = XCreateImage(dpy, NULL, 16, ZPixmap, 0, NULL, S, S, 32, 0);
  TEST_ASSERT_NOT_NULL(im);
  TEST_ASSERT_EQUAL_INT(STRIDE_PX * 2, im->bytes_per_line);
  uint16_t *pixels = malloc((size_t)STRIDE_PX * S * sizeof *pixels);
  TEST_ASSERT_NOT_NULL(pixels);
  for (int j = 0; j < S; j++)
    for (int i = 0; i < STRIDE_PX; i++)
      pixels[j * STRIDE_PX + i] = (uint16_t)(0x2000 + j * 16 + i);
  im->data = (char *)pixels;

  const int x = cv.w - 3, y = cv.h - 2;
  for (long i = 0; i < (long)cv.w * cv.h; i++) cv.px[i] = (uint16_t)(0x8001 + i % 251);
  XPutImage(dpy, win, gc, im, OFF, OFF - 1, x, y, S - OFF, S - OFF + 1);
  for (int row = 0; row < cv.h; row++) {
    for (int col = 0; col < cv.w; col++) {
      const long i = (long)row * cv.w + col;
      const int inside = col >= x && row >= y;
      const uint16_t want = inside ? (uint16_t)(0x2000 + (OFF - 1 + row - y) * 16 +
                                                (OFF + col - x))
                                   : (uint16_t)(0x8001 + i % 251);
      if (cv.px[i] != want) {
        char msg[60];
        snprintf(msg, sizeof msg, "pixel (%d, %d)", col, row);
        TEST_FAIL_MESSAGE(msg);
      }
    }
  }
  XDestroyImage(im);
  XFreeGC(dpy, gc);
}

void test_mirroring_each_glyph_column_block_through_get_and_put_image(void) {
  Pixmap p = XCreatePixmap(dpy, win, 4, 1, 16);
  for (int x = 0; x < 4; x++) plot(p, x, 0, 0x10 + x);
  XImage *im = XGetImage(dpy, p, 0, 0, 4, 1, ~0UL, ZPixmap);
  unsigned long row[2];
  for (int blk = 0; blk < 2; blk++) {
    for (int i = 0; i < 2; i++) row[i] = XGetPixel(im, blk * 2 + i, 0);
    for (int i = 0; i < 2; i++) XPutPixel(im, blk * 2 + i, 0, row[1 - i]);
  }
  XGCValues v = {0};
  GC gc = XCreateGC(dpy, p, 0, &v);
  XPutImage(dpy, p, gc, im, 0, 0, 0, 0, 4, 1);
  XImage *back = XGetImage(dpy, p, 0, 0, 4, 1, ~0UL, ZPixmap);
  TEST_ASSERT_EQUAL_HEX64(0x11, XGetPixel(back, 0, 0));
  TEST_ASSERT_EQUAL_HEX64(0x10, XGetPixel(back, 1, 0));
  TEST_ASSERT_EQUAL_HEX64(0x13, XGetPixel(back, 2, 0));
  TEST_ASSERT_EQUAL_HEX64(0x12, XGetPixel(back, 3, 0));
  XDestroyImage(back);
  XDestroyImage(im);
  XFreeGC(dpy, gc);
  XFreePixmap(dpy, p);
}

void test_the_device_has_no_keyboard_so_lookup_string_finds_nothing(void) {
  XEvent e = {0};
  e.type = KeyPress;
  char c = 'x';
  KeySym ks = 99;
  TEST_ASSERT_EQUAL_INT(0, XLookupString(&e.xkey, &c, 1, &ks, 0));
  TEST_ASSERT_EQUAL_HEX64(0, ks);
  TEST_ASSERT_EQUAL_INT('x', c);
}

void test_there_is_never_a_text_source(void) {
  TEST_ASSERT_NULL(textclient_open(dpy));
  TEST_ASSERT_EQUAL_INT(-1, textclient_getc(NULL));
  TEST_ASSERT_FALSE(textclient_puts(NULL, "x"));
}

void test_the_timer_stand_ins_never_fire_and_can_be_cancelled(void) {
  XtAppContext app = XtDisplayToApplicationContext(dpy);
  XtIntervalId id = XtAppAddTimeOut(app, 333, NULL, NULL);
  TEST_ASSERT_EQUAL_HEX64(0, id);
  XtRemoveTimeOut(id);
}

void test_rgb_to_hsv_inverts_hsv_to_rgb(void) {
  unsigned short r, g, b;
  int h = -1;
  double sat = -1, val = -1;
  hsv_to_rgb(120, 1.0, 1.0, &r, &g, &b);
  rgb_to_hsv(r, g, b, &h, &sat, &val);
  TEST_ASSERT_EQUAL_INT(120, h);
  TEST_ASSERT_TRUE(sat > 0.99 && val > 0.99);
  rgb_to_hsv(0x8000, 0x8000, 0x8000, &h, &sat, &val);
  TEST_ASSERT_TRUE(sat < 0.01);
}

void test_query_color_recovers_the_colour_alloc_color_made(void) {
  XColor red = {0};
  red.red = 0xFFFF;
  XAllocColor(dpy, 1, &red);
  XColor q = {0};
  q.pixel = red.pixel;
  TEST_ASSERT_TRUE(XQueryColor(dpy, 1, &q) != 0);
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, q.red);
  TEST_ASSERT_EQUAL_HEX16(0, q.green);
  TEST_ASSERT_EQUAL_HEX16(0, q.blue);
  XColor odd = {0};
  odd.red = 0x1234;
  odd.green = 0x9ABC;
  odd.blue = 0x5678;
  XAllocColor(dpy, 1, &odd);
  XColor again = {0};
  again.pixel = odd.pixel;
  XQueryColor(dpy, 1, &again);
  XAllocColor(dpy, 1, &again);
  TEST_ASSERT_EQUAL_HEX64(odd.pixel, again.pixel);
}

void test_the_screen_is_a_colour_display_with_no_writable_cells(void) {
  TEST_ASSERT_TRUE(CellsOfScreen(DefaultScreenOfDisplay(dpy)) > 2);
  TEST_ASSERT_FALSE(has_writable_cells(NULL, NULL));
}

void test_a_16_bit_image_is_two_bytes_a_pixel_and_keeps_16_bits(void) {
  XImage *im = XCreateImage(dpy, NULL, 16, ZPixmap, 0, NULL, 5, 3, 8, 0);
  TEST_ASSERT_NOT_NULL(im);
  TEST_ASSERT_EQUAL_INT(16, im->bits_per_pixel);
  TEST_ASSERT_EQUAL_INT(10, im->bytes_per_line);
  im->data = (char *)calloc(3, (size_t)im->bytes_per_line);
  XPutPixel(im, 1, 1, 0x1ABCD);
  XPutPixel(im, 2, 1, 0x1234);
  TEST_ASSERT_EQUAL_HEX64(0xABCD, XGetPixel(im, 1, 1));
  TEST_ASSERT_EQUAL_HEX64(0x1234, XGetPixel(im, 2, 1));
  TEST_ASSERT_EQUAL_HEX64(0, XGetPixel(im, 0, 1));
  TEST_ASSERT_EQUAL_HEX64(0, XGetPixel(im, 3, 1));
  XDestroyImage(im);
}

void test_utf8_to_latin1_copies_ascii_and_gives_up_on_anything_else(void) {
  const char *plain = "space program";
  char *copy = utf8_to_latin1(plain, 1);
  TEST_ASSERT_NOT_NULL(copy);
  TEST_ASSERT_TRUE(copy != plain);
  TEST_ASSERT_EQUAL_STRING("space program", copy);
  free(copy);
  copy = utf8_to_latin1("", 1);
  TEST_ASSERT_NOT_NULL(copy);
  TEST_ASSERT_EQUAL_STRING("", copy);
  free(copy);
  TEST_ASSERT_NULL(utf8_to_latin1("caf\xc3\xa9", 1));
  TEST_ASSERT_NULL(utf8_to_latin1("caf\xc3\xa9", 0));
}

void test_utf8_encode_writes_one_to_four_bytes_and_terminates(void) {
  char out[8];
  memset(out, 'x', sizeof out);
  TEST_ASSERT_EQUAL_INT(1, utf8_encode('A', out, sizeof out - 1));
  TEST_ASSERT_EQUAL_STRING("A", out);
  TEST_ASSERT_EQUAL_INT(2, utf8_encode(0xE9, out, sizeof out - 1));
  TEST_ASSERT_EQUAL_STRING("\xc3\xa9", out);
  TEST_ASSERT_EQUAL_INT(3, utf8_encode(0x2592, out, sizeof out - 1));
  TEST_ASSERT_EQUAL_STRING("\xe2\x96\x92", out);
  TEST_ASSERT_EQUAL_INT(4, utf8_encode(0x1F600, out, sizeof out - 1));
  TEST_ASSERT_EQUAL_STRING("\xf0\x9f\x98\x80", out);
}

void test_utf8_encode_writes_nothing_when_the_character_does_not_fit(void) {
  char out[8];
  memset(out, 'x', sizeof out);
  TEST_ASSERT_EQUAL_INT(0, utf8_encode(0x2592, out, 2));
  TEST_ASSERT_EQUAL_INT('x', out[0]);
}

void test_utf8_decode_reads_the_first_character_and_says_how_many_bytes(void) {
  unsigned long c = 0;
  TEST_ASSERT_EQUAL_INT(1, utf8_decode((const unsigned char *)"Az", 2, &c));
  TEST_ASSERT_EQUAL_HEX64('A', c);
  TEST_ASSERT_EQUAL_INT(2, utf8_decode((const unsigned char *)"\xc3\xa9", 2, &c));
  TEST_ASSERT_EQUAL_HEX64(0xE9, c);
  TEST_ASSERT_EQUAL_INT(3, utf8_decode((const unsigned char *)"\xe2\x96\x92", 3, &c));
  TEST_ASSERT_EQUAL_HEX64(0x2592, c);
  TEST_ASSERT_EQUAL_INT(4, utf8_decode((const unsigned char *)"\xf0\x9f\x98\x80", 4, &c));
  TEST_ASSERT_EQUAL_HEX64(0x1F600, c);
}

void test_utf8_decode_does_not_read_past_the_length(void) {
  unsigned long c = 0x77;
  TEST_ASSERT_EQUAL_INT(0, utf8_decode((const unsigned char *)"\xe2\x96", 2, &c));
  TEST_ASSERT_EQUAL_INT(0, utf8_decode((const unsigned char *)"A", 0, &c));
  TEST_ASSERT_EQUAL_INT(1, utf8_decode((const unsigned char *)"\x80", 1, &c));
}

void test_an_image_reports_the_server_byte_and_bit_order_and_lets_a_hack_set_them(void) {
  XImage *im = XCreateImage(dpy, NULL, 1, XYBitmap, 0, NULL, 9, 1, 8, 0);
  TEST_ASSERT_NOT_NULL(im);
  TEST_ASSERT_EQUAL_INT(LSBFirst, im->byte_order);
  TEST_ASSERT_EQUAL_INT(LSBFirst, im->bitmap_bit_order);
  TEST_ASSERT_EQUAL_INT(8, im->bitmap_unit);
  im->byte_order = MSBFirst;
  im->bitmap_unit = 8;
  im->bitmap_bit_order = LSBFirst;
  im->data = (char *)calloc(1, (size_t)im->bytes_per_line);
  XPutPixel(im, 1, 0, 1);
  TEST_ASSERT_EQUAL_HEX8(0x02, (uint8_t)im->data[0]);
  XDestroyImage(im);
}

void test_countof_is_the_number_of_elements_of_an_array_not_its_bytes(void) {
  int ints[7];
  struct { double d; char c; } structs[3];
  TEST_ASSERT_EQUAL_UINT64(7, countof(ints));
  TEST_ASSERT_EQUAL_UINT64(3, countof(structs));
}

void test_unmap_notify_has_the_x_event_number_and_fits_an_event_type(void) {
  XEvent e;
  e.type = UnmapNotify;
  TEST_ASSERT_EQUAL_INT(18, e.type);
}

void test_i_log2_is_the_floor_of_log_two_and_minus_one_for_zero(void) {
  TEST_ASSERT_EQUAL_INT(-1, i_log2(0));
  TEST_ASSERT_EQUAL_INT(0, i_log2(1));
  TEST_ASSERT_EQUAL_INT(1, i_log2(2));
  TEST_ASSERT_EQUAL_INT(1, i_log2(3));
  TEST_ASSERT_EQUAL_INT(8, i_log2(373));
  TEST_ASSERT_EQUAL_INT(8, i_log2(511));
  TEST_ASSERT_EQUAL_INT(9, i_log2(512));
  TEST_ASSERT_EQUAL_INT(i_log2(1000), i_log2_fast(1000));
}

void test_to_pow2_rounds_up_to_the_next_power_of_two(void) {
  TEST_ASSERT_EQUAL_UINT64(1, to_pow2(0));
  TEST_ASSERT_EQUAL_UINT64(1, to_pow2(1));
  TEST_ASSERT_EQUAL_UINT64(2, to_pow2(2));
  TEST_ASSERT_EQUAL_UINT64(4, to_pow2(3));
  TEST_ASSERT_EQUAL_UINT64(8, to_pow2(5));
  TEST_ASSERT_EQUAL_UINT64(512, to_pow2(512));
  TEST_ASSERT_EQUAL_UINT64(512, to_pow2(373));
}

void test_bitmap_pad_is_32_bits_and_makes_a_tight_image_row_whole_words(void) {
  TEST_ASSERT_EQUAL_INT(32, BitmapPad(dpy));
  XImage *im = XCreateImage(dpy, NULL, 16, ZPixmap, 0, NULL, 3, 1, BitmapPad(dpy), 0);
  TEST_ASSERT_NOT_NULL(im);
  TEST_ASSERT_EQUAL_INT(8, im->bytes_per_line);
  XDestroyImage(im);
}

void test_a_bitmap_image_is_one_bit_a_pixel_least_significant_bit_first(void) {
  XImage *im = XCreateImage(dpy, NULL, 1, XYBitmap, 0, NULL, 9, 2, 8, 0);
  TEST_ASSERT_NOT_NULL(im);
  TEST_ASSERT_EQUAL_INT(1, im->bits_per_pixel);
  TEST_ASSERT_EQUAL_INT(2, im->bytes_per_line);
  im->data = (char *)calloc(2, (size_t)im->bytes_per_line);
  XPutPixel(im, 0, 0, 1);
  XPutPixel(im, 2, 0, 1);
  XPutPixel(im, 8, 1, 1);
  TEST_ASSERT_EQUAL_HEX8(0x05, (uint8_t)im->data[0]);
  TEST_ASSERT_EQUAL_HEX8(0x00, (uint8_t)im->data[1]);
  TEST_ASSERT_EQUAL_HEX8(0x01, (uint8_t)im->data[3]);
  TEST_ASSERT_EQUAL_HEX64(1, XGetPixel(im, 2, 0));
  TEST_ASSERT_EQUAL_HEX64(0, XGetPixel(im, 1, 0));
  TEST_ASSERT_EQUAL_HEX64(1, XGetPixel(im, 8, 1));
  XPutPixel(im, 2, 0, 0);
  TEST_ASSERT_EQUAL_HEX8(0x01, (uint8_t)im->data[0]);
  XDestroyImage(im);
}

void test_put_image_of_a_bitmap_draws_set_bits_in_the_foreground_and_clear_in_the_background(void) {
  XImage *im = XCreateImage(dpy, NULL, 1, XYBitmap, 0, NULL, 4, 1, 8, 0);
  im->data = (char *)calloc(1, (size_t)im->bytes_per_line);
  XPutPixel(im, 1, 0, 1);
  XPutPixel(im, 3, 0, 1);
  XGCValues v = {0};
  GC gc = XCreateGC(dpy, win, 0, &v);
  XSetForeground(dpy, gc, 0x1111);
  XSetBackground(dpy, gc, 0x2222);
  XPutImage(dpy, win, gc, im, 0, 0, 5, 6, 4, 1);
  TEST_ASSERT_EQUAL_HEX16(0x2222, at(5, 6));
  TEST_ASSERT_EQUAL_HEX16(0x1111, at(6, 6));
  TEST_ASSERT_EQUAL_HEX16(0x2222, at(7, 6));
  TEST_ASSERT_EQUAL_HEX16(0x1111, at(8, 6));
  XFreeGC(dpy, gc);
  XDestroyImage(im);
}

void test_put_image_of_a_16_bit_image_copies_canvas_order_pixels_unchanged(void) {
  XImage *im = XCreateImage(dpy, NULL, 16, ZPixmap, 0, NULL, 2, 1, 8, 0);
  im->data = (char *)calloc(1, (size_t)im->bytes_per_line);
  XPutPixel(im, 0, 0, px_swap(0xF800));
  XPutPixel(im, 1, 0, px_swap(0x07E0));
  XGCValues v = {0};
  GC gc = XCreateGC(dpy, win, 0, &v);
  XPutImage(dpy, win, gc, im, 0, 0, 3, 4, 2, 1);
  TEST_ASSERT_EQUAL_HEX16(px_swap(0xF800), at(3, 4));
  TEST_ASSERT_EQUAL_HEX16(px_swap(0x07E0), at(4, 4));
  XFreeGC(dpy, gc);
  XDestroyImage(im);
}

void test_an_allocated_colour_keeps_its_pixel_through_a_16_bit_image(void) {
  XColor c = {0};
  c.red = 0xF800;
  c.green = 0x2000;
  c.blue = 0x0800;
  c.flags = DoRed | DoGreen | DoBlue;
  TEST_ASSERT_TRUE(XAllocColor(dpy, 1, &c));
  TEST_ASSERT_NOT_EQUAL((uint16_t)c.pixel, px_swap((uint16_t)c.pixel));
  XImage *im = XCreateImage(dpy, NULL, 16, ZPixmap, 0, NULL, 1, 1, 8, 0);
  im->data = (char *)calloc(1, (size_t)im->bytes_per_line);
  XPutPixel(im, 0, 0, c.pixel);
  XGCValues v = {0};
  GC gc = XCreateGC(dpy, win, 0, &v);
  XPutImage(dpy, win, gc, im, 0, 0, 3, 4, 1, 1);
  TEST_ASSERT_EQUAL_HEX16((uint16_t)c.pixel, at(3, 4));
  XFreeGC(dpy, gc);
  XDestroyImage(im);
}

void test_the_server_byte_order_is_least_significant_first(void) {
  TEST_ASSERT_NOT_EQUAL(MSBFirst, ImageByteOrder(dpy));
  TEST_ASSERT_EQUAL_INT(LSBFirst, ImageByteOrder(dpy));
}

static spline *square_spline(void) {
  spline *sp = make_spline(4);
  static const double xs[4] = {0, 10, 10, 0}, ys[4] = {0, 0, 10, 10};
  for (int i = 0; i < 4; i++) {
    sp->control_x[i] = xs[i];
    sp->control_y[i] = ys[i];
  }
  return sp;
}

void test_a_new_spline_has_zeroed_controls_and_no_points(void) {
  spline *sp = make_spline(5);
  TEST_ASSERT_NOT_NULL(sp);
  TEST_ASSERT_EQUAL_UINT(5, sp->n_controls);
  TEST_ASSERT_EQUAL_UINT(0, sp->n_points);
  for (int i = 0; i < 5; i++) {
    TEST_ASSERT_TRUE(sp->control_x[i] == 0);
    TEST_ASSERT_TRUE(sp->control_y[i] == 0);
  }
  free_spline(sp);
  free_spline(NULL);
}

void test_a_closed_spline_of_a_square_is_a_smooth_loop_inside_it(void) {
  spline *sp = square_spline();
  compute_closed_spline(sp);
  TEST_ASSERT_TRUE(sp->n_points >= 4 * 4);
  TEST_ASSERT_EQUAL_UINT(0, sp->n_points % 4);
  long sx = 0, sy = 0;
  for (unsigned i = 0; i < sp->n_points; i++) {
    TEST_ASSERT_TRUE(sp->points[i].x >= 0 && sp->points[i].x <= 10);
    TEST_ASSERT_TRUE(sp->points[i].y >= 0 && sp->points[i].y <= 10);
    sx += sp->points[i].x;
    sy += sp->points[i].y;
  }
  TEST_ASSERT_INT_WITHIN(1, 5 * (long)sp->n_points, sx);
  TEST_ASSERT_INT_WITHIN(1, 5 * (long)sp->n_points, sy);
  /* A uniform cubic B-spline starts a segment at (p[i-1] + 4 p[i] + p[i+1]) / 6:
   * for the corner at (0, 0) that is (10/6, 10/6), so the curve cuts the corner. */
  TEST_ASSERT_EQUAL_INT(2, sp->points[0].x);
  TEST_ASSERT_EQUAL_INT(2, sp->points[0].y);
  /* It closes: the last point is a step from the first, not a jump. */
  const int dx = sp->points[sp->n_points - 1].x - sp->points[0].x;
  const int dy = sp->points[sp->n_points - 1].y - sp->points[0].y;
  TEST_ASSERT_TRUE(dx * dx + dy * dy <= 4 * 4);
  free_spline(sp);
}

/* compute_closed_spline as it was before it learned to skip the double
 * arithmetic, copied from main: every spline below is compared against it,
 * point for point. */
#define REFERENCE_SPLINE_STEPS 6
static void reference_compute_closed_spline(spline *s) {
  const unsigned n = s->n_controls;
  s->n_points = 0;
  if (n == 0) return;
  const unsigned total = n * REFERENCE_SPLINE_STEPS;
  if (s->allocated_points < total) {
    XPoint *grown = (XPoint *)realloc(s->points, (size_t)total * sizeof(XPoint));
    if (!grown) return;
    s->points = grown;
    s->allocated_points = total;
  }
  unsigned k = 0;
  for (unsigned i = 0; i < n; i++) {
    const unsigned a = (i + n - 1) % n, b = i, c = (i + 1) % n, d = (i + 2) % n;
    for (int j = 0; j < REFERENCE_SPLINE_STEPS; j++) {
      const double t = (double)j / REFERENCE_SPLINE_STEPS, t2 = t * t, t3 = t2 * t;
      const double w0 = (1 - t) * (1 - t) * (1 - t) / 6;
      const double w1 = (3 * t3 - 6 * t2 + 4) / 6;
      const double w2 = (-3 * t3 + 3 * t2 + 3 * t + 1) / 6;
      const double w3 = t3 / 6;
      s->points[k].x = (short)lround(w0 * s->control_x[a] + w1 * s->control_x[b] +
                                     w2 * s->control_x[c] + w3 * s->control_x[d]);
      s->points[k].y = (short)lround(w0 * s->control_y[a] + w1 * s->control_y[b] +
                                     w2 * s->control_y[c] + w3 * s->control_y[d]);
      k++;
    }
  }
  s->n_points = k;
}


/* Random control points, drawn by `kind`: 0 whole numbers on the canvas (the
 * weights are sixths, so many results fall exactly on a half pixel), 1 fractions
 * on the canvas, 2 far off it, 3 huge, 4 up to a million, where float's spacing is coarse. Returns how many splines differed from
 * the reference. */
static int splines_that_differ_from_the_reference(int kind, int count) {
  srandom(1);
  int differing = 0;
  for (int i = 0; i < count; i++) {
    const unsigned n = 3 + (unsigned)(random() % 12);
    spline *fast = make_spline(n), *ref = make_spline(n);
    for (unsigned k = 0; k < n; k++) {
      double x, y;
      switch (kind) {
        case 0: x = (double)(random() % 466); y = (double)(random() % 466); break;
        case 1: x = (double)(random() % 466000) / 1000.0; y = (double)(random() % 466000) / 1000.0; break;
        case 2: x = (double)(random() % 2000000) / 10.0 - 100000.0; y = (double)(random() % 2000000) / 10.0 - 100000.0; break;
        case 4: x = (double)(random() % 20000000) / 10.0 - 1000000.0; y = (double)(random() % 20000000) / 10.0 - 1000000.0; break;
        default: x = (double)(random() % 2000000) * 1e5 - 1e11; y = (double)(random() % 2000000) * 1e5 - 1e11; break;
      }
      fast->control_x[k] = ref->control_x[k] = x;
      fast->control_y[k] = ref->control_y[k] = y;
    }
    compute_closed_spline(fast);
    reference_compute_closed_spline(ref);
    int same = fast->n_points == ref->n_points;
    for (unsigned k = 0; same && k < ref->n_points; k++)
      same = fast->points[k].x == ref->points[k].x && fast->points[k].y == ref->points[k].y;
    differing += !same;
    free_spline(fast);
    free_spline(ref);
  }
  return differing;
}

void test_splines_of_whole_number_controls_match_the_reference(void) {
  TEST_ASSERT_EQUAL_INT(0, splines_that_differ_from_the_reference(0, 5000));
}

void test_splines_of_fractional_controls_match_the_reference(void) {
  TEST_ASSERT_EQUAL_INT(0, splines_that_differ_from_the_reference(1, 5000));
}

void test_splines_of_far_off_controls_match_the_reference(void) {
  TEST_ASSERT_EQUAL_INT(0, splines_that_differ_from_the_reference(2, 5000));
}

void test_a_spline_of_more_than_64_controls_matches_the_reference(void) {
  srandom(1);
  for (int round = 0; round < 50; round++) {
    const unsigned n = 65 + (unsigned)(random() % 80);
    spline *fast = make_spline(n), *ref = make_spline(n);
    for (unsigned k = 0; k < n; k++) {
      fast->control_x[k] = ref->control_x[k] = (double)(random() % 466000) / 1000.0;
      fast->control_y[k] = ref->control_y[k] = (double)(random() % 466000) / 1000.0;
    }
    compute_closed_spline(fast);
    reference_compute_closed_spline(ref);
    TEST_ASSERT_EQUAL_UINT(ref->n_points, fast->n_points);
    for (unsigned k = 0; k < ref->n_points; k++) {
      TEST_ASSERT_EQUAL_INT(ref->points[k].x, fast->points[k].x);
      TEST_ASSERT_EQUAL_INT(ref->points[k].y, fast->points[k].y);
    }
    free_spline(fast);
    free_spline(ref);
  }
}

void test_splines_of_controls_up_to_a_million_match_the_reference(void) {
  TEST_ASSERT_EQUAL_INT(0, splines_that_differ_from_the_reference(4, 20000));
}

void test_splines_of_huge_controls_match_the_reference(void) {
  TEST_ASSERT_EQUAL_INT(0, splines_that_differ_from_the_reference(3, 5000));
}

void test_a_closed_spline_reuses_its_points_and_grows_them_for_more_controls(void) {
  spline *sp = square_spline();
  compute_closed_spline(sp);
  XPoint *first = sp->points;
  const unsigned allocated = sp->allocated_points;
  TEST_ASSERT_TRUE(allocated >= sp->n_points);
  compute_closed_spline(sp);
  TEST_ASSERT_TRUE(first == sp->points);
  TEST_ASSERT_EQUAL_UINT(allocated, sp->allocated_points);
  free_spline(sp);
  spline *big = make_spline(40);
  for (int i = 0; i < 40; i++) {
    big->control_x[i] = i;
    big->control_y[i] = i * 2;
  }
  compute_closed_spline(big);
  TEST_ASSERT_TRUE(big->n_points >= 40 * 4);
  free_spline(big);
}

void test_a_closed_spline_of_one_point_is_that_point(void) {
  spline *sp = make_spline(3);
  for (int i = 0; i < 3; i++) {
    sp->control_x[i] = 7;
    sp->control_y[i] = 9;
  }
  compute_closed_spline(sp);
  for (unsigned i = 0; i < sp->n_points; i++) {
    TEST_ASSERT_EQUAL_INT(7, sp->points[i].x);
    TEST_ASSERT_EQUAL_INT(9, sp->points[i].y);
  }
  free_spline(sp);
  spline *none = make_spline(0);
  compute_closed_spline(none);
  TEST_ASSERT_EQUAL_UINT(0, none->n_points);
  free_spline(none);
}

void test_fill_rule_constants_and_the_even_odd_fill_of_a_pentagram(void) {
  TEST_ASSERT_EQUAL_INT(0, EvenOddRule);
  TEST_ASSERT_EQUAL_INT(1, WindingRule);
  TEST_ASSERT_EQUAL_HEX64(1L << 9, GCFillRule);
  XGCValues v = {0};
  v.foreground = 0xFFFF;
  v.fill_rule = EvenOddRule;
  GC gc = XCreateGC(dpy, win, GCForeground | GCFillRule, &v);
  XFreeGC(dpy, gc);
  /* A five-pointed star drawn as one self-crossing polygon: even-odd leaves
   * the middle pentagon empty. */
  GC white = new_gc(0xFFFF, 0);
  XPoint star[5] = {{8, 0}, {13, 15}, {0, 6}, {15, 6}, {3, 15}};
  XFillPolygon(dpy, win, white, star, 5, Complex, CoordModeOrigin);
  TEST_ASSERT_EQUAL_HEX16(0, at(8, 8));
  TEST_ASSERT_TRUE(at(8, 2) != 0);
  XFreeGC(dpy, white);
}

void test_the_screen_stand_in_leads_back_to_the_display_and_its_black_and_white(void) {
  Screen *screen = DefaultScreenOfDisplay(dpy);
  TEST_ASSERT_TRUE(DisplayOfScreen(screen) == dpy);
  TEST_ASSERT_EQUAL_HEX64(BlackPixel(dpy, DefaultScreen(dpy)), BlackPixelOfScreen(screen));
  TEST_ASSERT_EQUAL_HEX64(WhitePixel(dpy, DefaultScreen(dpy)), WhitePixelOfScreen(screen));
}

void test_the_display_a_screen_leads_to_is_the_one_most_recently_opened(void) {
  Canvas other;
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&other, 4, 4, malloc));
  Display *second = xshim_open_display(&other);
  TEST_ASSERT_TRUE(DisplayOfScreen(DefaultScreenOfDisplay(second)) == second);
  xshim_close_display(second);
  canvas_free(&other);
  TEST_ASSERT_TRUE(DisplayOfScreen(DefaultScreenOfDisplay(dpy)) == dpy);
}

void test_closing_displays_out_of_order_never_leaves_a_dangling_current_display(void) {
  Canvas a, b;
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&a, 4, 4, malloc));
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&b, 4, 4, malloc));
  Display *da = xshim_open_display(&a);
  Display *db = xshim_open_display(&b);
  TEST_ASSERT_TRUE(xshim_current_display() == db);
  xshim_close_display(da);
  TEST_ASSERT_TRUE(xshim_current_display() == db);
  xshim_close_display(db);
  TEST_ASSERT_TRUE(xshim_current_display() == dpy);
  canvas_free(&a);
  canvas_free(&b);
}

void test_the_gc_function_and_alpha_calls_are_accepted_and_change_nothing(void) {
  GC gc = new_gc(0xF800, 0);
  XSetFunction(dpy, gc, GXxor);
  jwxyz_XSetAlphaAllowed(dpy, gc, True);
  XFillRectangle(dpy, win, gc, 0, 0, 4, 4);
  XFillRectangle(dpy, win, gc, 0, 0, 4, 4);
  TEST_ASSERT_EQUAL_HEX16(0xF800, at(1, 1));
  XFreeGC(dpy, gc);
}

void test_relative_coordinates_fill_the_same_polygon_as_absolute_ones(void) {
  GC gc = new_gc(0xFFFF, 0);
  XPoint absolute[4] = {{4, 4}, {12, 4}, {12, 12}, {4, 12}};
  XFillPolygon(dpy, win, gc, absolute, 4, Convex, CoordModeOrigin);
  const int want = count_set();
  TEST_ASSERT_TRUE(want > 40);
  XClearWindow(dpy, win);
  XPoint relative[4] = {{4, 4}, {8, 0}, {0, 8}, {-8, 0}};
  XFillPolygon(dpy, win, gc, relative, 4, Convex, CoordModePrevious);
  TEST_ASSERT_EQUAL_INT(want, count_set());
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(8, 8));
  TEST_ASSERT_EQUAL_HEX16(0, at(2, 2));
  XFreeGC(dpy, gc);
}

void test_relative_coordinates_draw_the_same_lines_and_points_as_absolute_ones(void) {
  GC gc = new_gc(0xFFFF, 0);
  XPoint abs_line[3] = {{2, 2}, {9, 2}, {9, 9}};
  XDrawLines(dpy, win, gc, abs_line, 3, CoordModeOrigin);
  const int want = count_set();
  XClearWindow(dpy, win);
  XPoint rel_line[3] = {{2, 2}, {7, 0}, {0, 7}};
  XDrawLines(dpy, win, gc, rel_line, 3, CoordModePrevious);
  TEST_ASSERT_EQUAL_INT(want, count_set());
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(9, 5));
  XClearWindow(dpy, win);
  XPoint rel_pts[3] = {{3, 3}, {2, 1}, {1, 1}};
  XDrawPoints(dpy, win, gc, rel_pts, 3, CoordModePrevious);
  TEST_ASSERT_EQUAL_INT(3, count_set());
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(3, 3));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(5, 4));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(6, 5));
  XFreeGC(dpy, gc);
}

void test_a_pixmap_made_from_bitmap_data_reads_least_significant_bit_first(void) {
  static const char data[2] = {0x01, (char)0x80};
  Pixmap bits = XCreatePixmapFromBitmapData(dpy, win, (char *)data, 8, 2, 1, 0, 1);
  TEST_ASSERT_NOT_EQUAL(None, bits);
  GC gc = new_gc(0xFFFF, 0x0001);
  XCopyPlane(dpy, bits, win, gc, 0, 0, 8, 2, 0, 0, 1);
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(0, 0));
  TEST_ASSERT_EQUAL_HEX16(0x0001, at(1, 0));
  TEST_ASSERT_EQUAL_HEX16(0x0001, at(7, 0));
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(7, 1));
  TEST_ASSERT_EQUAL_HEX16(0x0001, at(0, 1));
  XFreePixmap(dpy, bits);
  Pixmap colour = XCreatePixmapFromBitmapData(dpy, win, (char *)data, 8, 2, 0xF800, 0x001F, 16);
  TEST_ASSERT_NOT_EQUAL(None, colour);
  XCopyArea(dpy, colour, win, gc, 0, 0, 8, 2, 0, 4);
  TEST_ASSERT_EQUAL_HEX16(0xF800, at(0, 4));
  TEST_ASSERT_EQUAL_HEX16(0x001F, at(1, 4));
  TEST_ASSERT_EQUAL_HEX16(0xF800, at(7, 5));
  XFreePixmap(dpy, colour);
  XFreeGC(dpy, gc);
  TEST_ASSERT_EQUAL_UINT64(None, XCreatePixmapFromBitmapData(dpy, win, (char *)data, 0, 2, 1, 0, 1));
}

void test_stipple_constants_and_fields_are_accepted_and_fills_stay_solid(void) {
  TEST_ASSERT_EQUAL_INT(1, CoordModePrevious);
  TEST_ASSERT_EQUAL_HEX64(1L << 8, GCFillStyle);
  TEST_ASSERT_EQUAL_HEX64(1L << 11, GCStipple);
  XGCValues v = {0};
  v.foreground = 0xFFFF;
  v.fill_style = FillOpaqueStippled;
  v.stipple = None;
  GC gc = XCreateGC(dpy, win, GCForeground | GCFillStyle | GCStipple, &v);
  XFillRectangle(dpy, win, gc, 0, 0, 4, 4);
  TEST_ASSERT_EQUAL_INT(16, count_set());
  XFreeGC(dpy, gc);
}

void test_create_image_refuses_depths_and_sizes_it_cannot_hold(void) {
  TEST_ASSERT_NULL(XCreateImage(dpy, NULL, 8, ZPixmap, 0, NULL, 4, 4, 8, 0));
  TEST_ASSERT_NULL(XCreateImage(dpy, NULL, 32, ZPixmap, 0, NULL, 0, 4, 32, 0));
  TEST_ASSERT_NULL(XCreateImage(dpy, NULL, 32, ZPixmap, 0, NULL, 4, 0, 32, 0));
  TEST_ASSERT_NULL(
      XCreateImage(dpy, NULL, 32, ZPixmap, 0, NULL, INT_MAX, 4, 32, 0));
}

void test_file_to_ximage_has_no_decoder_and_returns_null(void) {
  TEST_ASSERT_NULL(file_to_ximage(dpy, NULL, "/no/such/mask.png"));
}

void test_ease_in_out_sine_runs_from_0_to_1_through_one_half(void) {
  TEST_ASSERT_FLOAT_WITHIN(2e-6f, 0.0f, (float)ease(EASE_IN_OUT_SINE, 0.0));
  TEST_ASSERT_FLOAT_WITHIN(2e-6f, 0.5f, (float)ease(EASE_IN_OUT_SINE, 0.5));
  TEST_ASSERT_FLOAT_WITHIN(2e-6f, 1.0f, (float)ease(EASE_IN_OUT_SINE, 1.0));
  TEST_ASSERT_FLOAT_WITHIN(2e-6f, 0.146447f, (float)ease(EASE_IN_OUT_SINE, 0.25));
  TEST_ASSERT_FLOAT_WITHIN(2e-6f, 0.853553f, (float)ease(EASE_IN_OUT_SINE, 0.75));
}

void test_ease_in_out_sine_does_not_clamp_past_the_end(void) {
  /* unknownpleasures passes height * 1.5; upstream does not clamp, so 1.5
   * lands on 0.5 and the shim must match. */
  TEST_ASSERT_FLOAT_WITHIN(2e-6f, 0.5f, (float)ease(EASE_IN_OUT_SINE, 1.5));
}

void test_ease_in_sine_and_out_sine_are_the_two_halves(void) {
  TEST_ASSERT_FLOAT_WITHIN(2e-6f, 0.0f, (float)ease(EASE_IN_SINE, 0.0));
  TEST_ASSERT_FLOAT_WITHIN(2e-6f, 1.0f, (float)ease(EASE_IN_SINE, 1.0));
  TEST_ASSERT_FLOAT_WITHIN(2e-6f, 0.292893f, (float)ease(EASE_IN_SINE, 0.5));
  TEST_ASSERT_FLOAT_WITHIN(2e-6f, 0.0f, (float)ease(EASE_OUT_SINE, 0.0));
  TEST_ASSERT_FLOAT_WITHIN(2e-6f, 1.0f, (float)ease(EASE_OUT_SINE, 1.0));
  TEST_ASSERT_FLOAT_WITHIN(2e-6f, 0.707107f, (float)ease(EASE_OUT_SINE, 0.5));
}

void test_ease_none_returns_its_input(void) {
  TEST_ASSERT_TRUE(ease(EASE_NONE, 0.3) == 0.3);
  TEST_ASSERT_TRUE(ease(EASE_NONE, 7.0) == 7.0);
}

static double fake_now;
static double fake_clock(void) { return fake_now += 0.25; }

void test_a_replacement_clock_is_used_until_it_is_cleared(void) {
  fake_now = 100.0;
  xshim_set_clock(fake_clock);
  TEST_ASSERT_TRUE(double_time() == 100.25);
  TEST_ASSERT_TRUE(double_time() == 100.5);
  xshim_set_clock(NULL);
  TEST_ASSERT_TRUE_MESSAGE(double_time() > 1e9, "real clock not restored");
}

void test_double_time_is_wall_clock_seconds_and_moves_forward(void) {
  double a = double_time();
  usleep(20000);
  double b = double_time();
  TEST_ASSERT_TRUE_MESSAGE(a > 1e9, "not seconds since the epoch");
  TEST_ASSERT_TRUE_MESSAGE(b - a >= 0.015, "did not advance by the sleep");
  TEST_ASSERT_TRUE_MESSAGE(b - a < 1.0, "advanced by far more than the sleep");
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_create_image_fills_in_geometry_and_computes_the_row_size);
  RUN_TEST(test_create_image_keeps_a_row_size_the_caller_gave);
  RUN_TEST(test_image_pixels_round_trip_through_a_caller_allocated_buffer);
  RUN_TEST(test_image_pixel_outside_the_image_reads_zero_and_writes_nothing);
  RUN_TEST(test_get_pixel_on_an_image_without_data_reads_zero);
  RUN_TEST(test_get_image_copies_a_pixmap_region_as_32_bit_pixels);
  RUN_TEST(test_get_image_from_the_window_reads_the_canvas);
  RUN_TEST(test_get_image_refuses_a_bitmap_format_none_and_an_empty_region);
  RUN_TEST(test_get_image_reads_zero_where_the_region_hangs_off_the_drawable);
  RUN_TEST(test_put_image_writes_the_low_16_bits_into_a_pixmap);
  RUN_TEST(test_put_image_on_the_window_clips_and_marks_the_rows_dirty);
  RUN_TEST(test_mirroring_each_glyph_column_block_through_get_and_put_image);
  RUN_TEST(test_the_device_has_no_keyboard_so_lookup_string_finds_nothing);
  RUN_TEST(test_the_timer_stand_ins_never_fire_and_can_be_cancelled);
  RUN_TEST(test_there_is_never_a_text_source);
  RUN_TEST(test_rgb_to_hsv_inverts_hsv_to_rgb);
  RUN_TEST(test_query_color_recovers_the_colour_alloc_color_made);
  RUN_TEST(test_the_screen_is_a_colour_display_with_no_writable_cells);
  RUN_TEST(test_a_16_bit_image_is_two_bytes_a_pixel_and_keeps_16_bits);
  RUN_TEST(test_utf8_to_latin1_copies_ascii_and_gives_up_on_anything_else);
  RUN_TEST(test_utf8_encode_writes_one_to_four_bytes_and_terminates);
  RUN_TEST(test_utf8_encode_writes_nothing_when_the_character_does_not_fit);
  RUN_TEST(test_utf8_decode_reads_the_first_character_and_says_how_many_bytes);
  RUN_TEST(test_utf8_decode_does_not_read_past_the_length);
  RUN_TEST(test_clipped_fill_draws_only_where_the_mask_has_bits);
  RUN_TEST(test_clipped_fill_clips_to_the_canvas_and_the_rectangle);
  RUN_TEST(test_a_thin_line_on_a_bitmap_sets_every_pixel_of_the_diagonal);
  RUN_TEST(test_a_thin_line_on_a_bitmap_with_foreground_zero_clears_bits_and_clips);
  RUN_TEST(test_a_wide_round_capped_line_on_a_bitmap_is_a_band_with_rounded_ends);
  RUN_TEST(test_thin_lines_on_a_bitmap_run_in_any_direction_and_hit_both_endpoints);
  RUN_TEST(test_a_shallow_thin_line_on_a_bitmap_rounds_halves_up);
  RUN_TEST(test_a_wide_odd_width_line_on_a_bitmap_is_centred_on_its_pixel_row);
  RUN_TEST(test_a_wide_projecting_line_on_a_bitmap_reaches_half_a_width_past_each_end);
  RUN_TEST(test_a_wide_butt_capped_line_on_a_bitmap_ends_square_at_its_endpoints);
  RUN_TEST(test_a_wide_line_on_a_bitmap_clips_at_the_bitmap_edges);
  RUN_TEST(test_get_image_of_a_bitmap_gives_a_one_bit_image_of_its_bits);
  RUN_TEST(test_put_image_of_a_bitmap_onto_a_bitmap_uses_the_low_bit_of_each_colour);
  RUN_TEST(test_copy_plane_between_bitmaps_maps_set_bits_to_foreground_and_clear_to_background);
  RUN_TEST(test_copy_plane_between_bitmaps_honours_the_clip_mask);
  RUN_TEST(test_an_image_reports_the_server_byte_and_bit_order_and_lets_a_hack_set_them);
  RUN_TEST(test_countof_is_the_number_of_elements_of_an_array_not_its_bytes);
  RUN_TEST(test_unmap_notify_has_the_x_event_number_and_fits_an_event_type);
  RUN_TEST(test_i_log2_is_the_floor_of_log_two_and_minus_one_for_zero);
  RUN_TEST(test_to_pow2_rounds_up_to_the_next_power_of_two);
  RUN_TEST(test_bitmap_pad_is_32_bits_and_makes_a_tight_image_row_whole_words);
  RUN_TEST(test_a_bitmap_image_is_one_bit_a_pixel_least_significant_bit_first);
  RUN_TEST(test_put_image_of_a_bitmap_draws_set_bits_in_the_foreground_and_clear_in_the_background);
  RUN_TEST(test_put_image_clips_at_every_edge_and_copies_the_matching_source_pixels);
  RUN_TEST(test_put_image_of_a_16_bit_image_copies_canvas_order_pixels_unchanged);
  RUN_TEST(test_put_image_reads_a_padded_16_bit_source_from_its_offset);
  RUN_TEST(test_an_allocated_colour_keeps_its_pixel_through_a_16_bit_image);
  RUN_TEST(test_the_server_byte_order_is_least_significant_first);
  RUN_TEST(test_a_new_spline_has_zeroed_controls_and_no_points);
  RUN_TEST(test_a_closed_spline_of_a_square_is_a_smooth_loop_inside_it);
  RUN_TEST(test_splines_of_whole_number_controls_match_the_reference);
  RUN_TEST(test_splines_of_fractional_controls_match_the_reference);
  RUN_TEST(test_splines_of_far_off_controls_match_the_reference);
  RUN_TEST(test_splines_of_controls_up_to_a_million_match_the_reference);
  RUN_TEST(test_splines_of_huge_controls_match_the_reference);
  RUN_TEST(test_a_spline_of_more_than_64_controls_matches_the_reference);
  RUN_TEST(test_a_closed_spline_reuses_its_points_and_grows_them_for_more_controls);
  RUN_TEST(test_a_closed_spline_of_one_point_is_that_point);
  RUN_TEST(test_fill_rule_constants_and_the_even_odd_fill_of_a_pentagram);
  RUN_TEST(test_the_screen_stand_in_leads_back_to_the_display_and_its_black_and_white);
  RUN_TEST(test_the_display_a_screen_leads_to_is_the_one_most_recently_opened);
  RUN_TEST(test_closing_displays_out_of_order_never_leaves_a_dangling_current_display);
  RUN_TEST(test_the_gc_function_and_alpha_calls_are_accepted_and_change_nothing);
  RUN_TEST(test_relative_coordinates_fill_the_same_polygon_as_absolute_ones);
  RUN_TEST(test_relative_coordinates_draw_the_same_lines_and_points_as_absolute_ones);
  RUN_TEST(test_a_pixmap_made_from_bitmap_data_reads_least_significant_bit_first);
  RUN_TEST(test_stipple_constants_and_fields_are_accepted_and_fills_stay_solid);
  RUN_TEST(test_create_image_refuses_depths_and_sizes_it_cannot_hold);
  RUN_TEST(test_file_to_ximage_has_no_decoder_and_returns_null);
  RUN_TEST(test_ease_in_out_sine_runs_from_0_to_1_through_one_half);
  RUN_TEST(test_ease_in_out_sine_does_not_clamp_past_the_end);
  RUN_TEST(test_ease_in_sine_and_out_sine_are_the_two_halves);
  RUN_TEST(test_ease_none_returns_its_input);
  RUN_TEST(test_double_time_is_wall_clock_seconds_and_moves_forward);
  RUN_TEST(test_a_replacement_clock_is_used_until_it_is_cleared);
  RUN_TEST(test_release_hook_runs_once_with_the_display_and_is_then_cleared);
  RUN_TEST(test_image_data_to_pixmap_makes_a_colour_pixmap_and_a_mask);
  RUN_TEST(test_image_data_to_pixmap_without_a_mask_pointer_still_loads_the_colours);
  RUN_TEST(test_image_data_to_pixmap_rejects_a_blob_that_is_not_ours);
  RUN_TEST(test_a_deflated_blob_loads_the_same_pixels_and_mask_as_the_plain_one);
  RUN_TEST(test_a_deflated_blob_that_is_corrupt_or_the_wrong_size_is_rejected);
  RUN_TEST(test_a_blob_wider_or_taller_than_int16_max_is_rejected);
  RUN_TEST(test_get_geometry_of_the_window_is_the_canvas_and_of_nothing_is_failure);
  RUN_TEST(test_copy_area_draws_the_image_at_the_destination);
  RUN_TEST(test_copy_area_copies_only_the_requested_source_rectangle);
  RUN_TEST(test_copy_area_is_clipped_at_every_canvas_edge);
  RUN_TEST(test_copy_area_is_clipped_to_the_source_image);
  RUN_TEST(test_clip_mask_draws_only_where_the_mask_is_set);
  RUN_TEST(test_clip_origin_positions_the_mask_and_outside_the_mask_is_not_drawn);
  RUN_TEST(test_clip_mask_outlives_the_pixmap_it_was_set_from);
  RUN_TEST(test_clip_mask_none_removes_the_clip);
  RUN_TEST(test_copy_plane_draws_set_bits_in_foreground_and_clear_bits_in_background);
  RUN_TEST(test_a_gc_made_or_changed_with_a_background_in_its_mask_keeps_it);
  RUN_TEST(test_copy_plane_honours_the_clip_mask);
  RUN_TEST(test_copy_with_the_wrong_kind_of_source_draws_nothing);
  RUN_TEST(test_pixmaps_and_clip_masks_leave_no_memory_behind);
  RUN_TEST(test_free_pixmap_of_none_and_sync_are_harmless);
  RUN_TEST(test_draw_points_plots_each_point_in_the_gc_colour);
  RUN_TEST(test_draw_points_with_zero_count_draws_nothing);
  RUN_TEST(test_change_gc_sets_foreground_only_when_masked);
  RUN_TEST(test_draw_rectangle_outlines_a_w_plus_1_by_h_plus_1_box);
  RUN_TEST(test_make_uniform_colormap_gives_distinct_rgb565_colours);
  RUN_TEST(test_make_smooth_colormap_loops_without_jumps);
  RUN_TEST(test_make_smooth_colormap_is_not_always_a_bright_rainbow);
  RUN_TEST(test_make_smooth_colormap_does_not_always_span_the_hue_wheel);
  RUN_TEST(test_make_smooth_colormap_sometimes_goes_there_and_back);
  RUN_TEST(test_make_color_ramp_runs_from_the_first_colour_to_the_second);
  RUN_TEST(test_make_color_ramp_closed_comes_back_to_the_first_colour);
  RUN_TEST(test_make_color_ramp_takes_the_short_way_round_the_hue_wheel);
  RUN_TEST(test_make_color_ramp_takes_the_short_way_backwards_too);
  RUN_TEST(test_make_color_ramp_blends_saturation_and_value_too);
  RUN_TEST(test_make_color_loop_passes_each_anchor_in_turn_and_closes);
  RUN_TEST(test_make_color_loop_takes_the_short_way_and_blends_value);
  RUN_TEST(test_a_gc_remembers_its_function_but_draws_copy);
  RUN_TEST(test_set_graphics_exposures_is_accepted_and_draws_nothing);
  RUN_TEST(test_load_image_async_simple_finishes_at_once_with_nothing);
  RUN_TEST(test_load_image_async_simple_paints_the_grab_image_covering_the_target);
  RUN_TEST(test_load_image_async_simple_crops_a_tall_grab_image_top_and_bottom);
  RUN_TEST(test_load_image_async_simple_paints_a_pixmap_target_and_not_the_window);
  RUN_TEST(test_load_image_async_simple_ignores_a_grab_image_it_cannot_read);
  RUN_TEST(test_a_load_already_under_way_does_not_paint_again);
  RUN_TEST(test_loading_a_grab_image_leaves_no_memory_behind);
  RUN_TEST(test_threadpool_runs_each_thread_in_turn_on_the_caller);
  RUN_TEST(test_threadpool_create_failure_undoes_the_threads_it_made);
  RUN_TEST(test_one_core_and_a_pointer_sized_alignment_without_threads);
  RUN_TEST(test_the_arrow_and_page_keysyms_have_x_values);
  RUN_TEST(test_visual_depth_is_sixteen);
  RUN_TEST(test_xshm_image_is_a_16_bit_image_put_where_asked);
  RUN_TEST(test_xshm_image_taller_than_the_window_edge_is_clipped);
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
  RUN_TEST(test_clear_window_uses_the_window_background);
  RUN_TEST(test_graphics_functions_have_xs_values);
  RUN_TEST(test_draw_line_and_lines_connect_points);
  RUN_TEST(test_fill_arc_full_circle_draws);
  RUN_TEST(test_fill_arcs_fills_each_full_arc_in_the_gc_colour);
  RUN_TEST(test_fill_polygon_fills_triangle);
  RUN_TEST(test_fill_polygon_draws_with_1000_points);
  RUN_TEST(test_resources_parse_star_and_dot_prefixes_and_tabs);
  RUN_TEST(test_resources_missing_integer_returns_zero);
  RUN_TEST(test_pixel_resource_black_white_hex);
  RUN_TEST(test_gc_accepts_function_and_line_width_fields);
  RUN_TEST(test_a_point_drawn_with_gxxor_flips_the_pixel_and_drawing_it_twice_restores_it);
  RUN_TEST(test_window_attributes_have_depth_visual_screen);
  RUN_TEST(test_button_press_event_fields_exist);
  RUN_TEST(test_xrectangle_type_exists);
  RUN_TEST(test_progname_is_set);
  RUN_TEST(test_overrides_beat_the_hacks_own_defaults);
  RUN_TEST(test_a_class_prefixed_default_is_found);
  RUN_TEST(test_a_plain_default_beats_a_class_prefixed_one_listed_earlier);
  RUN_TEST(test_an_override_beats_a_class_prefixed_default);
  RUN_TEST(test_a_class_prefix_does_not_match_a_longer_or_different_name);
  RUN_TEST(test_progclass_is_set);
  RUN_TEST(test_framework_resources_fall_back_to_builtin_defaults);
  RUN_TEST(test_hack_defaults_beat_the_builtin_fallbacks);
  RUN_TEST(test_pixel_resource_x11_colour_names);
  RUN_TEST(test_pixel_resource_names_are_case_insensitive);
  RUN_TEST(test_erase_window_keeps_the_picture_until_the_wipe_ends);
  RUN_TEST(test_make_random_colormap_fills_colours_in_rgb565);
  RUN_TEST(test_root_window_of_screen_accepts_null_and_names_the_runner_window);
  RUN_TEST(test_select_input_is_accepted_and_ignored);
  RUN_TEST(test_pointer_motion_mask_is_the_x11_bit);
  RUN_TEST(test_window_attributes_have_your_event_mask);
  RUN_TEST(test_ya_rand_init_with_a_seed_repeats_the_sequence);
  RUN_TEST(test_ya_random_follows_the_seeded_sequence_and_is_not_constant);
  RUN_TEST(test_nrand_stays_below_n_and_covers_the_range);
  RUN_TEST(test_lrand_is_non_negative_and_below_maxrand);
  RUN_TEST(test_countof_gives_the_array_length);
  RUN_TEST(test_gettext_and_underscore_return_their_argument);
  RUN_TEST(test_fps_stand_ins_accept_a_null_state);
  RUN_TEST(test_xrm_option_strings_are_writable_like_xscreensavers);
  RUN_TEST(test_parse_color_reads_hex_and_names_into_16_bit_channels);
  RUN_TEST(test_parse_color_rejects_unknown_specs);
  RUN_TEST(test_gc_defaults_are_width_0_butt_miter);
  RUN_TEST(test_create_gc_reads_line_width_cap_and_join_from_the_mask);
  RUN_TEST(test_gc_accepts_a_plane_mask_and_draws_in_the_foreground_regardless);
  RUN_TEST(test_allocate_alpha_colors_reports_no_colour_planes);
  RUN_TEST(test_no_writable_colour_cells_can_be_allocated_or_stored);
  RUN_TEST(test_change_gc_updates_only_the_masked_line_fields);
  RUN_TEST(test_set_line_attributes_sets_all_three);
  RUN_TEST(test_wide_horizontal_line_butt_caps);
  RUN_TEST(test_wide_even_width_starts_half_before);
  RUN_TEST(test_wide_vertical_line_butt_caps);
  RUN_TEST(test_projecting_caps_extend_half_the_width);
  RUN_TEST(test_round_caps_width_6_have_tip_but_no_corner);
  RUN_TEST(test_diagonal_wide_line_covers_the_centre_line_and_is_about_width_thick);
  RUN_TEST(test_zero_length_wide_line_round_cap_is_a_dot_butt_is_nothing_wider_than_a_point);
  RUN_TEST(test_round_cap_dot_matches_canvas_fill_ellipse_at_every_width_and_position);
  RUN_TEST(test_wide_line_far_off_canvas_draws_nothing_and_does_not_overflow);
  RUN_TEST(test_wide_line_across_the_whole_int_range_still_crosses_the_canvas);
  RUN_TEST(test_an_enormous_width_is_bounded_not_a_hang_or_overflow);
  RUN_TEST(test_wide_line_clips_at_the_canvas_edge_and_marks_dirty_rows);
  RUN_TEST(test_diagonal_wide_line_marks_dirty_rows_it_touches);
  RUN_TEST(test_width_0_and_1_match_the_old_line);
  RUN_TEST(test_a_45_degree_wide_line_is_symmetric_about_its_diagonal);
  RUN_TEST(test_an_enormous_width_with_round_caps_neither_hangs_nor_overflows);
  RUN_TEST(test_miter_join_fills_the_outer_corner);
  RUN_TEST(test_bevel_join_leaves_the_outer_corner_unset);
  RUN_TEST(test_round_join_fills_a_disc_at_the_vertex);
  RUN_TEST(test_sharp_angle_miter_falls_back_to_bevel);
  RUN_TEST(test_polyline_with_two_points_equals_a_segment);
  RUN_TEST(test_polyline_n_0_and_1_draw_nothing_wide);
  RUN_TEST(test_repeated_points_do_not_change_the_picture);
  RUN_TEST(test_180_degree_turn_has_no_gap);
  RUN_TEST(test_collinear_points_draw_the_same_as_one_segment);
  RUN_TEST(test_collinear_points_with_projecting_caps_draw_the_same_as_one_segment);
  RUN_TEST(test_a_slanted_projecting_cap_reaches_half_the_width_past_the_end);
  RUN_TEST(test_draw_rectangle_wide_has_mitered_corners);
  RUN_TEST(test_width_0_and_1_polyline_and_rectangle_match_the_old_ones);
  RUN_TEST(test_draw_segments_draws_each_segment_independently);
  RUN_TEST(test_draw_segments_with_zero_count_draws_nothing);
  RUN_TEST(test_draw_segments_wide_uses_caps_not_joins);
  RUN_TEST(test_small_arcs_match_the_reference);
  RUN_TEST(test_arcs_of_any_size_match_the_reference);
  RUN_TEST(test_arcs_with_huge_angles_match_the_reference);
  RUN_TEST(test_arcs_with_huge_coordinates_and_sizes_match_the_reference);
  RUN_TEST(test_arcs_with_coordinates_up_to_a_million_match_the_reference);
  RUN_TEST(test_an_arc_far_from_the_canvas_matches_the_reference_at_every_point);
  RUN_TEST(test_arc_points_lie_within_a_pixel_of_the_ellipse);
  RUN_TEST(test_arc_points_start_and_end_at_the_requested_angles);
  RUN_TEST(test_negative_angle2_runs_clockwise);
  RUN_TEST(test_angle2_above_360_degrees_is_a_full_ellipse);
  RUN_TEST(test_draw_arc_quarter_draws_only_that_quadrant);
  RUN_TEST(test_draw_arc_full_ellipse_is_a_closed_outline);
  RUN_TEST(test_draw_arc_wide_uses_the_stroke_code);
  RUN_TEST(test_stroke_circle_fills_the_band_between_its_two_radii);
  RUN_TEST(test_stroke_circle_covers_what_the_wide_arc_draws);
  RUN_TEST(test_wide_full_arc_fills_every_pixel_in_its_band);
  RUN_TEST(test_stroke_circle_narrower_than_its_line_is_a_filled_disc);
  RUN_TEST(test_stroke_circle_is_clipped_to_the_canvas);
  RUN_TEST(test_wide_line_with_alpha_blends_over_the_canvas);
  RUN_TEST(test_wide_line_without_alpha_still_overwrites);
  RUN_TEST(test_stroke_circle_with_alpha_blends_each_pixel_once);
  RUN_TEST(test_wide_strokes_of_random_lines_match_the_pinned_hash);
  RUN_TEST(test_alpha_does_not_leak_into_the_next_gc);
  RUN_TEST(test_polyline_with_alpha_blends_a_join_once);
  RUN_TEST(test_polyline_with_alpha_blends_a_self_crossing_once);
  RUN_TEST(test_round_capped_segment_with_alpha_blends_its_cap_once);
  RUN_TEST(test_wide_round_cap_with_alpha_blends_like_the_body);
  RUN_TEST(test_fill_arc_half_fills_a_half_disc);
  RUN_TEST(test_fill_arc_quarter_is_a_pie_slice_including_the_centre);
  RUN_TEST(test_create_pixmap_is_zeroed_and_reports_its_geometry);
  RUN_TEST(test_create_pixmap_rejects_zero_and_huge_sizes);
  RUN_TEST(test_copy_area_from_the_window_to_itself_apart);
  RUN_TEST(test_copy_area_from_the_window_to_itself_overlapping_each_way);
  RUN_TEST(test_copy_area_from_the_window_clips_at_every_edge);
  RUN_TEST(test_copy_area_from_the_window_marks_the_rows_it_wrote);
  RUN_TEST(test_copy_area_from_the_window_into_a_pixmap_and_back);
  RUN_TEST(test_image_data_to_ximage_gives_rgba_bytes_bottom_row_first);
  RUN_TEST(test_image_data_to_ximage_refuses_a_bad_blob);
  RUN_TEST(test_copy_area_into_a_colour_pixmap_then_out_to_the_canvas);
  RUN_TEST(test_copy_area_into_a_bitmap_copies_bits);
  RUN_TEST(test_copy_area_between_different_depths_does_nothing);
  RUN_TEST(test_copy_area_clips_at_destination_edges);
  RUN_TEST(test_copy_area_with_a_clip_mask_skips_masked_pixels);
  RUN_TEST(test_scale_pixmap_loop_like_pacman_fills_the_destination);
  RUN_TEST(test_fill_rectangle_into_a_pixmap_lands_in_the_pixmap);
  RUN_TEST(test_every_primitive_draws_the_same_into_a_pixmap_as_on_the_window);
  RUN_TEST(test_drawing_into_a_pixmap_clips_at_the_pixmap_edges);
  RUN_TEST(test_primitives_on_none_draw_nothing_and_a_bitmap_line_stays_off_the_window);
  RUN_TEST(test_copy_plane_into_a_colour_pixmap_lands_in_the_pixmap);
  RUN_TEST(test_root_window_is_the_window_the_shim_draws_on);
  RUN_TEST(test_create_pixmap_wider_than_int16_returns_none);
  RUN_TEST(test_pixmaps_drawn_into_leave_no_memory_behind);
  RUN_TEST(test_copy_area_within_one_pixmap_downwards_reads_before_it_overwrites);
  RUN_TEST(test_copy_area_from_a_pixmap_marks_only_the_copied_rows_dirty);
  RUN_TEST(test_copy_area_within_one_pixmap_reads_before_it_overwrites);
  RUN_TEST(test_copy_area_into_a_pixmap_that_failed_to_allocate_leaves_the_canvas_alone);
  RUN_TEST(test_set_fill_style_is_accepted_and_drawing_stays_solid);
  RUN_TEST(test_query_pointer_reports_no_pointer);
  RUN_TEST(test_release_pixmaps_frees_every_live_pixmap_once);
  RUN_TEST(test_close_display_releases_pixmaps_a_hack_left_behind);
  RUN_TEST(test_pixmaps_freed_in_any_order_unlink_cleanly);
  RUN_TEST(test_clear_area_clears_only_its_rectangle_to_the_background);
  RUN_TEST(test_clear_area_of_zero_size_extends_to_the_window_edge);
  RUN_TEST(test_clear_area_clips_a_negative_origin_and_a_huge_size);
  RUN_TEST(test_fill_rectangle_in_a_bitmap_sets_its_bits_and_leaves_the_window);
  RUN_TEST(test_fill_rectangle_in_a_bitmap_with_foreground_zero_clears_bits);
  RUN_TEST(test_fill_rectangle_in_a_bitmap_clips_to_the_bitmap);
  RUN_TEST(test_fill_polygon_in_a_bitmap_fills_a_triangle_by_pixel_centres);
  RUN_TEST(test_fill_polygon_in_a_bitmap_starts_each_row_at_its_left_edge);
  RUN_TEST(test_fill_polygon_in_a_bitmap_leaves_a_notch_empty);
  RUN_TEST(test_fill_polygon_in_a_bitmap_clips_to_the_bitmap);
  RUN_TEST(test_dashed_line_styles_draw_solid);
  RUN_TEST(test_max_request_size_leaves_room_for_long_polylines);
  RUN_TEST(test_fill_arc_full_still_matches_the_old_ellipse);
  RUN_TEST(test_zero_size_and_zero_sweep_arcs_draw_nothing_much);
  RUN_TEST(test_huge_arcs_return_without_hanging_or_overflowing);
  return UNITY_END();
}
