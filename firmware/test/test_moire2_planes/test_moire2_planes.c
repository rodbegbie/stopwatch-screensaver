#include <sanitizer/allocator_interface.h>
#include <stdio.h>
#include <stdlib.h>
#include <unity.h>

#include "core/canvas.h"
#include "hacks/moire2_planes.h"
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

static GC gc_with(unsigned long fg, int function, int line_width) {
  XGCValues v = {0};
  GC gc = XCreateGC(dpy, XSHIM_WINDOW, 0, &v);
  XSetForeground(dpy, gc, fg);
  XSetFunction(dpy, gc, function);
  gc->line_width = line_width;
  return gc;
}

static HaloPlane *plane(Pixmap p) { return halo_plane_of(p); }

static int count_set(const HaloPlane *p) {
  int n = 0;
  for (int y = 0; y < p->h; y++)
    for (int x = 0; x < p->w; x++) n += halo_get_bit(p, x, y);
  return n;
}

static void set_bit(HaloPlane *p, int x, int y) {
  p->bits[(size_t)y * (size_t)p->stride + (size_t)(x >> 3)] |=
      (uint8_t)(0x80 >> (x & 7));
}

static void scatter(HaloPlane *p, unsigned seed) {
  srandom(seed);
  for (int y = 0; y < p->h; y++)
    for (int x = 0; x < p->w; x++)
      if (random() & 1) set_bit(p, x, y);
}

void test_xor_fill_flips_the_rectangle_and_a_zero_foreground_changes_nothing(void) {
  Pixmap p = halo_create_pixmap(dpy, XSHIM_WINDOW, 20, 6, 1);
  GC on = gc_with(1, GXcopy, 0), flip = gc_with(1, GXxor, 0),
     nothing = gc_with(0, GXxor, 0);
  halo_fill_rectangle(dpy, p, on, 3, 1, 8, 2);
  moire2_fill_rectangle(dpy, p, flip, 0, 0, 20, 6);
  TEST_ASSERT_EQUAL_INT(20 * 6 - 8 * 2, count_set(plane(p)));
  TEST_ASSERT_EQUAL_INT(0, halo_get_bit(plane(p), 3, 1));
  TEST_ASSERT_EQUAL_INT(1, halo_get_bit(plane(p), 2, 1));
  moire2_fill_rectangle(dpy, p, nothing, 0, 0, 20, 6);
  TEST_ASSERT_EQUAL_INT(20 * 6 - 8 * 2, count_set(plane(p)));
  moire2_fill_rectangle(dpy, p, flip, 5, 2, 100, 100);
  TEST_ASSERT_EQUAL_INT(0, halo_get_bit(plane(p), 4, 2));
  TEST_ASSERT_EQUAL_INT(1, halo_get_bit(plane(p), 5, 2));
  TEST_ASSERT_EQUAL_INT(0, halo_get_bit(plane(p), 5, 1));
  XFreeGC(dpy, on);
  XFreeGC(dpy, flip);
  XFreeGC(dpy, nothing);
  halo_free_pixmap(dpy, p);
}

void test_an_xor_fill_ending_inside_a_byte_leaves_the_bits_after_it(void) {
  Pixmap p = halo_create_pixmap(dpy, XSHIM_WINDOW, 20, 6, 1);
  GC flip = gc_with(1, GXxor, 0);
  moire2_fill_rectangle(dpy, p, flip, 1, 0, 10, 2);
  TEST_ASSERT_EQUAL_INT(20, count_set(plane(p)));
  TEST_ASSERT_EQUAL_INT(0, halo_get_bit(plane(p), 0, 0));
  TEST_ASSERT_EQUAL_INT(1, halo_get_bit(plane(p), 10, 1));
  TEST_ASSERT_EQUAL_INT(0, halo_get_bit(plane(p), 11, 1));
  XFreeGC(dpy, flip);
  halo_free_pixmap(dpy, p);
}

void test_a_copy_fill_sets_and_clears_like_halos(void) {
  Pixmap p = halo_create_pixmap(dpy, XSHIM_WINDOW, 20, 6, 1);
  GC on = gc_with(1, GXcopy, 0), off = gc_with(0, GXcopy, 0);
  moire2_fill_rectangle(dpy, p, on, 0, 0, 20, 6);
  TEST_ASSERT_EQUAL_INT(120, count_set(plane(p)));
  moire2_fill_rectangle(dpy, p, off, 4, 1, 5, 2);
  TEST_ASSERT_EQUAL_INT(110, count_set(plane(p)));
  XFreeGC(dpy, on);
  XFreeGC(dpy, off);
  halo_free_pixmap(dpy, p);
}

