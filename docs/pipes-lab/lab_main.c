/* Runs hack 0 for <frames> frames and prints, per frame, the vertices TinyGL
 * set up, the lit ones, filled pixels, the canvas rows the swap marked, and a
 * hash of the canvas. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <malloc/malloc.h>

#include "core/canvas.h"
#include "glshim/glshim.h"
#include "hacks/registry.h"
#include "runner/hack_runner.h"

double gl_budget_counts[7];
static int box_y0, box_y1;

static void on_box(int x0, int y0, int x1, int y1) {
  (void)x0;
  (void)x1;
  if (y0 < box_y0) box_y0 = y0;
  if (y1 > box_y1) box_y1 = y1;
}

static uint64_t hash(const Canvas *cv) {
  uint64_t h = 1469598103934665603ull;
  for (long i = 0; i < (long)cv->w * cv->h; i++) h = (h ^ cv->px[i]) * 1099511628211ull;
  return h;
}

int main(int argc, char **argv) {
  const int frames = atoi(argv[1]);
  srandom((unsigned)atoi(argv[2]));
  Canvas cv;
  if (canvas_init(&cv, 466, 466, malloc) != 0) return 1;
  HackRunner *r = runner_create(&cv);
  if (!r || runner_start(r, 0) != 0) return 1;
  glshim_box_observer = on_box;
  for (int f = 0; f < frames; f++) {
    for (int i = 0; i < 7; i++) gl_budget_counts[i] = 0;
    box_y0 = 1 << 30;
    box_y1 = -1;
    runner_step(r);
    printf("%d vertices=%.0f lit=%.0f light_terms=%.0f pixels=%.0f rows=%d hash=%016llx\n", f,
           gl_budget_counts[0], gl_budget_counts[1], gl_budget_counts[6], gl_budget_counts[5],
           box_y1 >= box_y0 ? box_y1 - box_y0 + 1 : 0, (unsigned long long)hash(&cv));
    if (getenv("HEAP")) { malloc_statistics_t st; malloc_zone_statistics(NULL, &st); printf("heap_in_use=%zu\n", st.size_in_use); }
    fflush(stdout);
  }
  runner_destroy(r);
  canvas_free(&cv);
  return 0;
}
