#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include "hacks/ad_flyingtoasters/ad_sim.h"

#define W 466
#define H 466

static long script[64];
static int script_len;
static int script_pos;

static long scripted(void) {
  long v = script_pos < script_len ? script[script_pos] : 0;
  script_pos++;
  return v;
}

static void set_script(const long *v, int n) {
  memcpy(script, v, (size_t)n * sizeof(long));
  script_len = n;
  script_pos = 0;
}

typedef struct {
  int count;
  int sprites[64];
  AdRect rects[64];
} Rec;

static void record(void *ctx, const AdRect *r, int sprite) {
  Rec *rec = ctx;
  if (rec->count < 64) {
    rec->sprites[rec->count] = sprite;
    rec->rects[rec->count] = *r;
  }
  rec->count++;
}

static AdRect rect_at(int top, int left) {
  AdRect r = {top, left, top + AD_RECT, left + AD_RECT};
  return r;
}

static const AdRect screen = {0, 0, H, W};

/* An empty simulation with n slots, filled by hand with place(). */
static void empty_sim(AdSim *sim, int n) {
  ad_sim_init(sim, screen, 0, scripted);
  sim->n = n;
}

static void place(AdSim *sim, int i, int top, int left, int speed, int toast) {
  AdFlyer *f = &sim->f[i];
  memset(f, 0, sizeof *f);
  f->live = 1;
  f->toast = toast;
  f->speed = speed;
  f->r = rect_at(top, left);
}

static void assert_pos(const AdSim *sim, int i, int top, int left) {
  TEST_ASSERT_EQUAL_INT(top, sim->f[i].r.top);
  TEST_ASSERT_EQUAL_INT(left, sim->f[i].r.left);
  TEST_ASSERT_EQUAL_INT(top + AD_RECT, sim->f[i].r.bottom);
  TEST_ASSERT_EQUAL_INT(left + AD_RECT, sim->f[i].r.right);
}

void setUp(void) {
  script_len = 0;
  script_pos = 0;
}
void tearDown(void) {}

static void init_one(AdSim *sim, long roll) {
  const long s[] = {100, 100, roll};
  set_script(s, 3);
  ad_sim_init(sim, screen, 1, scripted);
}

void test_flap_runs_0_1_2_3_2_1_0(void) {
  AdSim sim;
  Rec rec = {0};
  init_one(&sim, 0);
  for (int i = 0; i < 7; i++) ad_sim_draw_all(&sim, record, &rec);
  const int want[] = {0, 1, 2, 3, 2, 1, 0};
  TEST_ASSERT_EQUAL_INT(7, rec.count);
  TEST_ASSERT_EQUAL_INT_ARRAY(want, rec.sprites, 7);
}

void test_speed_from_roll(void) {
  const long rolls[] = {24, 25, 49, 50, 74, 75, 99};
  const int speed[] = {1, 2, 2, 3, 3, 1, 1};
  const int toast[] = {0, 0, 0, 0, 0, 1, 1};
  for (int i = 0; i < 7; i++) {
    AdSim sim;
    init_one(&sim, rolls[i]);
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, sim.f[0].live, "live");
    TEST_ASSERT_EQUAL_INT_MESSAGE(speed[i], sim.f[0].speed, "speed");
    TEST_ASSERT_EQUAL_INT_MESSAGE(toast[i], sim.f[0].toast, "toast");
  }
}

void test_toast_draws_sprite_4(void) {
  AdSim sim;
  Rec rec = {0};
  init_one(&sim, 80);
  ad_sim_draw_all(&sim, record, &rec);
  ad_sim_draw_all(&sim, record, &rec);
  TEST_ASSERT_EQUAL_INT(4, rec.sprites[0]);
  TEST_ASSERT_EQUAL_INT(4, rec.sprites[1]);
}

void test_init_scatter_does_not_draw_and_leaves_running_set(void) {
  AdSim sim;
  init_one(&sim, 0);
  TEST_ASSERT_EQUAL_INT(1, sim.running);
  TEST_ASSERT_EQUAL_INT(1, sim.n);
  assert_pos(&sim, 0, 100, 100);
}

