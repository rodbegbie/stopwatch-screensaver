#include <sanitizer/allocator_interface.h>
#include <stdio.h>
#include <stdlib.h>
#include <unity.h>

#include "core/canvas.h"
#include "hacks/halo_planes.h"
#include "x11shim/xshim.h"

static Canvas cv;
static Display *dpy;
static size_t live_before;

void setUp(void) {
  live_before = __sanitizer_get_current_allocated_bytes();
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&cv, 40, 40, malloc));
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

static GC gc_with(unsigned long fg, unsigned long bg) {
  XGCValues v = {0};
  GC gc = XCreateGC(dpy, XSHIM_WINDOW, 0, &v);
  XSetForeground(dpy, gc, fg);
  XSetBackground(dpy, gc, bg);
  return gc;
}

static HaloPlane *plane(Pixmap p) { return halo_plane_of(p); }

static int count_set(const HaloPlane *p) {
  int n = 0;
  for (int y = 0; y < p->h; y++)
    for (int x = 0; x < p->w; x++) n += halo_get_bit(p, x, y);
  return n;
}

void test_a_new_plane_is_clear_and_a_zero_size_is_refused(void) {
  Pixmap p = halo_create_pixmap(dpy, XSHIM_WINDOW, 13, 5, 1);
  TEST_ASSERT_NOT_EQUAL(None, p);
  TEST_ASSERT_EQUAL_INT(13, plane(p)->w);
  TEST_ASSERT_EQUAL_INT(2, plane(p)->stride);
  TEST_ASSERT_EQUAL_INT(0, count_set(plane(p)));
  halo_free_pixmap(dpy, p);
  TEST_ASSERT_EQUAL_UINT64(None, halo_create_pixmap(dpy, XSHIM_WINDOW, 0, 5, 1));
}

void test_a_rectangle_sets_and_clears_bits_across_byte_edges_and_clips(void) {
  Pixmap p = halo_create_pixmap(dpy, XSHIM_WINDOW, 20, 6, 1);
  GC on = gc_with(1, 0), off = gc_with(0, 0);
  halo_fill_rectangle(dpy, p, on, 3, 1, 14, 3);
  TEST_ASSERT_EQUAL_INT(14 * 3, count_set(plane(p)));
  TEST_ASSERT_EQUAL_INT(0, halo_get_bit(plane(p), 2, 1));
  TEST_ASSERT_EQUAL_INT(1, halo_get_bit(plane(p), 3, 1));
  TEST_ASSERT_EQUAL_INT(1, halo_get_bit(plane(p), 16, 3));
  TEST_ASSERT_EQUAL_INT(0, halo_get_bit(plane(p), 17, 3));
  halo_fill_rectangle(dpy, p, off, 8, 2, 1, 1);
  TEST_ASSERT_EQUAL_INT(0, halo_get_bit(plane(p), 8, 2));
  TEST_ASSERT_EQUAL_INT(14 * 3 - 1, count_set(plane(p)));
  halo_fill_rectangle(dpy, p, on, -50, -50, 500, 500);
  TEST_ASSERT_EQUAL_INT(20 * 6, count_set(plane(p)));
  halo_fill_rectangle(dpy, p, off, 18, 4, 100, 100);
  TEST_ASSERT_EQUAL_INT(20 * 6 - 2 * 2, count_set(plane(p)));
  XFreeGC(dpy, on);
  XFreeGC(dpy, off);
  halo_free_pixmap(dpy, p);
}

/* The shim's own fill of the same circle is the reference. */
static void check_circle_matches_the_shim(int x, int y, unsigned w, unsigned h) {
  Pixmap p = halo_create_pixmap(dpy, XSHIM_WINDOW, 40, 40, 1);
  GC on = gc_with(1, 0), white = gc_with(0xFFFF, 0);
  canvas_clear(&cv, 0);
  XFillArc(dpy, XSHIM_WINDOW, white, x, y, w, h, 0, 360 * 64);
  halo_fill_arc(dpy, p, on, x, y, w, h, 0, 360 * 64);
  for (int yy = 0; yy < 40; yy++)
    for (int xx = 0; xx < 40; xx++)
      if ((cv.px[yy * 40 + xx] != 0) != halo_get_bit(plane(p), xx, yy)) {
        char msg[96];
        snprintf(msg, sizeof msg, "circle %d,%d %ux%u differs at %d,%d", x, y, w, h, xx, yy);
        TEST_FAIL_MESSAGE(msg);
      }
  XFreeGC(dpy, on);
  XFreeGC(dpy, white);
  halo_free_pixmap(dpy, p);
}

