/* Builds the unmodified loop.c and makes it pick squares or hexagons again at
 * every start. Upstream chooses `local_neighbors` only `if (!local_neighbors)`
 * and never clears it, which is fine when each run is a process but means the
 * first start after a boot fixes the mode until the next one (on the device
 * every boot looked the same).
 *
 * Its transition table is built once, for that mode (8^4 entries for squares,
 * 8^6, about 1 MB, for hexagons), so it is freed with each start and each stop:
 * a table left over from the other mode would be read past its end, and the
 * memory in use at rest would depend on which mode ran last.
 *
 * The static function table is rewritten before main, so the framework starts
 * and stops the hack through these wrappers. */
#include "loop/loop.c"

static void forget_the_table(void) {
  free(table);
  table = NULL;
}

static void init_loop_fresh(ModeInfo *mi) {
  forget_the_table();
  local_neighbors = 0;
  init_loop(mi);
}

static void free_loop_fresh(ModeInfo *mi) {
  free_loop(mi);
  forget_the_table();
}

__attribute__((constructor)) static void loop_starts_fresh(void) {
  loop_xlockmore_function_table.hack_init = init_loop_fresh;
  loop_xlockmore_function_table.hack_free = free_loop_fresh;
}

/* For the tests: 4 for squares, 6 for hexagons, 0 before the first start. */
int loop_neighbors_in_use(void) { return local_neighbors; }