void test_a_fill_on_the_window_still_reaches_the_shim(void) {
  GC red = gc_with(0xF800, GXcopy, 0);
  moire2_fill_rectangle(dpy, XSHIM_WINDOW, red, 1, 1, 2, 2);
  TEST_ASSERT_EQUAL_HEX(0xF800, cv.px[1 * cv.w + 1]);
  XFreeGC(dpy, red);
}

/* The width of the ring along the horizontal line through the centre, on the
 * left side. */
static int ring_width_on_the_left(const HaloPlane *p, int cx) {
  const int cy = p->h / 2;
  int n = 0;
  for (int x = 0; x < cx; x++) n += halo_get_bit(p, x, cy);
  return n;
}

void test_a_circle_outline_is_one_ring_as_thick_as_the_line_width(void) {
  for (int t = 1; t <= 4; t++) {
    Pixmap p = halo_create_pixmap(dpy, XSHIM_WINDOW, 101, 101, 1);
    GC gc = gc_with(1, GXcopy, t == 1 ? 0 : t);
    moire2_draw_arc(dpy, p, gc, 10, 10, 81, 81, 0, 360 * 64);
    char msg[32];
    snprintf(msg, sizeof msg, "thickness %d", t);
    TEST_ASSERT_EQUAL_INT_MESSAGE(t, ring_width_on_the_left(plane(p), 50), msg);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, halo_get_bit(plane(p), 50, 50), msg);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, halo_get_bit(plane(p), 3, 3), msg);
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, halo_get_bit(plane(p), 50, 5), msg);
    TEST_ASSERT_TRUE_MESSAGE(halo_get_bit(plane(p), 50, 10) ||
                                 halo_get_bit(plane(p), 50, 11),
                             msg);
    XFreeGC(dpy, gc);
    halo_free_pixmap(dpy, p);
  }
}

void test_a_circle_outline_is_symmetric_and_closed(void) {
  Pixmap p = halo_create_pixmap(dpy, XSHIM_WINDOW, 101, 101, 1);
  GC gc = gc_with(1, GXcopy, 2);
  moire2_draw_arc(dpy, p, gc, 10, 10, 81, 81, 0, 360 * 64);
  for (int y = 0; y < 101; y++)
    for (int x = 0; x < 101; x++) {
      TEST_ASSERT_EQUAL_INT(halo_get_bit(plane(p), x, y),
                            halo_get_bit(plane(p), 100 - x, 100 - y));
      TEST_ASSERT_EQUAL_INT(halo_get_bit(plane(p), x, y),
                            halo_get_bit(plane(p), y, x));
    }
  for (int y = 0; y < 101; y++) {
    int first = -1, last = -1;
    for (int x = 0; x < 101; x++)
      if (halo_get_bit(plane(p), x, y)) {
        if (first < 0) first = x;
        last = x;
      }
    if (first >= 0) TEST_ASSERT_TRUE(last > first);
  }
  XFreeGC(dpy, gc);
  halo_free_pixmap(dpy, p);
}

void test_an_ellipse_outline_follows_its_own_width_and_height(void) {
  Pixmap p = halo_create_pixmap(dpy, XSHIM_WINDOW, 120, 60, 1);
  GC gc = gc_with(1, GXcopy, 0);
  moire2_draw_arc(dpy, p, gc, 5, 5, 101, 41, 0, 360 * 64);
  TEST_ASSERT_EQUAL_INT(1, halo_get_bit(plane(p), 5, 25));
  TEST_ASSERT_EQUAL_INT(1, halo_get_bit(plane(p), 105, 25));
  TEST_ASSERT_EQUAL_INT(1, halo_get_bit(plane(p), 55, 5));
  TEST_ASSERT_EQUAL_INT(1, halo_get_bit(plane(p), 55, 45));
  TEST_ASSERT_EQUAL_INT(0, halo_get_bit(plane(p), 55, 25));
  TEST_ASSERT_EQUAL_INT(0, halo_get_bit(plane(p), 55, 55));
  XFreeGC(dpy, gc);
  halo_free_pixmap(dpy, p);
}

