#include <limits.h>
#include <math.h>
#include <sanitizer/allocator_interface.h>
#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include "colors.h"
#include "core/canvas.h"
#include "erase.h"
#include "fps.h"
#include "screenhack.h"
#include "utils.h"
#include "x11shim/arc.h"
#include "x11shim/xshim.h"
#include "ximage-loader.h"

static Canvas cv;
static Display *dpy;
static Window win = 1;

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

void test_drawing_primitive_with_a_pixmap_drawable_still_draws_to_the_canvas(void) {
  Pixmap p = XCreatePixmap(dpy, win, 8, 8, 16);
  GC gc = new_gc(0xFFFF, 0);
  XFillRectangle(dpy, p, gc, 1, 1, 2, 2);
  TEST_ASSERT_EQUAL_INT(4, count_set());
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

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_image_data_to_pixmap_makes_a_colour_pixmap_and_a_mask);
  RUN_TEST(test_image_data_to_pixmap_rejects_a_blob_that_is_not_ours);
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
  RUN_TEST(test_fill_arc_full_circle_draws);
  RUN_TEST(test_fill_arcs_fills_each_full_arc_in_the_gc_colour);
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
  RUN_TEST(test_overrides_beat_the_hacks_own_defaults);
  RUN_TEST(test_progclass_is_set);
  RUN_TEST(test_framework_resources_fall_back_to_builtin_defaults);
  RUN_TEST(test_hack_defaults_beat_the_builtin_fallbacks);
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
  RUN_TEST(test_parse_color_reads_hex_and_names_into_16_bit_channels);
  RUN_TEST(test_parse_color_rejects_unknown_specs);
  RUN_TEST(test_gc_defaults_are_width_0_butt_miter);
  RUN_TEST(test_create_gc_reads_line_width_cap_and_join_from_the_mask);
  RUN_TEST(test_change_gc_updates_only_the_masked_line_fields);
  RUN_TEST(test_set_line_attributes_sets_all_three);
  RUN_TEST(test_wide_horizontal_line_butt_caps);
  RUN_TEST(test_wide_even_width_starts_half_before);
  RUN_TEST(test_wide_vertical_line_butt_caps);
  RUN_TEST(test_projecting_caps_extend_half_the_width);
  RUN_TEST(test_round_caps_width_6_have_tip_but_no_corner);
  RUN_TEST(test_diagonal_wide_line_covers_the_centre_line_and_is_about_width_thick);
  RUN_TEST(test_zero_length_wide_line_round_cap_is_a_dot_butt_is_nothing_wider_than_a_point);
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
  RUN_TEST(test_arc_points_lie_within_a_pixel_of_the_ellipse);
  RUN_TEST(test_arc_points_start_and_end_at_the_requested_angles);
  RUN_TEST(test_negative_angle2_runs_clockwise);
  RUN_TEST(test_angle2_above_360_degrees_is_a_full_ellipse);
  RUN_TEST(test_draw_arc_quarter_draws_only_that_quadrant);
  RUN_TEST(test_draw_arc_full_ellipse_is_a_closed_outline);
  RUN_TEST(test_draw_arc_wide_uses_the_stroke_code);
  RUN_TEST(test_fill_arc_half_fills_a_half_disc);
  RUN_TEST(test_fill_arc_quarter_is_a_pie_slice_including_the_centre);
  RUN_TEST(test_create_pixmap_is_zeroed_and_reports_its_geometry);
  RUN_TEST(test_create_pixmap_rejects_zero_and_huge_sizes);
  RUN_TEST(test_copy_area_into_a_colour_pixmap_then_out_to_the_canvas);
  RUN_TEST(test_copy_area_into_a_bitmap_copies_bits);
  RUN_TEST(test_copy_area_between_different_depths_does_nothing);
  RUN_TEST(test_copy_area_clips_at_destination_edges);
  RUN_TEST(test_copy_area_with_a_clip_mask_skips_masked_pixels);
  RUN_TEST(test_scale_pixmap_loop_like_pacman_fills_the_destination);
  RUN_TEST(test_drawing_primitive_with_a_pixmap_drawable_still_draws_to_the_canvas);
  RUN_TEST(test_copy_area_within_one_pixmap_reads_before_it_overwrites);
  RUN_TEST(test_set_fill_style_is_accepted_and_drawing_stays_solid);
  RUN_TEST(test_query_pointer_reports_no_pointer);
  RUN_TEST(test_release_pixmaps_frees_every_live_pixmap_once);
  RUN_TEST(test_close_display_releases_pixmaps_a_hack_left_behind);
  RUN_TEST(test_pixmaps_freed_in_any_order_unlink_cleanly);
  RUN_TEST(test_fill_arc_full_still_matches_the_old_ellipse);
  RUN_TEST(test_zero_size_and_zero_sweep_arcs_draw_nothing_much);
  RUN_TEST(test_huge_arcs_return_without_hanging_or_overflowing);
  return UNITY_END();
}