void test_step_moves_speed_times_minus2_plus1(void) {
  const long rolls[] = {0, 30, 60};
  for (int s = 1; s <= 3; s++) {
    AdSim sim;
    Rec rec = {0};
    init_one(&sim, rolls[s - 1]);
    TEST_ASSERT_EQUAL_INT(s, sim.f[0].speed);
    ad_sim_step(&sim, record, &rec);
    assert_pos(&sim, 0, 100 + s, 100 - 2 * s);
    TEST_ASSERT_EQUAL_INT(1, rec.count);
    TEST_ASSERT_EQUAL_INT(100 + s, rec.rects[0].top);
  }
}

void test_nudge_beside_drops_two_more(void) {
  AdSim sim;
  Rec rec = {0};
  empty_sim(&sim, 2);
  place(&sim, 0, 100, 200, 2, 0);
  place(&sim, 1, 101, 129, 1, 1);
  ad_sim_step(&sim, record, &rec);
  assert_pos(&sim, 0, 103, 198);
}

void test_nudge_above_goes_left_below_goes_less_left(void) {
  AdSim sim;
  Rec rec = {0};
  empty_sim(&sim, 2);
  place(&sim, 0, 100, 200, 2, 0);
  place(&sim, 1, 170, 198, 1, 1);
  ad_sim_step(&sim, record, &rec);
  assert_pos(&sim, 0, 101, 197);

  empty_sim(&sim, 2);
  place(&sim, 0, 100, 200, 2, 0);
  place(&sim, 1, 32, 129, 1, 1);
  ad_sim_step(&sim, record, &rec);
  assert_pos(&sim, 0, 101, 199);
}

void test_nudged_move_that_still_collides_keeps_base_step(void) {
  AdSim sim;
  Rec rec = {0};
  empty_sim(&sim, 2);
  place(&sim, 0, 100, 200, 2, 0);
  place(&sim, 1, 101, 132, 1, 1);
  ad_sim_step(&sim, record, &rec);
  assert_pos(&sim, 0, 101, 198);
}

void test_speed_one_never_checks(void) {
  AdSim sim;
  Rec rec = {0};
  empty_sim(&sim, 2);
  place(&sim, 0, 100, 200, 1, 0);
  place(&sim, 1, 101, 135, 1, 1);
  ad_sim_step(&sim, record, &rec);
  assert_pos(&sim, 0, 101, 198);
  TEST_ASSERT_TRUE(sim.f[0].r.left < sim.f[1].r.right);
}

void test_collision_reports_nudge_and_skips_self_and_dead_slots(void) {
  AdSim sim;
  int dv = 9, dh = 9;
  empty_sim(&sim, 3);
  place(&sim, 0, 100, 200, 2, 0);
  place(&sim, 2, 300, 140, 1, 1);
  sim.f[1].live = 0;
  sim.f[1].r = rect_at(100, 200);
  TEST_ASSERT_EQUAL_INT(0, ad_sim_collision(&sim, 0, sim.f[0].r, &dv, &dh));
  TEST_ASSERT_EQUAL_INT(1, ad_sim_collision(&sim, 1, rect_at(100, 260), &dv, &dh));
  TEST_ASSERT_EQUAL_INT(-1, dv);
  TEST_ASSERT_EQUAL_INT(0, dh);
}

void test_off_screen_flyer_is_removed_and_respawned_at_an_edge(void) {
  AdSim sim;
  Rec rec = {0};

  const long top_edge[] = {1, 50, 0};
  empty_sim(&sim, 1);
  place(&sim, 0, 100, -68, 1, 0);
  ad_sim_step(&sim, record, &rec);
  TEST_ASSERT_EQUAL_INT(0, sim.f[0].live);
  TEST_ASSERT_EQUAL_INT(0, rec.count);
  set_script(top_edge, 3);
  ad_sim_step(&sim, record, &rec);
  TEST_ASSERT_EQUAL_INT(1, sim.f[0].live);
  TEST_ASSERT_EQUAL_INT(1, sim.f[0].r.bottom);
  assert_pos(&sim, 0, -69, 48);
  TEST_ASSERT_EQUAL_INT(1, rec.count);

  const long right_edge[] = {0, 300, 0};
  empty_sim(&sim, 1);
  place(&sim, 0, 100, -68, 1, 0);
  ad_sim_step(&sim, record, &rec);
  set_script(right_edge, 3);
  ad_sim_step(&sim, record, &rec);
  TEST_ASSERT_EQUAL_INT(1, sim.f[0].live);
  assert_pos(&sim, 0, 301, W - 2);
}

