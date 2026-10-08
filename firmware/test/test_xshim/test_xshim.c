#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include "core/canvas.h"
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

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_white_and_black_pixel);
  RUN_TEST(test_alloc_color_red_gives_f800_and_white_gives_ffff);
  RUN_TEST(test_foreground_via_gc_used_by_fill_rectangle);
  RUN_TEST(test_get_window_attributes_reports_canvas_size);
  RUN_TEST(test_clear_window_uses_black_background);
  RUN_TEST(test_draw_line_and_lines_connect_points);
  RUN_TEST(test_fill_arc_full_circle_draws_partial_arc_does_not);
  RUN_TEST(test_fill_polygon_fills_triangle);
  RUN_TEST(test_resources_parse_star_and_dot_prefixes_and_tabs);
  RUN_TEST(test_resources_missing_integer_returns_zero);
  RUN_TEST(test_pixel_resource_black_white_hex);
  return UNITY_END();
}