void test_circles_hanging_off_the_plane_or_huge_stay_inside_it(void) {
  Pixmap p = halo_create_pixmap(dpy, XSHIM_WINDOW, 30, 20, 1);
  GC gc = gc_with(1, GXcopy, 3);
  moire2_draw_arc(dpy, p, gc, -40, -30, 90, 70, 0, 360 * 64);
  moire2_draw_arc(dpy, p, gc, -5000, -5000, 10000, 10000, 0, 360 * 64);
  moire2_draw_arc(dpy, p, gc, 25, 15, 40, 40, 0, 360 * 64);
  moire2_draw_arc(dpy, p, gc, 0, 0, 0, 0, 0, 360 * 64);
  XFreeGC(dpy, gc);
  halo_free_pixmap(dpy, p);
}

void test_a_partial_arc_draws_nothing_in_a_plane(void) {
  Pixmap p = halo_create_pixmap(dpy, XSHIM_WINDOW, 30, 30, 1);
  GC gc = gc_with(1, GXcopy, 0);
  moire2_draw_arc(dpy, p, gc, 2, 2, 20, 20, 0, 90 * 64);
  TEST_ASSERT_EQUAL_INT(0, count_set(plane(p)));
  XFreeGC(dpy, gc);
  halo_free_pixmap(dpy, p);
}

/* Reference for a plane-to-plane copy, one bit at a time. */
static void reference_copy(const HaloPlane *s, HaloPlane *d, int function,
                           int sx, int sy, int w, int h, int dx, int dy) {
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++) {
      const int x = sx + i, y = sy + j, ox = dx + i, oy = dy + j;
      if (x < 0 || y < 0 || x >= s->w || y >= s->h) continue;
      if (ox < 0 || oy < 0 || ox >= d->w || oy >= d->h) continue;
      const int bit = halo_get_bit(s, x, y);
      const uint8_t m = (uint8_t)(0x80 >> (ox & 7));
      uint8_t *b = &d->bits[(size_t)oy * (size_t)d->stride + (size_t)(ox >> 3)];
      if (function == GXor) {
        if (bit) *b |= m;
      } else if (function == GXxor) {
        if (bit) *b ^= m;
      } else {
        *b = bit ? (uint8_t)(*b | m) : (uint8_t)(*b & ~m);
      }
    }
}

static void check_copy(int function, int sx, int sy, int w, int h, int dx,
                       int dy) {
  Pixmap sp = halo_create_pixmap(dpy, XSHIM_WINDOW, 37, 21, 1);
  Pixmap dp = halo_create_pixmap(dpy, XSHIM_WINDOW, 29, 17, 1);
  Pixmap rp = halo_create_pixmap(dpy, XSHIM_WINDOW, 29, 17, 1);
  scatter(plane(sp), 7);
  scatter(plane(dp), 11);
  memcpy(plane(rp)->bits, plane(dp)->bits,
         (size_t)plane(dp)->stride * (size_t)plane(dp)->h);
  GC gc = gc_with(1, function, 0);
  moire2_copy_area(dpy, sp, dp, gc, sx, sy, (unsigned)w, (unsigned)h, dx, dy);
  reference_copy(plane(sp), plane(rp), function, sx, sy, w, h, dx, dy);
  char msg[96];
  snprintf(msg, sizeof msg, "fn %d src %d,%d %dx%d dst %d,%d", function, sx,
           sy, w, h, dx, dy);
  for (int y = 0; y < plane(dp)->h; y++)
    for (int x = 0; x < plane(dp)->w; x++)
      TEST_ASSERT_EQUAL_INT_MESSAGE(halo_get_bit(plane(rp), x, y),
                                    halo_get_bit(plane(dp), x, y), msg);
  XFreeGC(dpy, gc);
  halo_free_pixmap(dpy, sp);
  halo_free_pixmap(dpy, dp);
  halo_free_pixmap(dpy, rp);
}