void test_a_circle_in_a_plane_matches_the_shims_pixels(void) {
  check_circle_matches_the_shim(5, 5, 20, 20);
  check_circle_matches_the_shim(-3, 2, 17, 17);
  check_circle_matches_the_shim(30, 30, 30, 30);
  check_circle_matches_the_shim(10, 10, 1, 1);
  check_circle_matches_the_shim(0, 0, 2, 2);
  check_circle_matches_the_shim(7, 3, 13, 13);
  check_circle_matches_the_shim(4, 6, 21, 10);
  check_circle_matches_the_shim(-100, -100, 300, 300);
  check_circle_matches_the_shim(20, 20, 0, 5);
}

void test_a_circle_on_the_window_matches_the_shims_pixels(void) {
  GC white = gc_with(0xFFFF, 0);
  static uint16_t want[40 * 40];
  for (int i = 0; i < 6; i++) {
    const int x = i * 5 - 4, y = i * 3 - 2, d = 9 + i * 4;
    canvas_clear(&cv, 0);
    XFillArc(dpy, XSHIM_WINDOW, white, x, y, d, d, 0, 360 * 64);
    memcpy(want, cv.px, sizeof want);
    canvas_clear(&cv, 0);
    halo_fill_arc(dpy, XSHIM_WINDOW, white, x, y, d, d, 0, 360 * 64);
    TEST_ASSERT_EQUAL_MEMORY(want, cv.px, sizeof want);
  }
  XFreeGC(dpy, white);
}

static Pixmap plane_from_rows(unsigned w, unsigned h, const char *const *rows) {
  Pixmap p = halo_create_pixmap(dpy, XSHIM_WINDOW, w, h, 1);
  GC on = gc_with(1, 0);
  for (unsigned y = 0; y < h; y++)
    for (unsigned x = 0; x < w; x++)
      if (rows[y][x] == '#') halo_fill_rectangle(dpy, p, on, x, y, 1, 1);
  XFreeGC(dpy, on);
  return p;
}

void test_copy_between_planes_flips_bits_aligned_and_unaligned(void) {
  static const char *const a[] = {"##..####..", "#.#.#..#.#"};
  static const char *const b[] = {"#.#.#.#.#.", ".#.#.#.#.#"};
  Pixmap src = plane_from_rows(10, 2, a);
  Pixmap dst = plane_from_rows(10, 2, b);
  GC gc = gc_with(1, 0);
  halo_copy_plane(dpy, src, dst, gc, 0, 0, 10, 2, 0, 0, 1);
  static const char *const want_aligned[] = {".##..#.##.", "######...."};
  for (int y = 0; y < 2; y++)
    for (int x = 0; x < 10; x++)
      TEST_ASSERT_EQUAL_INT(want_aligned[y][x] == '#', halo_get_bit(plane(dst), x, y));
  halo_copy_plane(dpy, src, dst, gc, 0, 0, 10, 2, 0, 0, 1);
  for (int y = 0; y < 2; y++)
    for (int x = 0; x < 10; x++)
      TEST_ASSERT_EQUAL_INT(b[y][x] == '#', halo_get_bit(plane(dst), x, y));
  halo_copy_plane(dpy, src, dst, gc, 1, 0, 6, 1, 3, 1, 1);
  for (int i = 0; i < 6; i++) {
    const int want = (b[1][3 + i] == '#') ^ (a[0][1 + i] == '#');
    TEST_ASSERT_EQUAL_INT(want, halo_get_bit(plane(dst), 3 + i, 1));
  }
  halo_copy_plane(dpy, src, dst, gc, -4, -4, 100, 100, 5, 1, 1);
  XFreeGC(dpy, gc);
  halo_free_pixmap(dpy, src);
  halo_free_pixmap(dpy, dst);
}

void test_copy_to_the_window_paints_set_bits_in_the_foreground_and_clear_in_the_background(void) {
  static const char *const rows[] = {"#.#.#.#.##", "#########.", "..........", "#........#"};
  Pixmap p = plane_from_rows(10, 4, rows);
  GC gc = gc_with(0x1234, 0x00F0);
  halo_copy_plane(dpy, p, XSHIM_WINDOW, gc, 0, 0, 10, 4, 2, 3, 1);
  for (int y = 0; y < 4; y++)
    for (int x = 0; x < 10; x++)
      TEST_ASSERT_EQUAL_HEX(rows[y][x] == '#' ? 0x1234 : 0x00F0,
                            cv.px[(3 + y) * 40 + 2 + x]);
  TEST_ASSERT_EQUAL_HEX(0, cv.px[0]);
  int x0, x1;
  TEST_ASSERT_TRUE(canvas_dirty_row(&cv, 3, &x0, &x1));
  TEST_ASSERT_EQUAL_INT(2, x0);
  TEST_ASSERT_EQUAL_INT(11, x1);
  XFreeGC(dpy, gc);
  halo_free_pixmap(dpy, p);
}