void test_initial_scatter_is_inside_bounds(void) {
  for (unsigned seed = 1; seed <= 200; seed++) {
    AdSim sim;
    srandom(seed);
    ad_sim_init(&sim, screen, 10, random);
    for (int i = 0; i < 10; i++) {
      if (!sim.f[i].live) continue;
      TEST_ASSERT_TRUE(sim.f[i].r.top >= 0);
      TEST_ASSERT_TRUE(sim.f[i].r.left >= 0);
      TEST_ASSERT_TRUE(sim.f[i].r.bottom <= H);
      TEST_ASSERT_TRUE(sim.f[i].r.right <= W);
    }
  }
}

void test_n_is_clamped_to_the_slot_array(void) {
  AdSim sim;
  srandom(3);
  ad_sim_init(&sim, screen, 500, random);
  TEST_ASSERT_EQUAL_INT(AD_MAX_FLYERS, sim.n);
}

void test_slot_stays_empty_after_four_colliding_tries(void) {
  AdSim sim;
  Rec rec = {0};
  const long all_collide[] = {1, 100, 1, 100, 1, 100, 1, 100};
  empty_sim(&sim, 2);
  place(&sim, 0, -50, 100, 1, 1);
  sim.f[1].live = 0;
  set_script(all_collide, 8);
  ad_sim_step(&sim, record, &rec);
  TEST_ASSERT_EQUAL_INT(0, sim.f[1].live);
  TEST_ASSERT_EQUAL_INT(8, script_pos);

  const long last_try_clear[] = {1, 100, 1, 100, 1, 100, 1, 400, 0};
  set_script(last_try_clear, 9);
  ad_sim_step(&sim, record, &rec);
  TEST_ASSERT_EQUAL_INT(9, script_pos);
  TEST_ASSERT_EQUAL_INT(1, sim.f[1].live);
}

static uint16_t canvas[H][W];

static void blit_stub(void *ctx, const AdRect *r, int sprite) {
  (void)ctx;
  (void)sprite;
  for (int dy = 0; dy < AD_RECT; dy++) {
    for (int dx = 0; dx < AD_RECT; dx++) {
      int y = r->top + dy, x = r->left + dx;
      if (y < 0 || y >= H || x < 0 || x >= W) continue;
      canvas[y][x] = (dy < AD_STRIP || dx >= AD_RECT - AD_STRIP) ? 0 : 0xFFFF;
    }
  }
}

void test_trail_is_wiped(void) {
  AdSim sim;
  memset(canvas, 0, sizeof canvas);
  srandom(1);
  ad_sim_init(&sim, screen, 10, random);
  ad_sim_draw_all(&sim, blit_stub, NULL);
  for (int step = 0; step < 300; step++) ad_sim_step(&sim, blit_stub, NULL);
  int stray = 0;
  for (int y = 0; y < H; y++) {
    for (int x = 0; x < W; x++) {
      if (!canvas[y][x]) continue;
      int inside = 0;
      for (int i = 0; i < sim.n; i++) {
        const AdRect *r = &sim.f[i].r;
        if (sim.f[i].live && y >= r->top && y < r->bottom && x >= r->left && x < r->right)
          inside = 1;
      }
      if (!inside) stray++;
    }
  }
  TEST_ASSERT_EQUAL_INT(0, stray);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_flap_runs_0_1_2_3_2_1_0);
  RUN_TEST(test_speed_from_roll);
  RUN_TEST(test_toast_draws_sprite_4);
  RUN_TEST(test_init_scatter_does_not_draw_and_leaves_running_set);
  RUN_TEST(test_step_moves_speed_times_minus2_plus1);
  RUN_TEST(test_nudge_beside_drops_two_more);
  RUN_TEST(test_nudge_above_goes_left_below_goes_less_left);
  RUN_TEST(test_nudged_move_that_still_collides_keeps_base_step);
  RUN_TEST(test_speed_one_never_checks);
  RUN_TEST(test_collision_reports_nudge_and_skips_self_and_dead_slots);
  RUN_TEST(test_off_screen_flyer_is_removed_and_respawned_at_an_edge);
  RUN_TEST(test_initial_scatter_is_inside_bounds);
  RUN_TEST(test_n_is_clamped_to_the_slot_array);
  RUN_TEST(test_slot_stays_empty_after_four_colliding_tries);
  RUN_TEST(test_trail_is_wiped);
  return UNITY_END();
}
