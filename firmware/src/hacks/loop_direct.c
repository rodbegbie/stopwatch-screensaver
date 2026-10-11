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
#include <stdlib.h>

#include "screenhack.h"
#include "xlockmore.h"

/* malloc(0) returns NULL on the board (and a pointer on the host). Square mode
 * asks for a list of rectangles for each of its eight colour states, most of
 * them empty at any moment, and takes NULL for "out of memory": it freed its
 * cells and drew nothing more, a black screen. Ask for a byte instead.
 *
 * The allocator is a pointer so a test can swap in one that behaves like the
 * board's. */
void *(*loop_allocator)(size_t) = malloc;

static void *loop_malloc(size_t n) { return loop_allocator(n ? n : 1); }

#define malloc loop_malloc
#include "loop/loop.c"
#undef malloc

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