static void check_expand(int sx, int sy, int w, int h, int dx, int dy) {
  Pixmap sp = halo_create_pixmap(dpy, XSHIM_WINDOW, 53, 31, 1);
  scatter(plane(sp), 5);
  XGCValues v = {0};
  GC gc = XCreateGC(dpy, XSHIM_WINDOW, 0, &v);
  XSetForeground(dpy, gc, 0xF800);
  XSetBackground(dpy, gc, 0x001F);
  const size_t n = (size_t)cv.w * (size_t)cv.h;
  uint16_t *want = (uint16_t *)malloc(n * sizeof(uint16_t));
  for (size_t i = 0; i < n; i++) cv.px[i] = 0x1234;
  halo_copy_plane(dpy, sp, XSHIM_WINDOW, gc, sx, sy, (unsigned)w, (unsigned)h,
                  dx, dy, 1);
  memcpy(want, cv.px, n * sizeof(uint16_t));
  for (size_t i = 0; i < n; i++) cv.px[i] = 0x1234;
  canvas_clear_dirty(&cv);
  moire2_copy_plane(dpy, sp, XSHIM_WINDOW, gc, sx, sy, (unsigned)w, (unsigned)h,
                    dx, dy, 1);
  char msg[64];
  snprintf(msg, sizeof msg, "src %d,%d %dx%d dst %d,%d", sx, sy, w, h, dx, dy);
  for (size_t i = 0; i < n; i++)
    TEST_ASSERT_EQUAL_HEX16_MESSAGE(want[i], cv.px[i], msg);
  free(want);
  XFreeGC(dpy, gc);
  halo_free_pixmap(dpy, sp);
}

void test_expanding_a_plane_to_the_window_matches_the_per_pixel_version(void) {
  check_expand(0, 0, 40, 30, 0, 0);
  check_expand(3, 2, 30, 20, 0, 0);
  check_expand(8, 1, 32, 20, 0, 0);
  check_expand(5, 5, 20, 12, 1, 3);
  check_expand(0, 0, 25, 25, 7, 2);
  check_expand(11, 0, 9, 4, 13, 9);
  check_expand(0, 0, 53, 31, 0, 0);
  check_expand(-5, -3, 30, 20, 4, 4);
  check_expand(30, 15, 40, 40, 0, 0);
  check_expand(0, 0, 53, 31, -6, -4);
  check_expand(0, 0, 53, 31, 30, 20);
}

void test_expanding_marks_exactly_the_rectangle_it_wrote_dirty(void) {
  Pixmap sp = halo_create_pixmap(dpy, XSHIM_WINDOW, 53, 31, 1);
  XGCValues v = {0};
  GC gc = XCreateGC(dpy, XSHIM_WINDOW, 0, &v);
  canvas_clear_dirty(&cv);
  moire2_copy_plane(dpy, sp, XSHIM_WINDOW, gc, 0, 0, 10, 3, 5, 7, 1);
  int x0, x1;
  TEST_ASSERT_FALSE(canvas_dirty_row(&cv, 6, &x0, &x1));
  TEST_ASSERT_TRUE(canvas_dirty_row(&cv, 7, &x0, &x1));
  TEST_ASSERT_EQUAL_INT(5, x0);
  TEST_ASSERT_TRUE(canvas_dirty_row(&cv, 9, &x0, &x1));
  TEST_ASSERT_FALSE(canvas_dirty_row(&cv, 10, &x0, &x1));
  canvas_clear_dirty(&cv);
  moire2_copy_plane(dpy, sp, XSHIM_WINDOW, gc, 0, 0, 10, 1, -3, 20, 1);
  TEST_ASSERT_TRUE(canvas_dirty_row(&cv, 20, &x0, &x1));
  TEST_ASSERT_EQUAL_INT(0, x0);
  TEST_ASSERT_EQUAL_INT(6, x1);
  XFreeGC(dpy, gc);
  halo_free_pixmap(dpy, sp);
}

void test_expanding_to_a_plane_still_goes_through_halos_xor(void) {
  Pixmap sp = halo_create_pixmap(dpy, XSHIM_WINDOW, 16, 4, 1);
  Pixmap dp = halo_create_pixmap(dpy, XSHIM_WINDOW, 16, 4, 1);
  GC on = gc_with(1, GXcopy, 0);
  halo_fill_rectangle(dpy, sp, on, 0, 0, 16, 4);
  moire2_copy_plane(dpy, sp, dp, on, 0, 0, 16, 4, 0, 0, 1);
  TEST_ASSERT_EQUAL_INT(64, count_set(plane(dp)));
  moire2_copy_plane(dpy, sp, dp, on, 0, 0, 16, 4, 0, 0, 1);
  TEST_ASSERT_EQUAL_INT(0, count_set(plane(dp)));
  XFreeGC(dpy, on);
  halo_free_pixmap(dpy, sp);
  halo_free_pixmap(dpy, dp);
}

