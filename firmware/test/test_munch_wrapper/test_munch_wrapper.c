#include <sanitizer/allocator_interface.h>
#include <stdlib.h>
#include <unity.h>

#include "core/canvas.h"
#include "x11shim/xshim.h"

/* Includes the device wrapper itself. The renamed entry keeps it from
 * clashing with the copy in the registry. */
#define munch_hack munch_hack_under_test
#include "hacks/munch_safe.c"

static Canvas cv;
static Display *dpy;

void setUp(void) {
  TEST_ASSERT_EQUAL_INT(0, canvas_init(&cv, 64, 64, malloc));
  dpy = xshim_open_display(&cv);
  xshim_set_defaults((const char *const *)munch_defaults);
}
void tearDown(void) {
  xshim_set_defaults(NULL);
  xshim_close_display(dpy);
  canvas_free(&cv);
}

static void restart_once(struct state *st) {
  st->restart = 1;
  for (int i = 0; i <= st->simul && st->restart; i++) munch_draw(dpy, 1, st);
  TEST_ASSERT_EQUAL_INT(0, st->restart);
}

/* Each restart inside the hack asks for the "mismunch" resource and never
 * frees the string. */
void test_restarts_inside_the_hack_do_not_leak_the_mismunch_string(void) {
  struct state *st = munch_init(dpy, 1);
  restart_once(st);
  const size_t before = __sanitizer_get_current_allocated_bytes();
  for (int i = 0; i < 50; i++) restart_once(st);
  const size_t after = __sanitizer_get_current_allocated_bytes();
  TEST_ASSERT_TRUE(after <= before + 64);
  munch_free(dpy, 1, st);
}

/* The string init asks for is freed by init itself: the wrapper must not
 * free it a second time or leave a dangling pointer for the next restart. */
void test_start_and_stop_leave_nothing_behind(void) {
  const size_t before = __sanitizer_get_current_allocated_bytes();
  for (int i = 0; i < 20; i++) {
    struct state *st = munch_init(dpy, 1);
    restart_once(st);
    munch_free(dpy, 1, st);
  }
  TEST_ASSERT_TRUE(__sanitizer_get_current_allocated_bytes() <= before + 64);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_restarts_inside_the_hack_do_not_leak_the_mismunch_string);
  RUN_TEST(test_start_and_stop_leave_nothing_behind);
  return UNITY_END();
}