void test_a_long_run_of_bytes_expands_to_the_window_in_both_colours(void) {
  Pixmap p = halo_create_pixmap(dpy, XSHIM_WINDOW, 40, 2, 1);
  GC on = gc_with(1, 0);
  halo_fill_rectangle(dpy, p, on, 8, 0, 16, 1);
  GC gc = gc_with(0xAAAA, 0x5555);
  halo_copy_plane(dpy, p, XSHIM_WINDOW, gc, 0, 0, 40, 2, 0, 0, 1);
  for (int x = 0; x < 40; x++) {
    TEST_ASSERT_EQUAL_HEX((x >= 8 && x < 24) ? 0xAAAA : 0x5555, cv.px[x]);
    TEST_ASSERT_EQUAL_HEX(0x5555, cv.px[40 + x]);
  }
  XFreeGC(dpy, gc);
  XFreeGC(dpy, on);
  halo_free_pixmap(dpy, p);
}

void test_a_copy_hanging_off_every_edge_stays_inside_both_buffers(void) {
  Pixmap p = halo_create_pixmap(dpy, XSHIM_WINDOW, 4, 4, 1);
  GC on = gc_with(1, 0);
  halo_fill_rectangle(dpy, p, on, 0, 0, 4, 4);
  GC gc = gc_with(0xFFFF, 0);
  halo_copy_plane(dpy, p, XSHIM_WINDOW, gc, -2, -2, 100, 100, -1, -1, 1);
  halo_copy_plane(dpy, p, XSHIM_WINDOW, gc, 3, 3, 100, 100, 38, 38, 1);
  TEST_ASSERT_EQUAL_HEX(0, cv.px[0]);
  TEST_ASSERT_EQUAL_HEX(0xFFFF, cv.px[1 * 40 + 1]);
  TEST_ASSERT_EQUAL_HEX(0xFFFF, cv.px[38 * 40 + 38]);
  TEST_ASSERT_EQUAL_HEX(0, cv.px[39 * 40 + 39]);
  XFreeGC(dpy, gc);
  XFreeGC(dpy, on);
  halo_free_pixmap(dpy, p);
}

/* An overrun past the right edge wraps onto the next row, which
 * AddressSanitizer cannot see, so the test looks at that row. */
void test_a_copy_past_the_right_edge_does_not_wrap_onto_the_next_row(void) {
  Pixmap p = halo_create_pixmap(dpy, XSHIM_WINDOW, 8, 1, 1);
  GC on = gc_with(1, 0);
  halo_fill_rectangle(dpy, p, on, 0, 0, 8, 1);
  GC gc = gc_with(0xFFFF, 0x0001);
  halo_copy_plane(dpy, p, XSHIM_WINDOW, gc, 0, 0, 8, 1, 36, 0, 1);
  TEST_ASSERT_EQUAL_HEX(0xFFFF, cv.px[39]);
  TEST_ASSERT_EQUAL_HEX(0, cv.px[40]);
  TEST_ASSERT_EQUAL_HEX(0, cv.px[41]);
  XFreeGC(dpy, gc);
  XFreeGC(dpy, on);
  halo_free_pixmap(dpy, p);
}

/* Halo makes its window GCs with the colours in the creation mask, as here,
 * not with XSetBackground. */
void test_clear_bits_take_the_background_a_gc_was_created_with(void) {
  Pixmap p = halo_create_pixmap(dpy, XSHIM_WINDOW, 8, 1, 1);
  XGCValues v = {0};
  v.foreground = 0x1234;
  v.background = 0x5678;
  GC gc = XCreateGC(dpy, XSHIM_WINDOW, GCForeground | GCBackground, &v);
  halo_copy_plane(dpy, p, XSHIM_WINDOW, gc, 0, 0, 8, 1, 0, 0, 1);
  TEST_ASSERT_EQUAL_HEX(0x5678, cv.px[0]);
  XFreeGC(dpy, gc);
  halo_free_pixmap(dpy, p);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_a_new_plane_is_clear_and_a_zero_size_is_refused);
  RUN_TEST(test_a_rectangle_sets_and_clears_bits_across_byte_edges_and_clips);
  RUN_TEST(test_a_circle_in_a_plane_matches_the_shims_pixels);
  RUN_TEST(test_a_circle_on_the_window_matches_the_shims_pixels);
  RUN_TEST(test_copy_between_planes_flips_bits_aligned_and_unaligned);
  RUN_TEST(test_copy_to_the_window_paints_set_bits_in_the_foreground_and_clear_in_the_background);
  RUN_TEST(test_a_long_run_of_bytes_expands_to_the_window_in_both_colours);
  RUN_TEST(test_a_copy_hanging_off_every_edge_stays_inside_both_buffers);
  RUN_TEST(test_a_copy_past_the_right_edge_does_not_wrap_onto_the_next_row);
  RUN_TEST(test_clear_bits_take_the_background_a_gc_was_created_with);
  return UNITY_END();
}