void test_copy_area_ors_xors_and_copies_aligned_and_unaligned(void) {
  const int fns[] = {GXor, GXxor, GXcopy};
  for (int f = 0; f < 3; f++) {
    check_copy(fns[f], 0, 0, 29, 17, 0, 0);
    check_copy(fns[f], 8, 2, 16, 10, 0, 0);
    check_copy(fns[f], 5, 3, 20, 12, 0, 0);
    check_copy(fns[f], 3, 1, 11, 9, 6, 2);
    check_copy(fns[f], 7, 0, 1, 1, 7, 0);
    check_copy(fns[f], 1, 4, 3, 3, 14, 5);
  }
}

void test_copy_area_hanging_off_every_edge_stays_inside_both_planes(void) {
  const int fns[] = {GXor, GXxor, GXcopy};
  for (int f = 0; f < 3; f++) {
    check_copy(fns[f], -5, -3, 20, 12, 2, 2);
    check_copy(fns[f], 30, 15, 20, 12, 0, 0);
    check_copy(fns[f], 2, 2, 50, 40, -4, -3);
    check_copy(fns[f], 2, 2, 50, 40, 20, 10);
    check_copy(fns[f], 0, 0, 40, 30, 25, 14);
  }
}

void test_a_copy_past_the_right_edge_does_not_wrap_onto_the_next_row(void) {
  Pixmap sp = halo_create_pixmap(dpy, XSHIM_WINDOW, 40, 4, 1);
  Pixmap dp = halo_create_pixmap(dpy, XSHIM_WINDOW, 20, 4, 1);
  GC gc = gc_with(1, GXor, 0);
  halo_fill_rectangle(dpy, sp, gc, 0, 0, 40, 4);
  moire2_copy_area(dpy, sp, dp, gc, 0, 0, 40, 4, 12, 0);
  TEST_ASSERT_EQUAL_INT(8 * 4, count_set(plane(dp)));
  for (int y = 0; y < 4; y++) TEST_ASSERT_EQUAL_INT(0, halo_get_bit(plane(dp), 11, y));
  XFreeGC(dpy, gc);
  halo_free_pixmap(dpy, sp);
  halo_free_pixmap(dpy, dp);
}

void test_copy_area_with_a_missing_plane_does_nothing(void) {
  Pixmap dp = halo_create_pixmap(dpy, XSHIM_WINDOW, 20, 4, 1);
  GC gc = gc_with(1, GXor, 0);
  moire2_copy_area(dpy, None, dp, gc, 0, 0, 20, 4, 0, 0);
  moire2_copy_area(dpy, dp, None, gc, 0, 0, 20, 4, 0, 0);
  TEST_ASSERT_EQUAL_INT(0, count_set(plane(dp)));
  XFreeGC(dpy, gc);
  halo_free_pixmap(dpy, dp);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_xor_fill_flips_the_rectangle_and_a_zero_foreground_changes_nothing);
  RUN_TEST(test_an_xor_fill_ending_inside_a_byte_leaves_the_bits_after_it);
  RUN_TEST(test_a_copy_fill_sets_and_clears_like_halos);
  RUN_TEST(test_a_fill_on_the_window_still_reaches_the_shim);
  RUN_TEST(test_a_circle_outline_is_one_ring_as_thick_as_the_line_width);
  RUN_TEST(test_a_circle_outline_is_symmetric_and_closed);
  RUN_TEST(test_an_ellipse_outline_follows_its_own_width_and_height);
  RUN_TEST(test_circles_hanging_off_the_plane_or_huge_stay_inside_it);
  RUN_TEST(test_a_partial_arc_draws_nothing_in_a_plane);
  RUN_TEST(test_expanding_a_plane_to_the_window_matches_the_per_pixel_version);
  RUN_TEST(test_expanding_marks_exactly_the_rectangle_it_wrote_dirty);
  RUN_TEST(test_expanding_to_a_plane_still_goes_through_halos_xor);
  RUN_TEST(test_copy_area_ors_xors_and_copies_aligned_and_unaligned);
  RUN_TEST(test_copy_area_hanging_off_every_edge_stays_inside_both_planes);
  RUN_TEST(test_a_copy_past_the_right_edge_does_not_wrap_onto_the_next_row);
  RUN_TEST(test_copy_area_with_a_missing_plane_does_nothing);
  return UNITY_END();
}
