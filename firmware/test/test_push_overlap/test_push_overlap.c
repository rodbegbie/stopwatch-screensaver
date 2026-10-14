#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include "hacks/registry.h"

void setUp(void) {}
void tearDown(void) {}

void test_no_hack_asks_for_both_repaints_all_and_sync_to_refresh(void) {
  for (int i = 0; i < g_hack_count; i++) {
    const HackEntry *e = g_hacks[i];
    TEST_ASSERT_FALSE_MESSAGE(e->repaints_all && e->sync_to_refresh, e->name);
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_no_hack_asks_for_both_repaints_all_and_sync_to_refresh);
  return UNITY_END();
}
