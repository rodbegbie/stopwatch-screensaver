#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include "core/canvas.h"
#include "core/push_plan.h"
#include "hacks/registry.h"
#include "runner/hack_runner.h"

#define W 466
#define H 466
#define UNSET 0xBEEF

static Canvas cv;
static uint16_t *shadow; /* what the display holds */
static PushRow *rows;
static int calls;

void setUp(void) {
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&cv, W, H, malloc));
  shadow = malloc((size_t)W * H * sizeof(uint16_t));
  rows = malloc(H * sizeof(PushRow));
  for (int i = 0; i < W * H; i++) shadow[i] = UNSET;
  calls = 0;
}
void tearDown(void) {
  free(rows);
  free(shadow);
  canvas_free(&cv);
}

static void on_begin(void *ctx) { (void)ctx; calls++; }
static void on_end(void *ctx) { (void)ctx; calls++; }
static void on_row(void *ctx, int y, int x0, int x1) {
  (void)ctx;
  calls++;
  memcpy(shadow + (size_t)y * W + x0, cv.px + (size_t)y * W + x0,
         (size_t)(x1 - x0 + 1) * sizeof(uint16_t));
}
static void on_all(void *ctx) {
  (void)ctx;
  calls++;
  memcpy(shadow, cv.px, (size_t)W * H * sizeof(uint16_t));
}
static const PushSink sink = {NULL, on_begin, on_row, on_all, on_end};

static int present(void) { return push_present(&cv, rows, &sink); }
static bool shadow_matches_canvas(void) {
  return memcmp(shadow, cv.px, (size_t)W * H * sizeof(uint16_t)) == 0;
}

#define OVERLAY_X 133
#define OVERLAY_Y 218
#define OVERLAY_W 200
#define OVERLAY_H 30
static uint16_t saved[OVERLAY_W * OVERLAY_H];

/* Mirrors present() in main.cpp: text is written straight into the canvas,
 * marked dirty by hand, pushed, then the hack's pixels are pasted back. */
static void stamp(void) {
  canvas_copy_rect(&cv, OVERLAY_X, OVERLAY_Y, OVERLAY_W, OVERLAY_H, saved);
  for (int y = OVERLAY_Y; y < OVERLAY_Y + OVERLAY_H; y++)
    for (int x = OVERLAY_X; x < OVERLAY_X + OVERLAY_W; x++) cv.px[y * W + x] = 0xFFFF;
  canvas_mark_dirty(&cv, OVERLAY_X, OVERLAY_Y, OVERLAY_W, OVERLAY_H);
}
static void unstamp(void) {
  canvas_paste_rect(&cv, OVERLAY_X, OVERLAY_Y, OVERLAY_W, OVERLAY_H, saved);
}

void test_a_fresh_canvas_reaches_the_display_in_full(void) {
  present();
  TEST_ASSERT_TRUE(shadow_matches_canvas());
}

void test_a_present_after_nothing_was_drawn_touches_the_display_not_at_all(void) {
  present();
  int before = calls;
  TEST_ASSERT_EQUAL_INT(0, present());
  TEST_ASSERT_EQUAL_INT(before, calls);
}

void test_every_registered_hack_reaches_the_display_intact(void) {
  HackRunner *r = runner_create(&cv);
  TEST_ASSERT_NOT_NULL(r);
  for (int i = 0; i < g_hack_count; i++) {
    TEST_ASSERT_EQUAL_INT(0, runner_start(r, i));
    for (int f = 0; f < 3000; f++) {
      runner_step(r);
      present();
      if (f % 10 == 9) TEST_ASSERT_TRUE_MESSAGE(shadow_matches_canvas(), g_hacks[i]->name);
    }
    TEST_ASSERT_TRUE_MESSAGE(shadow_matches_canvas(), g_hacks[i]->name);
  }
  runner_destroy(r);
}

void test_switching_hacks_shows_the_new_one_whole_from_its_first_frame(void) {
  HackRunner *r = runner_create(&cv);
  TEST_ASSERT_EQUAL_INT(0, runner_start(r, 0));
  for (int k = 0; k < 8; k++) {
    for (int f = 0; f < 40; f++) {
      runner_step(r);
      present();
    }
    runner_next(r);
    runner_step(r);
    present();
    TEST_ASSERT_TRUE_MESSAGE(shadow_matches_canvas(), g_hacks[runner_index(r)]->name);
  }
  runner_destroy(r);
}

void test_overlay_text_is_erased_from_the_display_on_the_next_frame(void) {
  HackRunner *r = runner_create(&cv);
  TEST_ASSERT_EQUAL_INT(0, runner_start(r, 0));
  for (int f = 0; f < 30; f++) {
    runner_step(r);
    present();
  }
  stamp();
  present();
  TEST_ASSERT_EQUAL_HEX16(0xFFFF, shadow[(OVERLAY_Y + 5) * W + OVERLAY_X + 5]);
  unstamp();
  runner_step(r);
  present();
  TEST_ASSERT_TRUE(shadow_matches_canvas());
  runner_destroy(r);
}

void test_overlay_text_is_erased_when_the_hack_changes_in_the_next_frame(void) {
  HackRunner *r = runner_create(&cv);
  TEST_ASSERT_EQUAL_INT(0, runner_start(r, 0));
  for (int f = 0; f < 30; f++) {
    runner_step(r);
    present();
  }
  stamp();
  present();
  unstamp();
  runner_next(r);
  runner_step(r);
  present();
  TEST_ASSERT_TRUE(shadow_matches_canvas());
  runner_destroy(r);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_a_fresh_canvas_reaches_the_display_in_full);
  RUN_TEST(test_a_present_after_nothing_was_drawn_touches_the_display_not_at_all);
  RUN_TEST(test_every_registered_hack_reaches_the_display_intact);
  RUN_TEST(test_switching_hacks_shows_the_new_one_whole_from_its_first_frame);
  RUN_TEST(test_overlay_text_is_erased_from_the_display_on_the_next_frame);
  RUN_TEST(test_overlay_text_is_erased_when_the_hack_changes_in_the_next_frame);
  return UNITY_END();
}
