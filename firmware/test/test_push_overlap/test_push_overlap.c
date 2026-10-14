#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <unity.h>

#include "core/canvas.h"
#include "hacks/registry.h"
#include "runner/push_overlap.h"

#define W 64
#define H 48

/* Host stand-ins for the device's FreeRTOS primitives. */
typedef struct {
  pthread_mutex_t m;
  pthread_cond_t c;
  int n;
} Sem;
static void *h_sem_create(void) {
  Sem *s = calloc(1, sizeof *s);
  pthread_mutex_init(&s->m, NULL);
  pthread_cond_init(&s->c, NULL);
  return s;
}
static void h_sem_give(void *p) {
  Sem *s = p;
  pthread_mutex_lock(&s->m);
  s->n++;
  pthread_cond_signal(&s->c);
  pthread_mutex_unlock(&s->m);
}
static void h_sem_take(void *p) {
  Sem *s = p;
  pthread_mutex_lock(&s->m);
  while (!s->n) pthread_cond_wait(&s->c, &s->m);
  s->n--;
  pthread_mutex_unlock(&s->m);
}
static void h_sem_destroy(void *p) { free(p); }
typedef struct {
  void (*body)(void *);
  void *arg;
} Boot;
static void *boot(void *p) {
  Boot b = *(Boot *)p;
  free(p);
  b.body(b.arg);
  return NULL;
}
static int spawn_fails;
static int h_spawn(void (*body)(void *), void *arg) {
  if (spawn_fails) return -1;
  Boot *b = malloc(sizeof *b);
  b->body = body;
  b->arg = arg;
  pthread_t t;
  if (pthread_create(&t, NULL, boot, b)) return -1;
  pthread_detach(t);
  return 0;
}
static uint32_t clock_us;
static uint32_t h_now_us(void) { return clock_us += 12500; }
static const PushOverlapOps kOps = {h_sem_create, h_sem_give, h_sem_take,
                                    h_spawn, h_now_us, h_sem_destroy};

typedef struct {
  atomic_int in_push, max_in_push;
  int count;
  uint32_t hash;
  const uint16_t *pushed_px;
  int dirty_rows;
  int16_t x0[H], x1[H];
} Fake;
static Fake fake;

static uint32_t fnv(const uint16_t *px) {
  uint32_t h = 2166136261u;
  for (int i = 0; i < W * H; i++) h = (h ^ px[i]) * 16777619u;
  return h;
}
static int fake_push(void *ctx, Canvas *view) {
  Fake *f = ctx;
  int now = atomic_fetch_add(&f->in_push, 1) + 1;
  if (now > atomic_load(&f->max_in_push)) atomic_store(&f->max_in_push, now);
  f->pushed_px = view->px;
  uint32_t h = 2166136261u;
  f->dirty_rows = 0;
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) h = (h ^ view->px[y * W + x]) * 16777619u;
    int x0, x1;
    if (canvas_dirty_row(view, y, &x0, &x1)) {
      f->x0[f->dirty_rows] = (int16_t)x0;
      f->x1[f->dirty_rows] = (int16_t)x1;
      f->dirty_rows++;
    }
    if (y % 8 == 0) usleep(300);
  }
  f->hash = h;
  f->count++;
  atomic_fetch_sub(&f->in_push, 1);
  return f->dirty_rows;
}

static Canvas cv;
static uint16_t *spare, *first_px;
static PushOverlap *po;

static void paint(uint16_t seed) {
  for (int i = 0; i < W * H; i++) cv.px[i] = (uint16_t)(seed + i * 7);
  canvas_mark_dirty(&cv, 0, 0, W, H);
}
static void scribble(void) {
  for (int i = 0; i < W * H; i++) cv.px[i] = 0xDEAD;
}

void setUp(void) {
  memset(&fake, 0, sizeof fake);
  clock_us = 0;
  spawn_fails = 0;
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&cv, W, H, malloc));
  canvas_clear_dirty(&cv);
  spare = calloc(W * H, sizeof(uint16_t));
  first_px = cv.px;
  po = push_overlap_create(&cv, spare, &kOps, fake_push, &fake);
  TEST_ASSERT_NOT_NULL(po);
}
void tearDown(void) {
  push_overlap_destroy(po);
  cv.px = first_px;
  free(spare);
  canvas_free(&cv);
}

void test_no_hack_asks_for_both_repaints_all_and_sync_to_refresh(void) {
  for (int i = 0; i < g_hack_count; i++) {
    const HackEntry *e = g_hacks[i];
    TEST_ASSERT_FALSE_MESSAGE(e->repaints_all && e->sync_to_refresh, e->name);
  }
}

void test_a_finished_frame_is_pushed_intact_while_the_next_step_scribbles(void) {
  paint(100);
  uint32_t want = fnv(cv.px);
  TEST_ASSERT_TRUE(push_overlap_present(po, &cv, true));
  scribble();
  push_overlap_wait(po);
  TEST_ASSERT_EQUAL_HEX32(want, fake.hash);
}

