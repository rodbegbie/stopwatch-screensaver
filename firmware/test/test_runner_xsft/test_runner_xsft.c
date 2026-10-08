#include <stdlib.h>
#include <unity.h>

#include "core/canvas.h"
#include "runner/hack_runner.h"
#include "screenhackI.h"

static Canvas cv;
static int setup_calls;
static int seen_count;
static int seen_delay;
static void *seen_arg;
static int sentinel;
static const char **merged;

static const char *const kPlainDefaults[] = {"*count: 3", NULL};

static void fake_setup(struct xscreensaver_function_table *t, void *arg);
static void *fake_init(Display *dpy, Window w, void *arg);
static unsigned long fake_draw(Display *dpy, Window w, void *closure);
static void fake_free(Display *dpy, Window w, void *closure);

static struct xscreensaver_function_table fake_xsft = {
    .setup_cb = fake_setup,
    .setup_arg = &sentinel,
};

static void fake_setup(struct xscreensaver_function_table *t, void *arg) {
  (void)arg;
  setup_calls++;
  merged = (const char **)malloc(3 * sizeof(*merged));
  merged[0] = "*delay: 1000";
  merged[1] = "*count: 7";
  merged[2] = NULL;
  t->defaults = (const char *const *)merged;
  t->init_cb = (void *(*)(Display *, Window))fake_init;
  t->draw_cb = fake_draw;
  t->free_cb = fake_free;
}

/* xlockmore_init takes the function table as a third argument that the
 * struct's two-argument prototype hides; screenhack.c casts to pass it. */
static void *fake_init(Display *dpy, Window w, void *arg) {
  (void)w;
  seen_arg = arg;
  seen_count = get_integer_resource(dpy, "count", "Int");
  seen_delay = get_integer_resource(dpy, "delay", "Usecs");
  return &fake_xsft;
}

static unsigned long fake_draw(Display *dpy, Window w, void *closure) {
  (void)dpy;
  (void)w;
  (void)closure;
  return 5000;
}

static void fake_free(Display *dpy, Window w, void *closure) {
  (void)dpy;
  (void)w;
  (void)closure;
}

static void *plain_init(Display *dpy, Window w) {
  (void)w;
  seen_count = get_integer_resource(dpy, "count", "Int");
  return NULL;
}

static unsigned long plain_draw(Display *dpy, Window w, void *closure) {
  (void)dpy;
  (void)w;
  (void)closure;
  return 7000;
}

static void plain_free(Display *dpy, Window w, void *closure) {
  (void)dpy;
  (void)w;
  (void)closure;
}

void setUp(void) {
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&cv, 466, 466, malloc));
  setup_calls = 0;
  seen_count = 0;
  seen_delay = 0;
  seen_arg = NULL;
  merged = NULL;
  fake_xsft.init_cb = NULL;
  fake_xsft.draw_cb = NULL;
  fake_xsft.free_cb = NULL;
  fake_xsft.defaults = NULL;
}

void tearDown(void) {
  free(merged);
  canvas_free(&cv);
}

void test_setup_runs_once_across_100_starts(void) {
  static const HackEntry entry = {"Fake", NULL, NULL, NULL, NULL, &fake_xsft};
  const HackEntry *const hacks[] = {&entry};
  HackRunner *r = runner_create_with(&cv, hacks, 1);
  for (int i = 0; i < 100; i++) {
    TEST_ASSERT_EQUAL_INT(0, runner_start(r, 0));
    TEST_ASSERT_EQUAL_UINT32(5000, runner_step(r));
  }
  runner_destroy(r);
  TEST_ASSERT_EQUAL_INT(1, setup_calls);
}

void test_framework_and_hack_defaults_both_resolve(void) {
  static const HackEntry entry = {"Fake", NULL, NULL, NULL, NULL, &fake_xsft};
  const HackEntry *const hacks[] = {&entry};
  HackRunner *r = runner_create_with(&cv, hacks, 1);
  runner_start(r, 0);
  runner_destroy(r);
  TEST_ASSERT_EQUAL_INT(7, seen_count);
  TEST_ASSERT_EQUAL_INT(1000, seen_delay);
}

void test_init_receives_the_tables_setup_arg(void) {
  static const HackEntry entry = {"Fake", NULL, NULL, NULL, NULL, &fake_xsft};
  const HackEntry *const hacks[] = {&entry};
  HackRunner *r = runner_create_with(&cv, hacks, 1);
  runner_start(r, 0);
  runner_destroy(r);
  TEST_ASSERT_EQUAL_PTR(&sentinel, seen_arg);
}