void test_present_swaps_the_two_buffers(void) {
  uint16_t *shown = cv.px, *other = spare;
  paint(1);
  TEST_ASSERT_TRUE(push_overlap_present(po, &cv, true));
  TEST_ASSERT_TRUE(cv.px == other);
  push_overlap_wait(po);
  TEST_ASSERT_TRUE(fake.pushed_px == shown);
}

void test_pushes_never_overlap_each_other(void) {
  for (int i = 0; i < 10; i++) {
    paint((uint16_t)i);
    push_overlap_present(po, &cv, true);
  }
  push_overlap_wait(po);
  TEST_ASSERT_EQUAL_INT(10, fake.count);
  TEST_ASSERT_EQUAL_INT(1, atomic_load(&fake.max_in_push));
}

void test_an_ineligible_present_brings_back_the_newest_frame(void) {
  uint16_t *shown = cv.px;
  paint(5);
  push_overlap_present(po, &cv, true);
  canvas_mark_dirty(&cv, 0, 0, W, H);
  TEST_ASSERT_FALSE(push_overlap_present(po, &cv, false));
  TEST_ASSERT_TRUE(cv.px == shown);
  int x0, x1;
  TEST_ASSERT_FALSE(canvas_dirty_row(&cv, 0, &x0, &x1));
}

void test_an_eligible_present_with_nothing_dirty_pushes_nothing_and_shows_the_newest_frame(void) {
  paint(5);
  push_overlap_present(po, &cv, true);
  uint16_t *pushed = first_px;
  TEST_ASSERT_FALSE(push_overlap_present(po, &cv, true));
  TEST_ASSERT_TRUE(cv.px == pushed);
  push_overlap_wait(po);
  TEST_ASSERT_EQUAL_INT(1, fake.count);
}

void test_reset_stops_the_next_ineligible_present_swapping_back(void) {
  paint(5);
  push_overlap_present(po, &cv, true);
  uint16_t *live = cv.px;
  push_overlap_reset(po);
  TEST_ASSERT_FALSE(push_overlap_present(po, &cv, false));
  TEST_ASSERT_TRUE(cv.px == live);
}

void test_credit_is_the_previous_pushes_duration_and_zero_after_a_fallback(void) {
  paint(1);
  push_overlap_present(po, &cv, true);
  TEST_ASSERT_EQUAL_UINT32(0, push_overlap_credit_us(po));
  paint(2);
  push_overlap_present(po, &cv, true);
  TEST_ASSERT_EQUAL_UINT32(12500, push_overlap_credit_us(po));
  canvas_mark_dirty(&cv, 0, 0, 1, 1);
  push_overlap_present(po, &cv, false);
  TEST_ASSERT_EQUAL_UINT32(0, push_overlap_credit_us(po));
}

void test_the_view_gets_the_frames_dirty_spans_and_the_canvas_is_left_clean(void) {
  canvas_mark_dirty(&cv, 10, 3, 20, 2);
  push_overlap_present(po, &cv, true);
  push_overlap_wait(po);
  TEST_ASSERT_EQUAL_INT(2, fake.dirty_rows);
  TEST_ASSERT_EQUAL_INT(10, fake.x0[0]);
  TEST_ASSERT_EQUAL_INT(29, fake.x1[0]);
  int x0, x1;
  TEST_ASSERT_FALSE(canvas_dirty_row(&cv, 3, &x0, &x1));
}

void test_take_rows_totals_completed_pushes(void) {
  canvas_mark_dirty(&cv, 0, 0, W, 4);
  push_overlap_present(po, &cv, true);
  canvas_mark_dirty(&cv, 0, 0, W, 6);
  push_overlap_present(po, &cv, true);
  push_overlap_wait(po);
  TEST_ASSERT_EQUAL_INT(10, push_overlap_take_rows(po));
  TEST_ASSERT_EQUAL_INT(0, push_overlap_take_rows(po));
}

void test_create_fails_cleanly_when_spawn_fails(void) {
  spawn_fails = 1;
  TEST_ASSERT_NULL(push_overlap_create(&cv, spare, &kOps, fake_push, &fake));
  TEST_ASSERT_NULL(push_overlap_create(&cv, NULL, &kOps, fake_push, &fake));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_no_hack_asks_for_both_repaints_all_and_sync_to_refresh);
  RUN_TEST(test_a_finished_frame_is_pushed_intact_while_the_next_step_scribbles);
  RUN_TEST(test_present_swaps_the_two_buffers);
  RUN_TEST(test_pushes_never_overlap_each_other);
  RUN_TEST(test_an_ineligible_present_brings_back_the_newest_frame);
  RUN_TEST(test_an_eligible_present_with_nothing_dirty_pushes_nothing_and_shows_the_newest_frame);
  RUN_TEST(test_reset_stops_the_next_ineligible_present_swapping_back);
  RUN_TEST(test_credit_is_the_previous_pushes_duration_and_zero_after_a_fallback);
  RUN_TEST(test_the_view_gets_the_frames_dirty_spans_and_the_canvas_is_left_clean);
  RUN_TEST(test_take_rows_totals_completed_pushes);
  RUN_TEST(test_create_fails_cleanly_when_spawn_fails);
  return UNITY_END();
}