void test_plain_hack_with_null_xsft_still_runs(void) {
  static const HackEntry entry = {"Plain", kPlainDefaults, plain_init,
                                  plain_draw, plain_free, NULL};
  const HackEntry *const hacks[] = {&entry};
  HackRunner *r = runner_create_with(&cv, hacks, 1);
  TEST_ASSERT_EQUAL_INT(0, runner_start(r, 0));
  TEST_ASSERT_EQUAL_UINT32(7000, runner_step(r));
  runner_destroy(r);
  TEST_ASSERT_EQUAL_INT(3, seen_count);
  TEST_ASSERT_EQUAL_INT(0, setup_calls);
}

static const char *const kWhiteBackground[] = {".background: white", NULL};
static const char *const kBlackBackground[] = {".background: black", NULL};

static void *nothing_init(Display *dpy, Window w) {
  (void)dpy;
  (void)w;
  return NULL;
}

static long count_colour(uint16_t colour) {
  long n = 0;
  for (long i = 0; i < (long)cv.w * cv.h; i++) n += cv.px[i] == colour;
  return n;
}

/* screenhack.c paints the window in the hack's background before init runs. */
void test_runner_paints_the_hacks_background_before_init(void) {
  static const HackEntry white = {"White", kWhiteBackground, nothing_init,
                                  plain_draw, plain_free, NULL};
  const HackEntry *const hacks[] = {&white};
  HackRunner *r = runner_create_with(&cv, hacks, 1);
  TEST_ASSERT_EQUAL_INT(0, runner_start(r, 0));
  TEST_ASSERT_EQUAL_INT64((long)cv.w * cv.h, count_colour(0xFFFF));
  runner_destroy(r);
}

void test_a_black_or_absent_background_clears_the_canvas_to_black(void) {
  static const HackEntry black = {"Black", kBlackBackground, nothing_init,
                                  plain_draw, plain_free, NULL};
  static const HackEntry none = {"None", kPlainDefaults, nothing_init,
                                 plain_draw, plain_free, NULL};
  const HackEntry *const hacks[] = {&black, &none};
  HackRunner *r = runner_create_with(&cv, hacks, 2);
  for (int i = 0; i < 2; i++) {
    for (long p = 0; p < (long)cv.w * cv.h; p++) cv.px[p] = 0x1234;
    TEST_ASSERT_EQUAL_INT(0, runner_start(r, i));
    TEST_ASSERT_EQUAL_INT64((long)cv.w * cv.h, count_colour(0));
  }
  runner_destroy(r);
}

void test_a_hack_does_not_inherit_the_previous_hacks_background(void) {
  static const HackEntry white = {"White", kWhiteBackground, nothing_init,
                                  plain_draw, plain_free, NULL};
  static const HackEntry none = {"None", kPlainDefaults, nothing_init,
                                 plain_draw, plain_free, NULL};
  const HackEntry *const hacks[] = {&white, &none};
  HackRunner *r = runner_create_with(&cv, hacks, 2);
  TEST_ASSERT_EQUAL_INT(0, runner_start(r, 0));
  TEST_ASSERT_EQUAL_INT(0, runner_start(r, 1));
  TEST_ASSERT_EQUAL_INT64((long)cv.w * cv.h, count_colour(0));
  runner_destroy(r);
}

void test_entry_overrides_apply_to_its_hack_only(void) {
  static const char *const over[] = {"*count: 2", NULL};
  static const HackEntry with = {"With", NULL, NULL, NULL, NULL, &fake_xsft,
                                 over};
  static const HackEntry without = {"Without", NULL, NULL, NULL, NULL,
                                    &fake_xsft};
  const HackEntry *const hacks[] = {&with, &without};
  HackRunner *r = runner_create_with(&cv, hacks, 2);
  runner_start(r, 0);
  TEST_ASSERT_EQUAL_INT(2, seen_count);
  runner_start(r, 1);
  TEST_ASSERT_EQUAL_INT(7, seen_count);
  runner_destroy(r);
}

void test_screenhackI_h_alone_provides_the_random_macros(void) {
  ya_rand_init(7);
  TEST_ASSERT_TRUE(NRAND(3) < 3);
  TEST_ASSERT_TRUE((double)LRAND() < MAXRAND);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_setup_runs_once_across_100_starts);
  RUN_TEST(test_framework_and_hack_defaults_both_resolve);
  RUN_TEST(test_init_receives_the_tables_setup_arg);
  RUN_TEST(test_plain_hack_with_null_xsft_still_runs);
  RUN_TEST(test_runner_paints_the_hacks_background_before_init);
  RUN_TEST(test_a_black_or_absent_background_clears_the_canvas_to_black);
  RUN_TEST(test_a_hack_does_not_inherit_the_previous_hacks_background);
  RUN_TEST(test_entry_overrides_apply_to_its_hack_only);
  RUN_TEST(test_screenhackI_h_alone_provides_the_random_macros);
  return UNITY_END();
}
